/*
 * Copyright (C) 2018-2019 Codership Oy <info@codership.com>
 *
 * This file is part of wsrep-lib.
 *
 * Wsrep-lib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wsrep-lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wsrep-lib.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "wsrep/server_state.hpp"
#include "wsrep/client_state.hpp"
#include "wsrep/server_service.hpp"
#include "wsrep/high_priority_service.hpp"
#include "wsrep/transaction.hpp"
#include "wsrep/view.hpp"
#include "wsrep/logger.hpp"
#include "wsrep/compiler.hpp"
#include "wsrep/id.hpp"

#include <cassert>
#include <sstream>
#include <algorithm>


//////////////////////////////////////////////////////////////////////////////
//                               Helpers                                    //
//////////////////////////////////////////////////////////////////////////////


//
// This method is used to deal with historical burden of several
// ways to bootstrap the cluster. Bootstrap happens if
//
// * bootstrap option is given
// * cluster_address is "gcomm://" (Galera provider)
//
static bool is_bootstrap(const std::string& cluster_address, bool bootstrap)
{
    return (bootstrap || cluster_address == "gcomm://");
}

// Helper method to provide detailed error message if transaction
// adopt for fragment removal fails.
static void log_adopt_error(const wsrep::transaction& transaction)
{
    wsrep::log_warning() << "Adopting a transaction ("
                         << transaction.server_id() << "," << transaction.id()
                         << ") for rollback failed, "
                         << "this may leave stale entries to streaming log "
                         << "which may need to be removed manually.";
}

// resolve which of the two errors return to caller
static inline int resolve_return_error(bool const vote,
                                       int  const vote_err,
                                       int  const apply_err)
{
    if (vote) return vote_err;
    return vote_err != 0 ? vote_err : apply_err;
}

static void
discard_streaming_applier(wsrep::server_state& server_state,
                          wsrep::high_priority_service& high_priority_service,
                          wsrep::high_priority_service* streaming_applier,
                          const wsrep::ws_meta& ws_meta)
{
    server_state.stop_streaming_applier(
        ws_meta.server_id(), ws_meta.transaction_id());
    server_state.server_service().release_high_priority_service(
        streaming_applier);
    high_priority_service.store_globals();
}

static int apply_fragment(wsrep::server_state& server_state,
                          wsrep::high_priority_service& high_priority_service,
                          wsrep::high_priority_service* streaming_applier,
                          const wsrep::ws_handle& ws_handle,
                          const wsrep::ws_meta& ws_meta,
                          const wsrep::const_buffer& data)
{
    int ret(0);
    int apply_err;
    wsrep::mutable_buffer err;
    {
        wsrep::high_priority_switch sw(high_priority_service,
                                       *streaming_applier);
        apply_err = streaming_applier->apply_write_set(ws_meta, data, err);
        if (!apply_err)
        {
            assert(err.size() == 0);
            streaming_applier->after_apply();
        }
        else
        {
            bool const remove_fragments(streaming_applier->transaction(
                ).streaming_context().fragments().size() > 0);
            ret = streaming_applier->rollback(ws_handle, ws_meta);
            ret = ret || (streaming_applier->after_apply(), 0);

            if (remove_fragments)
            {
                ret = ret || streaming_applier->start_transaction(ws_handle,
                                                                  ws_meta);
                ret = ret || (streaming_applier->adopt_apply_error(err), 0);
                ret = ret || streaming_applier->remove_fragments(ws_meta);
                ret = ret || streaming_applier->commit(ws_handle, ws_meta);
                ret = ret || (streaming_applier->after_apply(), 0);
            }
            else
            {
                ret = streaming_applier->log_dummy_write_set(ws_handle,
                                                             ws_meta, err);
            }
        }
    }

    if (!ret)
    {
        if (!apply_err)
        {
            high_priority_service.debug_crash("crash_apply_cb_before_append_frag");
            const wsrep::xid xid(streaming_applier->transaction().xid());
            ret = high_priority_service.append_fragment_and_commit(
                ws_handle, ws_meta, data, xid);
            high_priority_service.debug_crash("crash_apply_cb_after_append_frag");
            ret = ret || (high_priority_service.after_apply(), 0);
        }
        else
        {
            discard_streaming_applier(server_state,
                                      high_priority_service,
                                      streaming_applier,
                                      ws_meta);
            ret = resolve_return_error(err.size() > 0, ret, apply_err);
        }
    }

    return ret;
}

static int commit_fragment(wsrep::server_state& server_state,
                           wsrep::high_priority_service& high_priority_service,
                           wsrep::high_priority_service* streaming_applier,
                           const wsrep::ws_handle& ws_handle,
                           const wsrep::ws_meta& ws_meta,
                           const wsrep::const_buffer& data)
{
    int ret(0);
    {
        wsrep::high_priority_switch sw(
            high_priority_service, *streaming_applier);
        wsrep::mutable_buffer err;
        int const apply_err(
            streaming_applier->apply_write_set(ws_meta, data, err));
        if (apply_err)
        {
            assert(streaming_applier->transaction(
                ).streaming_context().fragments().size() > 0);
            ret = streaming_applier->rollback(ws_handle, ws_meta);
            ret = ret || (streaming_applier->after_apply(), 0);
            ret = ret || streaming_applier->start_transaction(
                ws_handle, ws_meta);
            ret = ret || (streaming_applier->adopt_apply_error(err),0);
        }
        else
        {
            assert(err.size() == 0);
        }

        const wsrep::transaction& trx(streaming_applier->transaction());
        // Fragment removal for XA is going to happen in after_commit
        if (trx.state() != wsrep::transaction::s_prepared)
        {
            streaming_applier->debug_crash(
                "crash_apply_cb_before_fragment_removal");

            ret = ret || streaming_applier->remove_fragments(ws_meta);

            streaming_applier->debug_crash(
                "crash_apply_cb_after_fragment_removal");
        }

        streaming_applier->debug_crash(
            "crash_commit_cb_before_last_fragment_commit");
        ret = ret || streaming_applier->commit(ws_handle, ws_meta);
        streaming_applier->debug_crash(
            "crash_commit_cb_last_fragment_commit_success");
        ret = ret || (streaming_applier->after_apply(), 0);
        ret = resolve_return_error(err.size() > 0, ret, apply_err);
    }

    if (!ret)
    {
        discard_streaming_applier(server_state, high_priority_service,
                                  streaming_applier, ws_meta);
    }

    return ret;
}

static int rollback_fragment(wsrep::server_state& server_state,
                             wsrep::high_priority_service& high_priority_service,
                             wsrep::high_priority_service* streaming_applier,
                             const wsrep::ws_handle& ws_handle,
                             const wsrep::ws_meta& ws_meta)
{
    int ret(0);
    int adopt_error(0);
    bool const remove_fragments(streaming_applier->transaction().
                                streaming_context().fragments().size() > 0);
    // If fragment removal is needed, adopt transaction state
    // and start a transaction for it.
    if (remove_fragments &&
        (adopt_error = high_priority_service.adopt_transaction(
          streaming_applier->transaction())))
    {
        log_adopt_error(streaming_applier->transaction());
    }
    // Even if the adopt above fails we roll back the streaming transaction.
    // Adopt failure will leave stale entries in streaming log which can
    // be removed manually.
    wsrep::const_buffer no_error;
    {
        wsrep::high_priority_switch ws(
            high_priority_service, *streaming_applier);
        // Streaming applier rolls back out of order. Fragment
        // removal grabs commit order below.
        ret = streaming_applier->rollback(wsrep::ws_handle(), wsrep::ws_meta());
        ret = ret || (streaming_applier->after_apply(), 0);
    }

    if (!ret)
    {
        discard_streaming_applier(server_state, high_priority_service,
                                  streaming_applier, ws_meta);

        if (adopt_error == 0)
        {
            if (remove_fragments)
            {
                ret = high_priority_service.remove_fragments(ws_meta);
                ret = ret || high_priority_service.commit(ws_handle, ws_meta);
                ret = ret || (high_priority_service.after_apply(), 0);
            }
            else
            {
                if (ws_meta.ordered())
                {
                    wsrep::mutable_buffer no_error;
                    ret = high_priority_service.log_dummy_write_set(
                        ws_handle, ws_meta, no_error);
                }
            }
        }
    }
    return ret;
}

static int apply_write_set(wsrep::server_state& server_state,
                           wsrep::high_priority_service& high_priority_service,
                           const wsrep::ws_handle& ws_handle,
                           const wsrep::ws_meta& ws_meta,
                           const wsrep::const_buffer& data)
{
    int ret(0);
    if (wsrep::rolls_back_transaction(ws_meta.flags()))
    {
        wsrep::mutable_buffer no_error;
        if (wsrep::starts_transaction(ws_meta.flags()))
        {
            // No transaction existed before, log a dummy write set
            ret = high_priority_service.log_dummy_write_set(
                ws_handle, ws_meta, no_error);
        }
        else
        {
            wsrep::high_priority_service* sa(
                server_state.find_streaming_applier(
                    ws_meta.server_id(), ws_meta.transaction_id()));
            if (sa == 0)
            {
                // It is a known limitation that galera provider
                // cannot always determine if certification test
                // for interrupted transaction will pass or fail
                // (see comments in transaction::certify_fragment()).
                // As a consequence, unnecessary rollback fragments
                // may be delivered here. The message below has
                // been intentionally turned into a debug message,
                // rather than warning.
                 WSREP_LOG_DEBUG(wsrep::log::debug_log_level(),
                                 wsrep::log::debug_level_server_state,
                                 "Could not find applier context for "
                                 << ws_meta.server_id()
                                 << ": " << ws_meta.transaction_id());
                ret = high_priority_service.log_dummy_write_set(
                    ws_handle, ws_meta, no_error);
            }
            else
            {
                // rollback_fragment() consumes sa
                ret = rollback_fragment(server_state,
                                        high_priority_service,
                                        sa,
                                        ws_handle,
                                        ws_meta);
            }
        }
    }
    else if (wsrep::starts_transaction(ws_meta.flags()) &&
             wsrep::commits_transaction(ws_meta.flags()))
    {
        ret = high_priority_service.start_transaction(ws_handle, ws_meta);
        if (!ret)
        {
            wsrep::mutable_buffer err;
            int const apply_err(high_priority_service.apply_write_set(
                ws_meta, data, err));
            if (!apply_err)
            {
                assert(err.size() == 0);
                ret = high_priority_service.commit(ws_handle, ws_meta);
                ret = ret || (high_priority_service.after_apply(), 0);
            }
            else
            {
                ret = high_priority_service.rollback(ws_handle, ws_meta);
                ret = ret || (high_priority_service.after_apply(), 0);
                ret = ret || high_priority_service.log_dummy_write_set(
                    ws_handle, ws_meta, err);
                ret = resolve_return_error(err.size() > 0, ret, apply_err);
            }
        }
    }
    else if (wsrep::starts_transaction(ws_meta.flags()))
    {
        assert(server_state.find_streaming_applier(
                   ws_meta.server_id(), ws_meta.transaction_id()) == 0);
        wsrep::high_priority_service* sa(
            server_state.server_service().streaming_applier_service(
                high_priority_service));
        server_state.start_streaming_applier(
            ws_meta.server_id(), ws_meta.transaction_id(), sa);
        sa->start_transaction(ws_handle, ws_meta);
        ret = apply_fragment(server_state,
                             high_priority_service,
                             sa,
                             ws_handle,
                             ws_meta,
                             data);
    }
    else if (ws_meta.flags() == 0 || wsrep::prepares_transaction(ws_meta.flags()))
    {
        wsrep::high_priority_service* sa(
            server_state.find_streaming_applier(
                ws_meta.server_id(), ws_meta.transaction_id()));
        if (sa == 0)
        {
            // It is possible that rapid group membership changes
            // may cause streaming transaction be rolled back before
            // commit fragment comes in. Although this is a valid
            // situation, log a warning if a sac cannot be found as
            // it may be an indication of  a bug too.
            wsrep::log_warning() << "Could not find applier context for "
                                 << ws_meta.server_id()
                                 << ": " << ws_meta.transaction_id();
            wsrep::mutable_buffer no_error;
            ret = high_priority_service.log_dummy_write_set(
                ws_handle, ws_meta, no_error);
        }
        else
        {
            sa->next_fragment(ws_meta);
            ret = apply_fragment(server_state,
                                 high_priority_service,
                                 sa,
                                 ws_handle,
                                 ws_meta,
                                 data);
        }
    }
    else if (wsrep::commits_transaction(ws_meta.flags()))
    {
        if (high_priority_service.is_replaying())
        {
            wsrep::mutable_buffer unused;
            ret = high_priority_service.start_transaction(
                ws_handle, ws_meta) ||
                high_priority_service.apply_write_set(ws_meta, data, unused) ||
                high_priority_service.commit(ws_handle, ws_meta);
        }
        else
        {
            wsrep::high_priority_service* sa(
                server_state.find_streaming_applier(
                    ws_meta.server_id(), ws_meta.transaction_id()));
            if (sa == 0)
            {
                // It is possible that rapid group membership changes
                // may cause streaming transaction be rolled back before
                // commit fragment comes in. Although this is a valid
                // situation, log a warning if a sac cannot be found as
                // it may be an indication of  a bug too.
                wsrep::log_warning()
                    << "Could not find applier context for "
                    << ws_meta.server_id()
                    << ": " << ws_meta.transaction_id();
                wsrep::mutable_buffer no_error;
                ret = high_priority_service.log_dummy_write_set(
                    ws_handle, ws_meta, no_error);
            }
            else
            {
                // Commit fragment consumes sa
                sa->next_fragment(ws_meta);
                ret = commit_fragment(server_state,
                                      high_priority_service,
                                      sa,
                                      ws_handle,
                                      ws_meta,
                                      data);
            }
        }
    }
    else
    {
        assert(0);
    }
    if (ret)
    {
        wsrep::log_error() << "Failed to apply write set: " << ws_meta;
    }
    return ret;
}

static int apply_toi(wsrep::provider& provider,
                     wsrep::high_priority_service& high_priority_service,
                     const wsrep::ws_handle& ws_handle,
                     const wsrep::ws_meta& ws_meta,
                     const wsrep::const_buffer& data)
{
    if (wsrep::starts_transaction(ws_meta.flags()) &&
        wsrep::commits_transaction(ws_meta.flags()))
    {
        //
        // Regular TOI.
        //
        provider.commit_order_enter(ws_handle, ws_meta);
        wsrep::mutable_buffer err;
        int const apply_err(high_priority_service.apply_toi(ws_meta,data,err));
        int const vote_err(provider.commit_order_leave(ws_handle, ws_meta,err));
        return resolve_return_error(err.size() > 0, vote_err, apply_err);
    }
    else if (wsrep::starts_transaction(ws_meta.flags()))
    {
        provider.commit_order_enter(ws_handle, ws_meta);
        wsrep::mutable_buffer err;
        int const apply_err(high_priority_service.apply_nbo_begin(ws_meta, data, err));
        int const vote_err(provider.commit_order_leave(ws_handle, ws_meta, err));
        return resolve_return_error(err.size() > 0, vote_err, apply_err);
    }
    else if (wsrep::commits_transaction(ws_meta.flags()))
    {
        // NBO end event is ignored here, both local and applied
        // have NBO end handled via local TOI calls.
        provider.commit_order_enter(ws_handle, ws_meta);
        wsrep::mutable_buffer err;
        provider.commit_order_leave(ws_handle, ws_meta, err);
        return 0;
    }
    else
    {
        assert(0);
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////////
//                            Server State                                  //
//////////////////////////////////////////////////////////////////////////////

int wsrep::server_state::load_provider(
    const std::string& provider_spec, const std::string& provider_options,
    const wsrep::provider::services& services)
{
    wsrep::log_info() << "Loading provider " << provider_spec
                      << " initial position: " << initial_position_;

    provider_ = wsrep::provider::make_provider(*this,
                                               provider_spec,
                                               provider_options,
                                               services);
    return (provider_ ? 0 : 1);
}

void wsrep::server_state::unload_provider()
{
    delete provider_;
    provider_ = 0;
}

int wsrep::server_state::connect(const std::string& cluster_name,
                                   const std::string& cluster_address,
                                   const std::string& state_donor,
                                   bool bootstrap)
{
    bootstrap_ = is_bootstrap(cluster_address, bootstrap);
    wsrep::log_info() << "Connecting with bootstrap option: " << bootstrap_;
    return provider().connect(cluster_name, cluster_address, state_donor,
                              bootstrap_);
}

int wsrep::server_state::disconnect()
{
    {
        wsrep::unique_lock<wsrep::mutex> lock(mutex_);
        // In case of failure situations which are caused by provider
        // being shut down some failing operation may also try to shut
        // down the replication. Check the state here and
        // return success if the provider disconnect is already in progress
        // or has completed.
        if (state(lock) == s_disconnecting || state(lock) == s_disconnected)
        {
            return 0;
        }
        state(lock, s_disconnecting);
        interrupt_state_waiters(lock);
    }
    return provider().disconnect();
}

wsrep::server_state::~server_state()
{
    delete provider_;
}

std::vector<wsrep::provider::status_variable>
wsrep::server_state::status() const
{
    return provider().status();
}


wsrep::seqno wsrep::server_state::pause()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    // Disallow concurrent calls to pause to in order to have non-concurrent
    // access to desynced_on_pause_ which is checked in resume() call.
    wsrep::log_info() << "pause";
    while (pause_count_ > 0)
    {
        cond_.wait(lock);
    }
    ++pause_count_;
    assert(pause_seqno_.is_undefined());
    lock.unlock();
    pause_seqno_ = provider().pause();
    lock.lock();
    if (pause_seqno_.is_undefined())
    {
        --pause_count_;
    }
    return pause_seqno_;
}

void wsrep::server_state::resume()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    wsrep::log_info() << "resume";
    assert(pause_seqno_.is_undefined() == false);
    assert(pause_count_ == 1);
    if (provider().resume())
    {
        throw wsrep::runtime_error("Failed to resume provider");
    }
    pause_seqno_ = wsrep::seqno::undefined();
    --pause_count_;
    cond_.notify_all();
}

wsrep::seqno wsrep::server_state::desync_and_pause()
{
    wsrep::log_info() << "Desyncing and pausing the provider";
    // Temporary variable to store desync() return status. This will be
    // assigned to desynced_on_pause_ after pause() call to prevent
    // concurrent access to  member variable desynced_on_pause_.
    bool desync_successful;
    if (desync())
    {
        // Desync may give transient error if the provider cannot
        // communicate with the rest of the cluster. However, this
        // error can be tolerated because if the provider can be
        // paused succesfully below.
        WSREP_LOG_DEBUG(wsrep::log::debug_log_level(),
                        wsrep::log::debug_level_server_state,
                        "Failed to desync server before pause");
        desync_successful = false;
    }
    else
    {
        desync_successful = true;
    }
    wsrep::seqno ret(pause());
    if (ret.is_undefined())
    {
        wsrep::log_warning() << "Failed to pause provider";
        resync();
        return wsrep::seqno::undefined();
    }
    else
    {
        desynced_on_pause_ = desync_successful;
    }
    wsrep::log_info() << "Provider paused at: " << ret;
    return ret;
}

void wsrep::server_state::resume_and_resync()
{
    wsrep::log_info() << "Resuming and resyncing the provider";
    try
    {
        // Assign desynced_on_pause_ to local variable before resuming
        // in order to avoid concurrent access to desynced_on_pause_ member
        // variable.
        bool do_resync = desynced_on_pause_;
        desynced_on_pause_ = false;
        resume();
        if (do_resync)
        {
            resync();
        }
    }
    catch (const wsrep::runtime_error& e)
    {
        wsrep::log_warning()
            << "Resume and resync failed, server may have to be restarted";
    }
}

std::string wsrep::server_state::prepare_for_sst()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    state(lock, s_joiner);
    lock.unlock();
    return server_service_.sst_request();
}

int wsrep::server_state::start_sst(const std::string& sst_request,
                                   const wsrep::gtid& gtid,
                                   bool bypass)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    state(lock, s_donor);
    int ret(0);
    lock.unlock();
    if (server_service_.start_sst(sst_request, gtid, bypass))
    {
        lock.lock();
        wsrep::log_warning() << "SST start failed";
        state(lock, s_synced);
        ret = 1;
    }
    return ret;
}

void wsrep::server_state::sst_sent(const wsrep::gtid& gtid, int error)
{
    if (0 == error)
        wsrep::log_info() << "SST sent: " << gtid;
    else
        wsrep::log_info() << "SST sending failed: " << error;

    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    state(lock, s_joined);
    lock.unlock();
    enum provider::status const retval(provider().sst_sent(gtid, error));
    if (retval != provider::success)
    {
        std::string msg("wsrep::sst_sent() returned an error: ");
        msg += wsrep::provider::to_string(retval);
        server_service_.log_message(wsrep::log::warning, msg.c_str());
    }
}

void wsrep::server_state::sst_received(wsrep::client_service& cs,
                                       int const error)
{
    wsrep::log_info() << "SST received";
    wsrep::gtid gtid(wsrep::gtid::undefined());
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    assert(state_ == s_joiner || state_ == s_initialized);

    // Run initialization only if the SST was successful.
    // In case of SST failure the system is in undefined state
    // may not be recoverable.
    if (error == 0)
    {
        if (server_service_.sst_before_init())
        {
            if (init_initialized_ == false)
            {
                state(lock, s_initializing);
                lock.unlock();
                server_service_.debug_sync("on_view_wait_initialized");
                lock.lock();
                wait_until_state(lock, s_initialized);
                assert(init_initialized_);
            }
        }
        state(lock, s_joined);
        lock.unlock();

        if (id_.is_undefined())
        {
            assert(0);
            throw wsrep::runtime_error(
                "wsrep::sst_received() called before connection to cluster");
        }

        gtid = server_service_.get_position(cs);
        wsrep::log_info() << "Recovered position from storage: " << gtid;
        wsrep::view const v(server_service_.get_view(cs, id_));
        wsrep::log_info() << "Recovered view from SST:\n" << v;

        /*
         * If the state id from recovered view has undefined ID, we may
         * be upgrading from earlier version which does not provide
         * view stored in stable storage. In this case we skip
         * sanity checks and assigning the current view and wait
         * until the first view delivery.
         */
        if (v.state_id().id().is_undefined() == false)
        {
            if (v.state_id().id() != gtid.id() ||
                v.state_id().seqno() > gtid.seqno())
            {
                /* Since IN GENERAL we may not be able to recover SST GTID from
                 * the state data, we have to rely on SST script passing the
                 * GTID value explicitly.
                 * Here we check if the passed GTID makes any sense: it should
                 * have the same UUID and greater or equal seqno than the last
                 * logged view. */
                std::ostringstream msg;
                msg << "SST script passed bogus GTID: " << gtid
                    << ". Preceeding view GTID: " << v.state_id();
                throw wsrep::runtime_error(msg.str());
            }

            if (current_view_.status() == wsrep::view::primary)
            {
                previous_primary_view_ = current_view_;
            }
            current_view_ = v;
            server_service_.log_view(NULL /* this view is stored already */, v);
        }
        else
        {
            wsrep::log_warning()
                << "View recovered from stable storage was empty. If the "
                << "server is doing rolling upgrade from previous version "
                << "which does not support storing view info into stable "
                << "storage, this is ok. Otherwise this may be a sign of "
                << "malfunction.";
        }
        lock.lock();
        recover_streaming_appliers_if_not_recovered(lock, cs);
        lock.unlock();
    }

    enum provider::status const retval(provider().sst_received(gtid, error));
    if (retval != provider::success)
    {
        std::string msg("wsrep::sst_received() failed: ");
        msg += wsrep::provider::to_string(retval);
        throw wsrep::runtime_error(msg);
    }
}

void wsrep::server_state::initialized()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    wsrep::log_info() << "Server initialized";
    init_initialized_ = true;
    if (server_service_.sst_before_init())
    {
        state(lock, s_initialized);
    }
    else
    {
        state(lock, s_initializing);
        state(lock, s_initialized);
    }
}

void wsrep::server_state::last_committed_gtid(const wsrep::gtid& gtid)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    assert(last_committed_gtid_.is_undefined() ||
           last_committed_gtid_.seqno() + 1 == gtid.seqno());
    last_committed_gtid_ = gtid;
    cond_.notify_all();
}

wsrep::gtid wsrep::server_state::last_committed_gtid() const
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    return last_committed_gtid_;
}

enum wsrep::provider::status
wsrep::server_state::wait_for_gtid(const wsrep::gtid& gtid, int timeout)
    const
{
    return provider().wait_for_gtid(gtid, timeout);
}

int 
wsrep::server_state::set_encryption_key(std::vector<unsigned char>& key)
{
    encryption_key_ = key;
    if (state_ != s_disconnected)
    {
        wsrep::const_buffer const key(encryption_key_.data(),
                                      encryption_key_.size());
        enum provider::status const retval(provider_->enc_set_key(key));
        if (retval != provider::success)
        {
            wsrep::log_error() << "Failed to set encryption key: "
                               << provider::to_string(retval);
            return 1;
        }
    }
    return 0;
}

std::pair<wsrep::gtid, enum wsrep::provider::status>
wsrep::server_state::causal_read(int timeout) const
{
    return provider().causal_read(timeout);
}

void wsrep::server_state::on_connect(const wsrep::view& view)
{
    // Sanity checks
    if (view.own_index() < 0 ||
        size_t(view.own_index()) >= view.members().size())
    {
        std::ostringstream os;
        os << "Invalid view on connect: own index out of range: " << view;
#ifndef NDEBUG
        wsrep::log_error() << os.str();
        assert(0);
#endif
        throw wsrep::runtime_error(os.str());
    }

    const size_t own_index(static_cast<size_t>(view.own_index()));
    if (id_.is_undefined() == false && id_ != view.members()[own_index].id())
    {
        std::ostringstream os;
        os << "Connection in connected state.\n"
           << "Connected view:\n" << view
           << "Previous view:\n" << current_view_
           << "Current own ID: " << id_;
#ifndef NDEBUG
        wsrep::log_error() << os.str();
        assert(0);
#endif
        throw wsrep::runtime_error(os.str());
    }
    else
    {
        id_ = view.members()[own_index].id();
    }

    wsrep::log_info() << "Server "
                      << name_
                      << " connected to cluster at position "
                      << view.state_id()
                      << " with ID "
                      << id_;

    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    connected_gtid_ = view.state_id();
    state(lock, s_connected);
}

void wsrep::server_state::on_primary_view(
    const wsrep::view& view WSREP_UNUSED,
    wsrep::high_priority_service* high_priority_service)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    assert(view.final() == false);
    //
    // Reached primary from connected state. This may mean the following
    //
    // 1) Server was joined to the cluster and got SST transfer
    // 2) Server was partitioned from the cluster and got back
    // 3) A new cluster was bootstrapped from non-prim cluster
    //
    // There is no enough information here what was the cause
    // of the primary component, so we need to walk through
    // all states leading to joined to notify possible state
    // waiters in other threads.
    //
    if (server_service_.sst_before_init())
    {
        if (state_ == s_connected)
        {
            state(lock, s_joiner);
            // We need to assign init_initialized_ here to local
            // variable. If the value here was false, we need to skip
            // the initializing -> initialized -> joined state cycle
            // below. However, if we don't assign the value to
            // local, it is possible that the main thread gets control
            // between changing the state to initializing and checking
            // initialized flag, which may cause the initialzing -> initialized
            // state change to be executed even if it should not be.
            const bool was_initialized(init_initialized_);
            state(lock, s_initializing);
            if (was_initialized)
            {
                // If server side has already been initialized,
                // skip directly to s_joined.
                state(lock, s_initialized);
                state(lock, s_joined);
            }
        }
        else if (state_ == s_joiner)
        {
            // Got partiioned from the cluster, got IST and
            // started applying actions.
            state(lock, s_joined);
        }
    }
    else
    {
        if (state_ == s_connected)
        {
            state(lock, s_joiner);
        }
        if (init_initialized_ && state_ != s_joined)
        {
            // If server side has already been initialized,
            // skip directly to s_joined.
            state(lock, s_joined);
        }
    }

    if (init_initialized_ == false)
    {
        lock.unlock();
        server_service_.debug_sync("on_view_wait_initialized");
        lock.lock();
        wait_until_state(lock, s_initialized);
    }
    assert(init_initialized_);

    if (bootstrap_)
    {
        server_service_.bootstrap();
        bootstrap_ = false;
    }

    assert(high_priority_service);

    if (high_priority_service)
    {
        recover_streaming_appliers_if_not_recovered(lock,
                                                    *high_priority_service);
        close_orphaned_sr_transactions(lock, *high_priority_service);
    }

    if (server_service_.sst_before_init())
    {
        if (state_ == s_initialized)
        {
            state(lock, s_joined);
            if (init_synced_)
            {
                state(lock, s_synced);
            }
        }
    }
    else
    {
        if (state_ == s_joiner)
        {
            state(lock, s_joined);
            if (init_synced_)
            {
                state(lock, s_synced);
            }
        }
    }
}

void wsrep::server_state::on_non_primary_view(
    const wsrep::view& view,
    wsrep::high_priority_service* high_priority_service)
{
        wsrep::unique_lock<wsrep::mutex> lock(mutex_);
        wsrep::log_info() << "Non-primary view";
        if (view.final())
        {
            go_final(lock, view, high_priority_service);
        }
        else if (state_ != s_disconnecting)
        {
            state(lock, s_connected);
        }
}

void wsrep::server_state::go_final(wsrep::unique_lock<wsrep::mutex>& lock,
                                   const wsrep::view& view,
                                   wsrep::high_priority_service* hps)
{
    (void)view; // avoid compiler warning "unused parameter 'view'"
    assert(view.final());
    assert(hps);
    if (hps)
    {
        close_transactions_at_disconnect(*hps);
    }
    state(lock, s_disconnected);
    id_ = wsrep::id::undefined();
}

void wsrep::server_state::on_view(const wsrep::view& view,
                                  wsrep::high_priority_service* high_priority_service)
{
    wsrep::log_info()
        << "================================================\nView:\n"
        << view
        << "=================================================";
    if (current_view_.status() == wsrep::view::primary)
    {
        previous_primary_view_ = current_view_;
    }
    current_view_ = view;
    switch (view.status())
    {
    case wsrep::view::primary:
        on_primary_view(view, high_priority_service);
        break;
    case wsrep::view::non_primary:
        on_non_primary_view(view, high_priority_service);
        break;
    case wsrep::view::disconnected:
    {
        wsrep::unique_lock<wsrep::mutex> lock(mutex_);
        go_final(lock, view, high_priority_service);
        break;
    }
    default:
        wsrep::log_warning() << "Unrecognized view status: " << view.status();
        assert(0);
    }

    server_service_.log_view(high_priority_service, view);
}

void wsrep::server_state::on_sync()
{
    wsrep::log_info() << "Server " << name_ << " synced with group";
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);

    // Initial sync
    if (server_service_.sst_before_init() && init_synced_ == false)
    {
        switch (state_)
        {
        case s_synced:
            break;
        case s_connected:
            state(lock, s_joiner);
            // fall through
        case s_joiner:
            state(lock, s_initializing);
            break;
        case s_donor:
            state(lock, s_joined);
            state(lock, s_synced);
            break;
        case s_initialized:
            state(lock, s_joined);
            // fall through
        default:
            /* State */
            state(lock, s_synced);
        };
    }
    else
    {
        // Calls to on_sync() in synced state are possible if
        // server desyncs itself from the group. Provider does not
        // inform about this through callbacks.
        if (state_ != s_synced)
        {
            state(lock, s_synced);
        }
    }
    init_synced_ = true;
}

int wsrep::server_state::on_apply(
    wsrep::high_priority_service& high_priority_service,
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta,
    const wsrep::const_buffer& data)
{
    if (is_toi(ws_meta.flags()))
    {
        return apply_toi(provider(), high_priority_service,
                         ws_handle, ws_meta, data);
    }
    else if (is_commutative(ws_meta.flags()) || is_native(ws_meta.flags()))
    {
        // Not implemented yet.
        assert(0);
        return 0;
    }
    else
    {
        return apply_write_set(*this, high_priority_service,
                               ws_handle, ws_meta, data);
    }
}

void wsrep::server_state::start_streaming_client(
    wsrep::client_state* client_state)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    WSREP_LOG_DEBUG(wsrep::log::debug_log_level(),
                    wsrep::log::debug_level_server_state,
                    "Start streaming client: " << client_state->id());
    if (streaming_clients_.insert(
            std::make_pair(client_state->id(), client_state)).second == false)
    {
        wsrep::log_warning() << "Failed to insert streaming client "
                             << client_state->id();
        assert(0);
    }
}

void wsrep::server_state::convert_streaming_client_to_applier(
    wsrep::client_state* client_state)
{
    // create streaming_applier beforehand as server_state lock should
    // not be held when calling server_service methods
    wsrep::high_priority_service* streaming_applier(
        server_service_.streaming_applier_service(
            client_state->client_service()));

    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    WSREP_LOG_DEBUG(wsrep::log::debug_log_level(),
                    wsrep::log::debug_level_server_state,
                    "Convert streaming client to applier "
                    << client_state->id());
    streaming_clients_map::iterator i(
        streaming_clients_.find(client_state->id()));
    assert(i != streaming_clients_.end());
    if (i == streaming_clients_.end())
    {
        wsrep::log_warning() << "Unable to find streaming client "
                             << client_state->id();
        assert(0);
    }
    else
    {
        streaming_clients_.erase(i);
    }

    // Convert to applier only if the state is not disconnected. In
    // disconnected state the applier map is supposed to be empty
    // and it will be reconstructed from fragment storage when
    // joining back to cluster.
    if (state(lock) != s_disconnected)
    {
        if (streaming_applier->adopt_transaction(client_state->transaction()))
        {
            log_adopt_error(client_state->transaction());
            streaming_applier->after_apply();
            server_service_.release_high_priority_service(streaming_applier);
            return;
        }
        if (streaming_appliers_.insert(
                std::make_pair(
                    std::make_pair(client_state->transaction().server_id(),
                                   client_state->transaction().id()),
                    streaming_applier)).second == false)
        {
            wsrep::log_warning() << "Could not insert streaming applier "
                                 << id_
                                 << ", "
                                 << client_state->transaction().id();
            assert(0);
        }
    }
    else
    {
        server_service_.release_high_priority_service(streaming_applier);
        client_state->client_service().store_globals();
    }
}


void wsrep::server_state::stop_streaming_client(
    wsrep::client_state* client_state)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
     WSREP_LOG_DEBUG(wsrep::log::debug_log_level(),
                     wsrep::log::debug_level_server_state,
                     "Stop streaming client: " << client_state->id());
    streaming_clients_map::iterator i(
        streaming_clients_.find(client_state->id()));
    assert(i != streaming_clients_.end());
    if (i == streaming_clients_.end())
    {
        wsrep::log_warning() << "Unable to find streaming client "
                             << client_state->id();
        assert(0);
        return;
    }
    else
    {
        streaming_clients_.erase(i);
        cond_.notify_all();
    }
}

void wsrep::server_state::start_streaming_applier(
    const wsrep::id& server_id,
    const wsrep::transaction_id& transaction_id,
    wsrep::high_priority_service* sa)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    if (streaming_appliers_.insert(
            std::make_pair(std::make_pair(server_id, transaction_id),
                           sa)).second == false)
    {
        wsrep::log_error() << "Could not insert streaming applier";
        throw wsrep::fatal_error();
    }
}

void wsrep::server_state::stop_streaming_applier(
    const wsrep::id& server_id,
    const wsrep::transaction_id& transaction_id)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    streaming_appliers_map::iterator i(
        streaming_appliers_.find(std::make_pair(server_id, transaction_id)));
    assert(i != streaming_appliers_.end());
    if (i == streaming_appliers_.end())
    {
        wsrep::log_warning() << "Could not find streaming applier for "
                             << server_id << ":" << transaction_id;
    }
    else
    {
        streaming_appliers_.erase(i);
        cond_.notify_all();
    }
}

wsrep::high_priority_service* wsrep::server_state::find_streaming_applier(
    const wsrep::id& server_id,
    const wsrep::transaction_id& transaction_id) const
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    streaming_appliers_map::const_iterator i(
        streaming_appliers_.find(std::make_pair(server_id, transaction_id)));
    return (i == streaming_appliers_.end() ? 0 : i->second);
}

wsrep::high_priority_service* wsrep::server_state::find_streaming_applier(
    const wsrep::xid& xid) const
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    streaming_appliers_map::const_iterator i(streaming_appliers_.begin());
    while (i != streaming_appliers_.end())
    {
        wsrep::high_priority_service* sa(i->second);
        if (sa->transaction().xid() == xid)
        {
            return sa;
        }
        i++;
    }
    return NULL;
}

//////////////////////////////////////////////////////////////////////////////
//                              Private                                     //
//////////////////////////////////////////////////////////////////////////////

int wsrep::server_state::desync(wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());
    ++desync_count_;
    lock.unlock();
    int ret(provider().desync());
    lock.lock();
    if (ret)
    {
        --desync_count_;
    }
    return ret;
}

void wsrep::server_state::resync(wsrep::unique_lock<wsrep::mutex>&
                                 lock WSREP_UNUSED)
{
    assert(lock.owns_lock());
    assert(desync_count_ > 0);
    if (desync_count_ > 0)
    {
        --desync_count_;
        if (provider().resync())
        {
            throw wsrep::runtime_error("Failed to resync");
        }
    }
    else
    {
        wsrep::log_warning() << "desync_count " << desync_count_
                             << " on resync";
    }
}


void wsrep::server_state::state(
    wsrep::unique_lock<wsrep::mutex>& lock WSREP_UNUSED,
    enum wsrep::server_state::state state)
{
    assert(lock.owns_lock());
    static const char allowed[n_states_][n_states_] =
        {
            /* dis, ing, ized, cted, jer, jed, dor, sed, ding */
            {  0,   1,   0,    1,    0,   0,   0,   0,   0}, /* dis */
            {  1,   0,   1,    0,    0,   0,   0,   0,   1}, /* ing */
            {  1,   0,   0,    1,    0,   1,   0,   0,   1}, /* ized */
            {  1,   0,   0,    1,    1,   0,   0,   1,   1}, /* cted */
            {  1,   1,   0,    0,    0,   1,   0,   0,   1}, /* jer */
            {  1,   0,   0,    1,    0,   0,   1,   1,   1}, /* jed */
            {  1,   0,   0,    1,    0,   1,   0,   1,   1}, /* dor */
            {  1,   0,   0,    1,    0,   1,   1,   0,   1}, /* sed */
            {  1,   0,   0,    0,    0,   0,   0,   0,   0}  /* ding */
        };

    if (allowed[state_][state] == false)
    {
        std::ostringstream os;
        os << "server: " << name_ << " unallowed state transition: "
           << wsrep::to_string(state_) << " -> " << wsrep::to_string(state);
        wsrep::log_warning() << os.str() << "\n";
        assert(0);
    }

    WSREP_LOG_DEBUG(wsrep::log::debug_log_level(),
                    wsrep::log::debug_level_server_state,
                    "server " << name_ << " state change: "
                    << to_c_string(state_) << " -> "
                    << to_c_string(state));
    state_hist_.push_back(state_);
    server_service_.log_state_change(state_, state);
    state_ = state;
    cond_.notify_all();
    while (state_waiters_[state_])
    {
        cond_.wait(lock);
    }
}

void wsrep::server_state::wait_until_state(
    wsrep::unique_lock<wsrep::mutex>& lock,
    enum wsrep::server_state::state state) const
{
    ++state_waiters_[state];
    while (state_ != state)
    {
        cond_.wait(lock);
        // If the waiter waits for any other state than disconnecting
        // or disconnected and the state has been changed to disconnecting,
        // this usually means that some error was encountered 
        if (state != s_disconnecting && state != s_disconnected
            && state_ == s_disconnecting)
        {
            throw wsrep::runtime_error("State wait was interrupted");
        }
    }
    --state_waiters_[state];
    cond_.notify_all();
}

void wsrep::server_state::interrupt_state_waiters(
    wsrep::unique_lock<wsrep::mutex>& lock WSREP_UNUSED)
{
    assert(lock.owns_lock());
    cond_.notify_all();
}

template <class C>
void wsrep::server_state::recover_streaming_appliers_if_not_recovered(
    wsrep::unique_lock<wsrep::mutex>& lock, C& c)
{
    assert(lock.owns_lock());
    if (streaming_appliers_recovered_ == false)
    {
        lock.unlock();
        server_service_.recover_streaming_appliers(c);
        lock.lock();
    }
    streaming_appliers_recovered_ = true;
}

class transaction_state_cmp
{
public:
    transaction_state_cmp(const enum wsrep::transaction::state s)
        : state_(s) { }
    bool operator()(const std::pair<wsrep::client_id,
                    wsrep::client_state*>& vt) const
    {
        return vt.second->transaction().state() == state_;
    }
private:
    enum wsrep::transaction::state state_;
};

void wsrep::server_state::close_orphaned_sr_transactions(
    wsrep::unique_lock<wsrep::mutex>& lock,
    wsrep::high_priority_service& high_priority_service)
{
    assert(lock.owns_lock());

    // When the originator of an SR transaction leaves the primary
    // component of the cluster, that SR must be rolled back. When two
    // consecutive primary views have the same membership, the system
    // may have been in a state with no primary components.
    // Example with 2 node cluster:
    // - (1,2 primary)
    // - (1 non-primary) and (2 non-primary)
    // - (1,2 primary)
    // We need to rollback SRs owned by both 1 and 2.
    const bool equal_consecutive_views =
        current_view_.equal_membership(previous_primary_view_);

    if (current_view_.own_index() == -1 || equal_consecutive_views)
    {
        streaming_clients_map::iterator i;
        transaction_state_cmp prepared_state_cmp(wsrep::transaction::s_prepared);
        while ((i = std::find_if_not(streaming_clients_.begin(),
                                     streaming_clients_.end(),
                                     prepared_state_cmp))
               != streaming_clients_.end())
        {
            wsrep::client_id client_id(i->first);
            wsrep::transaction_id transaction_id(i->second->transaction().id());
            // It is safe to unlock the server state temporarily here.
            // The processing happens inside view handler which is
            // protected by the provider commit ordering critical
            // section. The lock must be unlocked temporarily to
            // allow converting the current client to streaming
            // applier in transaction::streaming_rollback().
            // The iterator i may be invalidated when the server state
            // remains unlocked, so it should not be accessed after
            // the bf abort call.
            lock.unlock();
            i->second->total_order_bf_abort(current_view_.view_seqno());
            lock.lock();
            streaming_clients_map::const_iterator found_i;
            while ((found_i = streaming_clients_.find(client_id)) !=
                   streaming_clients_.end() &&
                   found_i->second->transaction().id() == transaction_id)
            {
                cond_.wait(lock);
            }
        }
    }

    streaming_appliers_map::iterator i(streaming_appliers_.begin());
    while (i != streaming_appliers_.end())
    {
        wsrep::high_priority_service* streaming_applier(i->second);

        // Rollback SR on equal consecutive primary views or if its
        // originator is not in the current view.
        // Transactions in prepared state must be committed or
        // rolled back explicitly, those are never rolled back here.
        if ((streaming_applier->transaction().state() !=
             wsrep::transaction::s_prepared) &&
            (equal_consecutive_views ||
             (std::find_if(current_view_.members().begin(),
                           current_view_.members().end(),
                           server_id_cmp(i->first.first)) ==
              current_view_.members().end())))
        {
            WSREP_LOG_DEBUG(wsrep::log::debug_log_level(),
                            wsrep::log::debug_level_server_state,
                            "Removing SR fragments for "
                            << i->first.first
                            << ", " << i->first.second);
            wsrep::id server_id(i->first.first);
            wsrep::transaction_id transaction_id(i->first.second);
            int adopt_error;
            if ((adopt_error = high_priority_service.adopt_transaction(
                     streaming_applier->transaction())))
            {
                log_adopt_error(streaming_applier->transaction());
            }
            // Even if the transaction adopt above fails, we roll back
            // the transaction. Adopt error will leave stale entries
            // in the streaming log which can be removed manually.
            {
                wsrep::high_priority_switch sw(high_priority_service,
                                               *streaming_applier);
                streaming_applier->rollback(
                    wsrep::ws_handle(), wsrep::ws_meta());
                streaming_applier->after_apply();
            }

            streaming_appliers_.erase(i++);
            server_service_.release_high_priority_service(streaming_applier);
            high_priority_service.store_globals();
            wsrep::ws_meta ws_meta(
                wsrep::gtid(),
                wsrep::stid(server_id, transaction_id, wsrep::client_id()),
                wsrep::seqno::undefined(), 0);
            lock.unlock();
            if (adopt_error == 0)
            {
                high_priority_service.remove_fragments(ws_meta);
                high_priority_service.commit(wsrep::ws_handle(transaction_id, 0),
                                             ws_meta);
            }
            high_priority_service.after_apply();
            lock.lock();
        }
        else
        {
            ++i;
        }
    }
}

void wsrep::server_state::close_transactions_at_disconnect(
    wsrep::high_priority_service& high_priority_service)
{
    // Close streaming applier without removing fragments
    // from fragment storage. When the server is started again,
    // it must be able to recover ongoing streaming transactions.
    streaming_appliers_map::iterator i(streaming_appliers_.begin());
    while (i != streaming_appliers_.end())
    {
        wsrep::high_priority_service* streaming_applier(i->second);
        {
            wsrep::high_priority_switch sw(high_priority_service,
                                           *streaming_applier);
            streaming_applier->rollback(
                wsrep::ws_handle(), wsrep::ws_meta());
            streaming_applier->after_apply();
        }
        streaming_appliers_.erase(i++);
        server_service_.release_high_priority_service(streaming_applier);
        high_priority_service.store_globals();
    }
    streaming_appliers_recovered_ = false;
}
