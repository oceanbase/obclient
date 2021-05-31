/*
 * Copyright (C) 2018 Codership Oy <info@codership.com>
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

#include "wsrep/transaction.hpp"
#include "wsrep/client_state.hpp"
#include "wsrep/server_state.hpp"
#include "wsrep/storage_service.hpp"
#include "wsrep/high_priority_service.hpp"
#include "wsrep/key.hpp"
#include "wsrep/logger.hpp"
#include "wsrep/compiler.hpp"

#include <sstream>
#include <memory>

namespace
{
    class storage_service_deleter
    {
    public:
        storage_service_deleter(wsrep::server_service& server_service)
            : server_service_(server_service)
        { }
        void operator()(wsrep::storage_service* storage_service)
        {
            server_service_.release_storage_service(storage_service);
        }
    private:
        wsrep::server_service& server_service_;
    };

    template <class D>
    class scoped_storage_service
    {
    public:
        scoped_storage_service(wsrep::client_service& client_service,
                               wsrep::storage_service* storage_service,
                               D deleter)
            : client_service_(client_service)
            , storage_service_(storage_service)
            , deleter_(deleter)
        {
            if (storage_service_ == 0)
            {
                throw wsrep::runtime_error("Null client_state provided");
            }
            client_service_.reset_globals();
            storage_service_->store_globals();
        }

        wsrep::storage_service& storage_service()
        {
            return *storage_service_;
        }

        ~scoped_storage_service()
        {
            deleter_(storage_service_);
            client_service_.store_globals();
        }
    private:
        scoped_storage_service(const scoped_storage_service&);
        scoped_storage_service& operator=(const scoped_storage_service&);
        wsrep::client_service& client_service_;
        wsrep::storage_service* storage_service_;
        D deleter_;
    };
}

// Public

wsrep::transaction::transaction(
    wsrep::client_state& client_state)
    : server_service_(client_state.server_state().server_service())
    , client_service_(client_state.client_service())
    , client_state_(client_state)
    , server_id_()
    , id_(transaction_id::undefined())
    , state_(s_executing)
    , state_hist_()
    , bf_abort_state_(s_executing)
    , bf_abort_provider_status_()
    , bf_abort_client_state_()
    , bf_aborted_in_total_order_()
    , ws_handle_()
    , ws_meta_()
    , flags_()
    , pa_unsafe_(false)
    , implicit_deps_(false)
    , certified_(false)
    , fragments_certified_for_statement_()
    , streaming_context_()
    , sr_keys_()
    , apply_error_buf_()
    , xid_()
{ }


wsrep::transaction::~transaction()
{
}

int wsrep::transaction::start_transaction(
    const wsrep::transaction_id& id)
{
    debug_log_state("start_transaction enter");
    assert(active() == false);
    assert(is_xa() == false);
    assert(flags() == 0);
    server_id_ = client_state_.server_state().id();
    id_ = id;
    state_ = s_executing;
    state_hist_.clear();
    ws_handle_ = wsrep::ws_handle(id);
    flags(wsrep::provider::flag::start_transaction);
    switch (client_state_.mode())
    {
    case wsrep::client_state::m_high_priority:
        debug_log_state("start_transaction success");
        return 0;
    case wsrep::client_state::m_local:
        debug_log_state("start_transaction success");
        return provider().start_transaction(ws_handle_);
    default:
        debug_log_state("start_transaction error");
        assert(0);
        return 1;
    }
}

int wsrep::transaction::start_transaction(
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta)
{
    debug_log_state("start_transaction enter");
    if (state() != s_replaying)
    {
        // assert(ws_meta.flags());
        assert(active() == false);
        assert(flags() == 0);
        server_id_ = ws_meta.server_id();
        id_ = ws_meta.transaction_id();
        assert(client_state_.mode() == wsrep::client_state::m_high_priority);
        state_ = s_executing;
        state_hist_.clear();
        ws_handle_ = ws_handle;
        ws_meta_ = ws_meta;
        flags(wsrep::provider::flag::start_transaction);
        certified_ = true;
    }
    else
    {
        ws_meta_ = ws_meta;
        assert(ws_meta_.flags() & wsrep::provider::flag::commit);
        assert(active());
        assert(client_state_.mode() == wsrep::client_state::m_high_priority);
        assert(state() == s_replaying);
        assert(ws_meta_.seqno().is_undefined() == false);
        certified_ = true;
    }
    debug_log_state("start_transaction leave");
    return 0;
}

int wsrep::transaction::next_fragment(
    const wsrep::ws_meta& ws_meta)
{
    debug_log_state("next_fragment enter");
    ws_meta_ = ws_meta;
    debug_log_state("next_fragment leave");
    return 0;
}

void wsrep::transaction::adopt(const wsrep::transaction& transaction)
{
    debug_log_state("adopt enter");
    assert(transaction.is_streaming());
    start_transaction(transaction.id());
    server_id_ = transaction.server_id_;
    flags_  = transaction.flags();
    streaming_context_ = transaction.streaming_context();
    debug_log_state("adopt leave");
}

void wsrep::transaction::fragment_applied(wsrep::seqno seqno)
{
    assert(active());
    streaming_context_.applied(seqno);
}

int wsrep::transaction::prepare_for_ordering(
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta,
    bool is_commit)
{
    assert(active());

    if (state_ != s_replaying)
    {
        ws_handle_ = ws_handle;
        ws_meta_ = ws_meta;
        certified_ = is_commit;
    }
    return 0;
}

int wsrep::transaction::assign_read_view(const wsrep::gtid* const gtid)
{
    try
    {
        return provider().assign_read_view(ws_handle_, gtid);
    }
    catch (...)
    {
        wsrep::log_error() << "Failed to assign read view";
        return 1;
    }
}

int wsrep::transaction::append_key(const wsrep::key& key)
{
    try
    {
        debug_log_key_append(key);
        sr_keys_.insert(key);
        return provider().append_key(ws_handle_, key);
    }
    catch (...)
    {
        wsrep::log_error() << "Failed to append key";
        return 1;
    }
}

int wsrep::transaction::append_data(const wsrep::const_buffer& data)
{

    return provider().append_data(ws_handle_, data);
}

int wsrep::transaction::after_row()
{
    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex());
    debug_log_state("after_row_enter");
    int ret(0);
    if (streaming_context_.fragment_size() &&
        streaming_context_.fragment_unit() != streaming_context::statement)
    {
        ret = streaming_step(lock);
    }
    debug_log_state("after_row_leave");
    return ret;
}

int wsrep::transaction::before_prepare(
    wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());
    int ret(0);
    debug_log_state("before_prepare_enter");
    assert(state() == s_executing || state() == s_must_abort ||
           state() == s_replaying);

    if (state() == s_must_abort)
    {
        assert(client_state_.mode() == wsrep::client_state::m_local);
        client_state_.override_error(wsrep::e_deadlock_error);
        return 1;
    }

    switch (client_state_.mode())
    {
    case wsrep::client_state::m_local:
        if (is_streaming())
        {
            client_service_.debug_crash(
                "crash_last_fragment_commit_before_fragment_removal");
            lock.unlock();
            if (client_service_.statement_allowed_for_streaming() == false)
            {
                client_state_.override_error(
                    wsrep::e_error_during_commit,
                    wsrep::provider::error_not_allowed);
                ret = 1;
            }
            else if (!is_xa())
            {
                // Note: we can't remove fragments here for XA,
                // the transaction has already issued XA END and
                // is in IDLE state, no more changes allowed!
                ret = client_service_.remove_fragments();
                if (ret)
                {
                    client_state_.override_error(wsrep::e_deadlock_error);
                }
            }
            lock.lock();
            client_service_.debug_crash(
                "crash_last_fragment_commit_after_fragment_removal");
            if (state() == s_must_abort)
            {
                client_state_.override_error(wsrep::e_deadlock_error);
                ret = 1;
            }
        }

        if (ret == 0)
        {
            if (is_xa())
            {
                // Force fragment replication on XA prepare
                flags(flags() | wsrep::provider::flag::prepare);
                flags(flags() | wsrep::provider::flag::pa_unsafe);
                append_sr_keys_for_commit();
                const bool force_streaming_step = true;
                ret = streaming_step(lock, force_streaming_step);
                if (ret == 0)
                {
                    assert(state() == s_executing);
                    state(lock, s_preparing);
                }
            }
            else
            {
                ret = certify_commit(lock);
            }

            assert((ret == 0 && state() == s_preparing) ||
                   (state() == s_must_abort ||
                    state() == s_must_replay ||
                    state() == s_cert_failed));

            if (ret)
            {
                assert(state() == s_must_replay ||
                       client_state_.current_error());
                ret = 1;
            }
        }

        break;
    case wsrep::client_state::m_high_priority:
        // Note: fragment removal is done from applying
        // context for high priority mode.
        if (is_xa())
        {
            assert(state() == s_executing ||
                   state() == s_replaying);
            if (state() == s_replaying)
            {
                break;
            }
        }
        state(lock, s_preparing);
        break;
    default:
        assert(0);
        break;
    }

    assert(state() == s_preparing ||
           (is_xa() && state() == s_replaying) ||
           (ret && (state() == s_must_abort ||
                    state() == s_must_replay ||
                    state() == s_cert_failed ||
                    state() == s_aborted)));
    debug_log_state("before_prepare_leave");
    return ret;
}

int wsrep::transaction::after_prepare(
    wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());

    int ret = 0;
    debug_log_state("after_prepare_enter");
    if (is_xa())
    {
        switch (state())
        {
        case s_preparing:
            assert(client_state_.mode() == wsrep::client_state::m_local ||
                   (certified() && ordered()));
            state(lock, s_prepared);
            break;
        case s_must_abort:
            // prepared locally, but client has not received
            // a result yet. We can still abort.
            assert(client_state_.mode() == wsrep::client_state::m_local);
            client_state_.override_error(wsrep::e_deadlock_error);
            ret = 1;
            break;
        case s_replaying:
            assert(client_state_.mode() == wsrep::client_state::m_high_priority);
            break;
        default:
            assert(0);
        }
    }
    else
    {
        assert(certified() && ordered());
        assert(state() == s_preparing || state() == s_must_abort);

        if (state() == s_must_abort)
        {
            assert(client_state_.mode() == wsrep::client_state::m_local);
            state(lock, s_must_replay);
            ret = 1;
        }
        else
        {
            state(lock, s_committing);
        }
    }
    debug_log_state("after_prepare_leave");
    return ret;
}

int wsrep::transaction::before_commit()
{
    int ret(1);

    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex());
    debug_log_state("before_commit_enter");
    assert(client_state_.mode() != wsrep::client_state::m_toi);
    assert(state() == s_executing ||
           state() == s_prepared ||
           state() == s_committing ||
           state() == s_must_abort ||
           state() == s_replaying);
    assert((state() != s_committing && state() != s_replaying) ||
           certified());

    switch (client_state_.mode())
    {
    case wsrep::client_state::m_local:
        if (state() == s_executing)
        {
            ret = before_prepare(lock) || after_prepare(lock);
            assert((ret == 0 &&
                    (state() == s_committing || state() == s_prepared))
                   ||
                   (state() == s_must_abort ||
                    state() == s_must_replay ||
                    state() == s_cert_failed ||
                    state() == s_aborted));
        }
        else if (state() != s_committing && state() != s_prepared)
        {
            assert(state() == s_must_abort);
            if (certified() ||
                (is_xa() && is_streaming()))
            {
                state(lock, s_must_replay);
            }
            else
            {
                client_state_.override_error(wsrep::e_deadlock_error);
            }
        }
        else
        {
            // 2PC commit, prepare was done before
            ret = 0;
        }

        if (ret == 0 && state() == s_prepared)
        {
            ret = certify_commit(lock);
            assert((ret == 0 && state() == s_committing) ||
                   (state() == s_must_abort ||
                    state() == s_must_replay ||
                    state() == s_cert_failed ||
                    state() == s_prepared));
        }

        if (ret == 0)
        {
            assert(certified());
            assert(ordered());
            lock.unlock();
            client_service_.debug_sync("wsrep_before_commit_order_enter");
            enum wsrep::provider::status
                status(provider().commit_order_enter(ws_handle_, ws_meta_));
            lock.lock();
            switch (status)
            {
            case wsrep::provider::success:
                break;
            case wsrep::provider::error_bf_abort:
                if (state() != s_must_abort)
                {
                    state(lock, s_must_abort);
                }
                state(lock, s_must_replay);
                ret = 1;
                break;
            default:
                ret = 1;
                assert(0);
                break;
            }
        }
        break;
    case wsrep::client_state::m_high_priority:
        assert(certified());
        assert(ordered());
        if (is_xa())
        {
            assert(state() == s_prepared ||
                   state() == s_replaying);
            state(lock, s_committing);
            ret = 0;
        }
        else if (state() == s_executing || state() == s_replaying)
        {
            ret = before_prepare(lock) || after_prepare(lock);
        }
        else
        {
            ret = 0;
        }
        lock.unlock();
        ret = ret || provider().commit_order_enter(ws_handle_, ws_meta_);
        lock.lock();
        if (ret)
        {
            state(lock, s_must_abort);
            state(lock, s_aborting);
        }
        break;
    default:
        assert(0);
        break;
    }
    debug_log_state("before_commit_leave");
    return ret;
}

int wsrep::transaction::ordered_commit()
{
    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex());
    debug_log_state("ordered_commit_enter");
    assert(state() == s_committing);
    assert(ordered());
    client_service_.debug_sync("wsrep_before_commit_order_leave");
    int ret(provider().commit_order_leave(ws_handle_, ws_meta_,
                                          apply_error_buf_));
    client_service_.debug_sync("wsrep_after_commit_order_leave");
    // Should always succeed:
    // 1) If before commit before succeeds, the transaction handle
    //    in the provider is guaranteed to exist and the commit
    //    has been ordered
    // 2) The transaction which has been ordered for commit cannot be BF
    //    aborted anymore
    // 3) The provider should always guarantee that the transactions which
    //    have been ordered for commit can finish committing.
    //
    // The exception here is a storage service transaction which is running
    // in high priority mode. The fragment storage commit may get BF
    // aborted in the provider after commit ordering has been
    // established since the transaction is operating in streaming
    // mode.
    if (ret)
    {
        assert(client_state_.mode() == wsrep::client_state::m_high_priority);
        state(lock, s_must_abort);
        state(lock, s_aborting);
    }
    else
    {
        state(lock, s_ordered_commit);
    }
    debug_log_state("ordered_commit_leave");
    return ret;
}

int wsrep::transaction::after_commit()
{
    int ret(0);

    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex());
    debug_log_state("after_commit_enter");
    assert(state() == s_ordered_commit);

    if (is_streaming())
    {
        assert(client_state_.mode() == wsrep::client_state::m_local ||
               client_state_.mode() == wsrep::client_state::m_high_priority);

        if (is_xa())
        {
            // XA fragment removal happens here,
            // see comment in before_prepare
            lock.unlock();
            scoped_storage_service<storage_service_deleter>
                sr_scope(
                    client_service_,
                    server_service_.storage_service(client_service_),
                    storage_service_deleter(server_service_));
            wsrep::storage_service& storage_service(
                sr_scope.storage_service());
            storage_service.adopt_transaction(*this);
            storage_service.remove_fragments();
            storage_service.commit(wsrep::ws_handle(), wsrep::ws_meta());
            lock.lock();
        }

        if (client_state_.mode() == wsrep::client_state::m_local)
        {
            lock.unlock();
            client_state_.server_state_.stop_streaming_client(&client_state_);
            lock.lock();
        }
        clear_fragments();
    }

    switch (client_state_.mode())
    {
    case wsrep::client_state::m_local:
        ret = provider().release(ws_handle_);
        break;
    case wsrep::client_state::m_high_priority:
        break;
    default:
        assert(0);
        break;
    }
    assert(ret == 0);
    state(lock, s_committed);

    // client_state_.server_state().last_committed_gtid(ws_meta.gitd());
    debug_log_state("after_commit_leave");
    return ret;
}

int wsrep::transaction::before_rollback()
{
    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex());
    debug_log_state("before_rollback_enter");
    assert(state() == s_executing ||
           state() == s_preparing ||
           state() == s_prepared ||
           state() == s_must_abort ||
           // Background rollbacker or rollback initiated from SE
           state() == s_aborting ||
           state() == s_cert_failed ||
           state() == s_must_replay);

    switch (client_state_.mode())
    {
    case wsrep::client_state::m_local:
        if (is_streaming())
        {
            client_service_.debug_sync("wsrep_before_SR_rollback");
        }
        switch (state())
        {
        case s_preparing:
            // Error detected during prepare phase
            state(lock, s_must_abort);
            // fall through
        case s_prepared:
            // fall through
        case s_executing:
            // Voluntary rollback
            if (is_streaming())
            {
                streaming_rollback(lock);
            }
            state(lock, s_aborting);
            break;
        case s_must_abort:
            if (certified())
            {
                state(lock, s_must_replay);
            }
            else
            {
                if (is_streaming())
                {
                    streaming_rollback(lock);
                }
                state(lock, s_aborting);
            }
            break;
        case s_cert_failed:
            if (is_streaming())
            {
                streaming_rollback(lock);
            }
            state(lock, s_aborting);
            break;
        case s_aborting:
            if (is_streaming())
            {
                streaming_rollback(lock);
            }
            break;
        case s_must_replay:
            break;
        default:
            assert(0);
            break;
        }
        break;
    case wsrep::client_state::m_high_priority:
        // Rollback by rollback write set or BF abort
        assert(state_ == s_executing || state_ == s_prepared || state_ == s_aborting);
        if (state_ != s_aborting)
        {
            state(lock, s_aborting);
        }
        break;
    default:
        assert(0);
        break;
    }

    debug_log_state("before_rollback_leave");
    return 0;
}

int wsrep::transaction::after_rollback()
{
    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex());
    debug_log_state("after_rollback_enter");
    assert(state() == s_aborting ||
           state() == s_must_replay);

    if (is_streaming() && bf_aborted_in_total_order_)
    {
        lock.unlock();
        // Storage service scope
        {
            scoped_storage_service<storage_service_deleter>
                sr_scope(
                    client_service_,
                    server_service_.storage_service(client_service_),
                    storage_service_deleter(server_service_));
            wsrep::storage_service& storage_service(
                sr_scope.storage_service());
            storage_service.adopt_transaction(*this);
            storage_service.remove_fragments();
            storage_service.commit(wsrep::ws_handle(), wsrep::ws_meta());
        }
        lock.lock();
        streaming_context_.cleanup();
    }

    if (is_streaming() && state() != s_must_replay)
    {
        clear_fragments();
    }

    if (state() == s_aborting)
    {
        state(lock, s_aborted);
    }

    // Releasing the transaction from provider is postponed into
    // after_statement() hook. Depending on DBMS system all the
    // resources acquired by transaction may or may not be released
    // during actual rollback. If the transaction has been ordered,
    // releasing the commit ordering critical section should be
    // also postponed until all resources have been released.
    debug_log_state("after_rollback_leave");
    return 0;
}

int wsrep::transaction::release_commit_order(
    wsrep::unique_lock<wsrep::mutex>& lock)
{
    lock.unlock();
    int ret(provider().commit_order_enter(ws_handle_, ws_meta_));
    lock.lock();
    if (!ret)
    {
        server_service_.set_position(client_service_, ws_meta_.gtid());
        ret = provider().commit_order_leave(ws_handle_, ws_meta_,
                                            apply_error_buf_);
    }
    return ret;
}

int wsrep::transaction::after_statement()
{
    int ret(0);
    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex());
    debug_log_state("after_statement_enter");
    assert(client_state_.mode() == wsrep::client_state::m_local);
    assert(state() == s_executing ||
           state() == s_prepared ||
           state() == s_committed ||
           state() == s_aborted ||
           state() == s_must_abort ||
           state() == s_cert_failed ||
           state() == s_must_replay);

    if (state() == s_executing &&
        streaming_context_.fragment_size() &&
        streaming_context_.fragment_unit() == streaming_context::statement)
    {
        ret = streaming_step(lock);
    }

    switch (state())
    {
    case s_executing:
        // ?
        break;
    case s_prepared:
        assert(is_xa());
        break;
    case s_committed:
        assert(is_streaming() == false);
        break;
    case s_must_abort:
    case s_cert_failed:
        client_state_.override_error(wsrep::e_deadlock_error);
        lock.unlock();
        ret = client_service_.bf_rollback();
        lock.lock();
        if (state() != s_must_replay)
        {
            break;
        }
        // Continue to replay if rollback() changed the state to s_must_replay
        // Fall through
    case s_must_replay:
    {
        if (is_xa() && !ordered())
        {
            ret = xa_replay(lock);
        }
        else
        {
            ret = replay(lock);
        }
        break;
    }
    case s_aborted:
        // Raise a deadlock error if the transaction was BF aborted and
        // rolled back by client outside of transaction hooks.
        if (bf_aborted() && client_state_.current_error() == wsrep::e_success &&
            !client_service_.is_xa_rollback())
        {
            client_state_.override_error(wsrep::e_deadlock_error);
        }
        break;
    default:
        assert(0);
        break;
    }

    assert(state() == s_executing ||
           state() == s_prepared ||
           state() == s_committed ||
           state() == s_aborted   ||
           state() == s_must_replay);

    if (state() == s_aborted)
    {
        if (ordered())
        {
            ret = release_commit_order(lock);
        }
        lock.unlock();
        provider().release(ws_handle_);
        lock.lock();
    }

    if (state() != s_executing &&
        (!client_service_.is_explicit_xa() ||
         client_state_.state() == wsrep::client_state::s_quitting))
    {
        cleanup();
    }
    fragments_certified_for_statement_ = 0;
    debug_log_state("after_statement_leave");
    assert(ret == 0 || state() == s_aborted);
    return ret;
}

void wsrep::transaction::after_applying()
{
    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex_);
    debug_log_state("after_applying enter");
    assert(state_ == s_executing ||
           state_ == s_prepared ||
           state_ == s_committed ||
           state_ == s_aborted ||
           state_ == s_replaying);

    if (state_ != s_executing && state_ != s_prepared)
    {
        if (state_ == s_replaying) state(lock, s_aborted);
        cleanup();
    }
    else
    {
        // State remains executing or prepared, so this is a streaming applier.
        // Reset the meta data to avoid releasing commit order
        // critical section above if the next fragment is rollback
        // fragment. Rollback fragment ordering will be handled by
        // another instance while removing the fragments from
        // storage.
        ws_meta_ = wsrep::ws_meta();
    }
    debug_log_state("after_applying leave");
}

bool wsrep::transaction::bf_abort(
    wsrep::unique_lock<wsrep::mutex>& lock,
    wsrep::seqno bf_seqno)
{
    bool ret(false);
    const enum wsrep::transaction::state state_at_enter(state());
    assert(lock.owns_lock());

    if (active() == false)
    {
         WSREP_LOG_DEBUG(client_state_.debug_log_level(),
                         wsrep::log::debug_level_transaction,
                         "Transaction not active, skipping bf abort");
    }
    else
    {
        switch (state_at_enter)
        {
        case s_executing:
        case s_preparing:
        case s_prepared:
        case s_certifying:
        case s_committing:
        {
            wsrep::seqno victim_seqno;
            enum wsrep::provider::status
                status(client_state_.provider().bf_abort(
                           bf_seqno, id_, victim_seqno));
            switch (status)
            {
            case wsrep::provider::success:
                WSREP_LOG_DEBUG(client_state_.debug_log_level(),
                                wsrep::log::debug_level_transaction,
                                "Seqno " << bf_seqno
                                << " succesfully BF aborted " << id_
                                << " victim_seqno " << victim_seqno);
                bf_abort_state_ = state_at_enter;
                state(lock, s_must_abort);
                ret = true;
                break;
            default:
                WSREP_LOG_DEBUG(client_state_.debug_log_level(),
                                wsrep::log::debug_level_transaction,
                                "Seqno " << bf_seqno
                                << " failed to BF abort " << id_
                                << " with status " << status
                                << " victim_seqno " << victim_seqno);
                break;
            }
            break;
        }
        default:
                WSREP_LOG_DEBUG(client_state_.debug_log_level(),
                                wsrep::log::debug_level_transaction,
                                "BF abort not allowed in state "
                                << wsrep::to_string(state_at_enter));
            break;
        }
    }

    if (ret)
    {
        bf_abort_client_state_ = client_state_.state();
        // If the transaction is in executing state, we must initiate
        // streaming rollback to ensure that the rollback fragment gets
        // replicated before the victim starts to roll back and release locks.
        // In other states the BF abort will be detected outside of
        // storage engine operations and streaming rollback will be
        // handled from before_rollback() call.
        if (client_state_.mode() == wsrep::client_state::m_local &&
            is_streaming() && state_at_enter == s_executing)
        {
            streaming_rollback(lock);
        }

        if ((client_state_.state() == wsrep::client_state::s_idle &&
             client_state_.server_state().rollback_mode() ==
             wsrep::server_state::rm_sync) // locally processing idle
            ||
            // high priority streaming
            (client_state_.mode() == wsrep::client_state::m_high_priority &&
             is_streaming()))
        {
            // We need to change the state to aborting under the
            // lock protection to avoid a race between client thread,
            // otherwise it could happen that the client gains control
            // between releasing the lock and before background
            // rollbacker gets control.
            if (is_xa() && state_at_enter == s_prepared)
            {
                state(lock, s_must_replay);
                client_state_.set_rollbacker_active(true);
            }
            else
            {
                state(lock, s_aborting);
                client_state_.set_rollbacker_active(true);
                if (client_state_.mode() == wsrep::client_state::m_high_priority)
                {
                    lock.unlock();
                    client_state_.server_state().stop_streaming_applier(
                        server_id_, id_);
                    lock.lock();
                }
            }

            lock.unlock();
            server_service_.background_rollback(client_state_);
        }
    }
    return ret;
}

bool wsrep::transaction::total_order_bf_abort(
    wsrep::unique_lock<wsrep::mutex>& lock WSREP_UNUSED,
    wsrep::seqno bf_seqno)
{
    bool ret(bf_abort(lock, bf_seqno));
    if (ret)
    {
        bf_aborted_in_total_order_ = true;
    }
    return ret;
}

void wsrep::transaction::clone_for_replay(const wsrep::transaction& other)
{
    assert(other.state() == s_replaying);
    id_ = other.id_;
    xid_ = other.xid_;
    server_id_ = other.server_id_;
    ws_handle_ = other.ws_handle_;
    ws_meta_ = other.ws_meta_;
    streaming_context_ = other.streaming_context_;
    state_ = s_replaying;
}

void wsrep::transaction::assign_xid(const wsrep::xid& xid)
{
    assert(active());
    assert(!is_xa());
    xid_ = xid;
}

int wsrep::transaction::restore_to_prepared_state(const wsrep::xid& xid)
{
    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex_);
    assert(active());
    assert(is_empty());
    assert(state() == s_executing || state() == s_replaying);
    flags(flags() & ~wsrep::provider::flag::start_transaction);
    if (state() == s_executing)
    {
        state(lock, s_certifying);
    }
    else
    {
        state(lock, s_preparing);
    }
    state(lock, s_prepared);
    xid_ = xid;
    return 0;
}

int wsrep::transaction::commit_or_rollback_by_xid(const wsrep::xid& xid,
                                                  bool commit)
{
    debug_log_state("commit_or_rollback_by_xid enter");
    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex_);
    wsrep::server_state& server_state(client_state_.server_state());
    wsrep::high_priority_service* sa(server_state.find_streaming_applier(xid));

    if (!sa)
    {
        assert(sa);
        client_state_.override_error(wsrep::e_error_during_commit);
        return 1;
    }

    int flags(0);
    if (commit)
    {
        flags = wsrep::provider::flag::commit;
    }
    else
    {
        flags = wsrep::provider::flag::rollback;
    }
    flags = flags | wsrep::provider::flag::pa_unsafe;
    wsrep::stid stid(sa->transaction().server_id(),
                     sa->transaction().id(),
                     client_state_.id());
    wsrep::ws_meta meta(stid);

    const enum wsrep::provider::status cert_ret(
        provider().certify(client_state_.id(),
                           ws_handle_,
                           flags,
                           meta));

    int ret;
    if (cert_ret == wsrep::provider::success)
    {
        if (commit)
        {
            state(lock, s_certifying);
            state(lock, s_committing);
            state(lock, s_committed);
        }
        else
        {
            state(lock, s_aborting);
            state(lock, s_aborted);
        }
        ret = 0;
    }
    else
    {
        client_state_.override_error(wsrep::e_error_during_commit,
                                     cert_ret);
        wsrep::log_error() << "Failed to commit_or_rollback_by_xid,"
                           << " xid: " << xid
                           << " error: " << cert_ret;
        ret = 1;
    }
    debug_log_state("commit_or_rollback_by_xid leave");
    return ret;
}

void wsrep::transaction::xa_detach()
{
    debug_log_state("xa_detach enter");
    assert(state() == s_prepared);
    wsrep::server_state& server_state(client_state_.server_state());
    server_state.convert_streaming_client_to_applier(&client_state_);
    client_service_.store_globals();
    client_service_.cleanup_transaction();
    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex_);
    streaming_context_.cleanup();
    state(lock, s_aborting);
    state(lock, s_aborted);
    provider().release(ws_handle_);
    cleanup();
    debug_log_state("xa_detach leave");
}

int wsrep::transaction::xa_replay(wsrep::unique_lock<wsrep::mutex>& lock)
{
    debug_log_state("xa_replay enter");
    assert(lock.owns_lock());
    assert(is_xa());
    assert(is_streaming());
    assert(state() == s_must_replay);
    assert(bf_aborted());

    state(lock, s_replaying);

    enum wsrep::provider::status status;
    wsrep::server_state& server_state(client_state_.server_state());

    lock.unlock();
    server_state.convert_streaming_client_to_applier(&client_state_);
    status = client_service_.replay_unordered();
    client_service_.store_globals();
    lock.lock();

    if (status != wsrep::provider::success)
    {
        client_service_.emergency_shutdown();
    }

    int ret(1);
    if (bf_abort_client_state_ == wsrep::client_state::s_idle)
    {
        state(lock, s_aborted);
        streaming_context_.cleanup();
        provider().release(ws_handle_);
        cleanup();
        ret = 0;
    }
    else
    {
        lock.unlock();
        enum wsrep::provider::status status(client_service_.commit_by_xid());
        lock.lock();
        switch (status)
        {
        case wsrep::provider::success:
            state(lock, s_committed);
            streaming_context_.cleanup();
            provider().release(ws_handle_);
            cleanup();
            ret = 0;
            break;
        default:
            log_warning() << "Failed to commit by xid during replay";
            // Commit by xid failed, return a commit
            // error and let the client retry
            state(lock, s_preparing);
            state(lock, s_prepared);
            client_state_.override_error(wsrep::e_error_during_commit, status);
        }
    }

    client_service_.signal_replayed();
    debug_log_state("xa_replay leave");
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
//                                 Private                                    //
////////////////////////////////////////////////////////////////////////////////

inline wsrep::provider& wsrep::transaction::provider()
{
    return client_state_.server_state().provider();
}

void wsrep::transaction::state(
    wsrep::unique_lock<wsrep::mutex>& lock __attribute__((unused)),
    enum wsrep::transaction::state next_state)
{
    WSREP_LOG_DEBUG(client_state_.debug_log_level(),
                    wsrep::log::debug_level_transaction,
                    "client: " << client_state_.id().get()
                    << " txc: " << id().get()
                    << " state: " << to_string(state_)
                    << " -> " << to_string(next_state));

    assert(lock.owns_lock());
    // BF aborter is allowed to change the state without gaining control
    // to the state if the next state is s_must_abort, s_aborting or
    // s_must_replay (for xa idle replay).
    assert(client_state_.owning_thread_id_ == wsrep::this_thread::get_id() ||
           next_state == s_must_abort ||
           next_state == s_must_replay ||
           next_state == s_aborting);

    static const char allowed[n_states][n_states] =
        { /*  ex pg pd ce co oc ct cf ma ab ad mr re */
            { 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0}, /* ex */
            { 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0}, /* pg */
            { 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0}, /* pd */
            { 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0}, /* ce */
            { 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0}, /* co */
            { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}, /* oc */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* ct */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}, /* cf */
            { 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0}, /* ma */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}, /* ab */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* ad */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, /* mr */
            { 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0}  /* re */
        };

    if (!allowed[state_][next_state])
    {
        wsrep::log_debug() << "unallowed state transition for transaction "
                           << id_ << ": " << wsrep::to_string(state_)
                           << " -> " << wsrep::to_string(next_state);
        assert(0);
    }

    state_hist_.push_back(state_);
    if (state_hist_.size() == 12)
    {
        state_hist_.erase(state_hist_.begin());
    }
    state_ = next_state;

    if (state_ == s_must_replay)
    {
        client_service_.will_replay();
    }
}

bool wsrep::transaction::abort_or_interrupt(
    wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());
    if (state() == s_must_abort)
    {
        client_state_.override_error(wsrep::e_deadlock_error);
        return true;
    }
    else if (state() == s_aborting || state() == s_aborted)
    {
        // Somehow we got there after BF abort without setting error
        // status. This may happen if the DBMS side aborts the transaction
        // but still tries to continue processing rows. Raise the error here
        // and assert so that the error will be caught in debug builds.
        if (bf_abort_client_state_ &&
            client_state_.current_error() == wsrep::e_success)
        {
            client_state_.override_error(wsrep::e_deadlock_error);
            assert(0);
        }
        return true;
    }
    else if (client_service_.interrupted(lock))
    {
        client_state_.override_error(wsrep::e_interrupted_error);
        if (state() != s_must_abort)
        {
            state(lock, s_must_abort);
        }
        return true;
    }
    return false;
}

int wsrep::transaction::streaming_step(wsrep::unique_lock<wsrep::mutex>& lock,
                                       bool force)
{
    assert(lock.owns_lock());
    assert(streaming_context_.fragment_size() || is_xa());

    if (client_service_.bytes_generated() <
        streaming_context_.log_position())
    {
        /* Something went wrong on DBMS side in keeping track of
           generated bytes. Return an error to abort the transaction. */
        wsrep::log_warning() << "Bytes generated "
                             << client_service_.bytes_generated()
                             << " less than bytes certified "
                             << streaming_context_.log_position()
                             << ", aborting streaming transaction";
        return 1;
    }
    int ret(0);

    const size_t bytes_to_replicate(client_service_.bytes_generated() -
                                    streaming_context_.log_position());

    switch (streaming_context_.fragment_unit())
    {
    case streaming_context::row:
        // fall through
    case streaming_context::statement:
        streaming_context_.increment_unit_counter(1);
        break;
    case streaming_context::bytes:
        streaming_context_.set_unit_counter(bytes_to_replicate);
        break;
    }

    // Some statements have no effect. Do not atttempt to
    // replicate a fragment if no data has been generated since
    // last fragment replication.
    // We use `force = true` on XA prepare: a fragment will be
    // generated even if no data is pending replication.
    if (bytes_to_replicate <= 0 && !force)
    {
        assert(bytes_to_replicate == 0);
        return ret;
    }

    if (streaming_context_.fragment_size_exceeded() || force)
    {
        streaming_context_.reset_unit_counter();
        ret = certify_fragment(lock);
    }

    return ret;
}

int wsrep::transaction::certify_fragment(
    wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());

    assert(client_state_.mode() == wsrep::client_state::m_local);
    assert(streaming_context_.rolled_back() == false ||
           state() == s_must_abort);

    client_service_.wait_for_replayers(lock);
    if (abort_or_interrupt(lock))
    {
        return 1;
    }

    state(lock, s_certifying);
    lock.unlock();
    client_service_.debug_sync("wsrep_before_fragment_certification");

    wsrep::mutable_buffer data;
    size_t log_position(0);
    if (client_service_.prepare_fragment_for_replication(data, log_position))
    {
        lock.lock();
        state(lock, s_must_abort);
        client_state_.override_error(wsrep::e_error_during_commit);
        return 1;
    }
    streaming_context_.set_log_position(log_position);

    if (data.size() == 0)
    {
        wsrep::log_warning() << "Attempt to replicate empty data buffer";
        lock.lock();
        state(lock, s_executing);
        return 0;
    }

    if (provider().append_data(ws_handle_,
                               wsrep::const_buffer(data.data(), data.size())))
    {
        lock.lock();
        state(lock, s_must_abort);
        client_state_.override_error(wsrep::e_error_during_commit);
        return 1;
    }

    if (is_xa())
    {
        // One more check to see if the transaction
        // has been aborted. This is necessary because
        // until the append_data above will make sure
        // that the transaction exists in provider.
        lock.lock();
        if (abort_or_interrupt(lock))
        {
            return 1;
        }
        lock.unlock();
    }

    if (is_streaming() == false)
    {
        client_state_.server_state_.start_streaming_client(&client_state_);
    }

    if (implicit_deps())
    {
        flags(flags() | wsrep::provider::flag::implicit_deps);
    }

    int ret(0);
    enum wsrep::client_error error(wsrep::e_success);
    enum wsrep::provider::status cert_ret(wsrep::provider::success);
    // Storage service scope
    {
        scoped_storage_service<storage_service_deleter>
            sr_scope(
                client_service_,
                server_service_.storage_service(client_service_),
                storage_service_deleter(server_service_));
        wsrep::storage_service& storage_service(
            sr_scope.storage_service());

        // First the fragment is appended to the stable storage.
        // This is done to ensure that there is enough capacity
        // available to store the fragment. The fragment meta data
        // is updated after certification.
        wsrep::id server_id(client_state_.server_state().id());
        assert(server_id.is_undefined() == false);
        if (storage_service.start_transaction(ws_handle_) ||
            storage_service.append_fragment(
                server_id,
                id(),
                flags(),
                wsrep::const_buffer(data.data(), data.size()),
                xid()))
        {
            ret = 1;
            error = wsrep::e_append_fragment_error;
        }

        if (ret == 0)
        {
            client_service_.debug_crash(
                "crash_replicate_fragment_before_certify");

            wsrep::ws_meta sr_ws_meta;
            cert_ret = provider().certify(client_state_.id(),
                                          ws_handle_,
                                          flags(),
                                          sr_ws_meta);
            client_service_.debug_crash(
                "crash_replicate_fragment_after_certify");

            switch (cert_ret)
            {
            case wsrep::provider::success:
                ++fragments_certified_for_statement_;
                assert(sr_ws_meta.seqno().is_undefined() == false);
                streaming_context_.certified();
                if (storage_service.update_fragment_meta(sr_ws_meta))
                {
                    storage_service.rollback(wsrep::ws_handle(),
                                             wsrep::ws_meta());
                    ret = 1;
                    error = wsrep::e_deadlock_error;
                    break;
                }
                if (storage_service.commit(ws_handle_, sr_ws_meta))
                {
                    ret = 1;
                    error = wsrep::e_deadlock_error;
                }
                else
                {
                    streaming_context_.stored(sr_ws_meta.seqno());
                }
                client_service_.debug_crash(
                    "crash_replicate_fragment_success");
                break;
            case wsrep::provider::error_bf_abort:
            case wsrep::provider::error_certification_failed:
                // Streaming transcation got BF aborted, so it must roll
                // back. Roll back the fragment storage operation out of
                // order as the commit order will be grabbed later on
                // during rollback process. Mark the fragment as certified
                // though in streaming context in order to enter streaming
                // rollback codepath.
                //
                // Note that despite we handle error_certification_failed
                // here, we mark the transaction as streaming. Apparently
                // the provider may return status corresponding to certification
                // failure even if the fragment has passed certification.
                // This may be a bug in provider implementation or a limitation
                // of error codes defined in wsrep-API. In order to make
                // sure that the transaction will be cleaned on other servers,
                // we take a risk of sending one rollback fragment for nothing.
                storage_service.rollback(wsrep::ws_handle(),
                                         wsrep::ws_meta());
                streaming_context_.certified();
                ret = 1;
                error = wsrep::e_deadlock_error;
                break;
            default:
                // Storage service rollback must be done out of order,
                // otherwise there may be a deadlock between BF aborter
                // and the rollback process.
                storage_service.rollback(wsrep::ws_handle(), wsrep::ws_meta());
                ret = 1;
                error = wsrep::e_deadlock_error;
                break;
            }
        }
    }

    // Note: This does not release the handle in the provider
    // since streaming is still on. However it is needed to
    // make provider internal state to transition for the
    // next fragment. If any of the operations above failed,
    // the handle needs to be left unreleased for the following
    // rollback process.
    if (ret == 0)
    {
        assert(error == wsrep::e_success);
        ret = provider().release(ws_handle_);
        if (ret)
        {
            error = wsrep::e_deadlock_error;
        }
    }
    lock.lock();
    if (ret)
    {
        assert(error != wsrep::e_success);
        if (is_streaming() == false)
        {
            lock.unlock();
            client_state_.server_state_.stop_streaming_client(&client_state_);
            lock.lock();
        }
        else
        {
            streaming_rollback(lock);
        }
        if (state_ != s_must_abort)
        {
            state(lock, s_must_abort);
        }
        client_state_.override_error(error, cert_ret);
    }
    else if (state_ == s_must_abort)
    {
        if (is_streaming())
        {
            streaming_rollback(lock);
        }
        client_state_.override_error(wsrep::e_deadlock_error, cert_ret);
        ret = 1;
    }
    else
    {
        assert(state_ == s_certifying);
        state(lock, s_executing);
        flags(flags() & ~wsrep::provider::flag::start_transaction);
    }
    return ret;
}

int wsrep::transaction::certify_commit(
    wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());
    assert(active());
    client_service_.wait_for_replayers(lock);

    assert(lock.owns_lock());

    if (abort_or_interrupt(lock))
    {
        if (is_xa() && state() == s_must_abort)
        {
            state(lock, s_must_replay);
        }
        return 1;
    }

    state(lock, s_certifying);
    lock.unlock();

    if (is_streaming())
    {
        if (!is_xa())
        {
            append_sr_keys_for_commit();
        }
        flags(flags() | wsrep::provider::flag::pa_unsafe);
    }

    if (implicit_deps())
    {
        flags(flags() | wsrep::provider::flag::implicit_deps);
    }

    flags(flags() | wsrep::provider::flag::commit);
    flags(flags() & ~wsrep::provider::flag::prepare);

    if (client_service_.prepare_data_for_replication())
    {
        lock.lock();
        // Here we fake that the size exceeded error came from provider,
        // even though it came from the client service. This requires
        // some consideration how to get meaningful error codes from
        // the client service.
        client_state_.override_error(wsrep::e_size_exceeded_error,
                                     wsrep::provider::error_size_exceeded);
        if (state_ != s_must_abort)
        {
            state(lock, s_must_abort);
        }
        return 1;
    }

    client_service_.debug_sync("wsrep_before_certification");
    enum wsrep::provider::status
        cert_ret(provider().certify(client_state_.id(),
                                   ws_handle_,
                                   flags(),
                                   ws_meta_));
    client_service_.debug_sync("wsrep_after_certification");

    lock.lock();

    assert(state() == s_certifying || state() == s_must_abort);

    int ret(1);
    switch (cert_ret)
    {
    case wsrep::provider::success:
        assert(ordered());
        certified_ = true;
        ++fragments_certified_for_statement_;
        switch (state())
        {
        case s_certifying:
            if (is_xa())
            {
                state(lock, s_committing);
            }
            else
            {
                state(lock, s_preparing);
            }
            ret = 0;
            break;
        case s_must_abort:
            // We got BF aborted after succesful certification
            // and before acquiring client state lock. The trasaction
            // must be replayed.
            state(lock, s_must_replay);
            break;
        default:
            assert(0);
            break;
        }
        break;
    case wsrep::provider::error_warning:
        assert(ordered() == false);
        state(lock, s_must_abort);
        client_state_.override_error(wsrep::e_error_during_commit, cert_ret);
        break;
    case wsrep::provider::error_transaction_missing:
        state(lock, s_must_abort);
        // The execution should never reach this point if the
        // transaction has not generated any keys or data.
        wsrep::log_warning() << "Transaction was missing in provider";
        client_state_.override_error(wsrep::e_error_during_commit, cert_ret);
        break;
    case wsrep::provider::error_bf_abort:
        // Transaction was replicated succesfully and it was either
        // certified succesfully or the result of certifying is not
        // yet known. Therefore the transaction must roll back
        // and go through replay either to replay and commit the whole
        // transaction or to determine failed certification status.
        if (state() != s_must_abort)
        {
            state(lock, s_must_abort);
        }
        state(lock, s_must_replay);
        break;
    case wsrep::provider::error_certification_failed:
        state(lock, s_cert_failed);
        client_state_.override_error(wsrep::e_deadlock_error);
        break;
    case wsrep::provider::error_size_exceeded:
        state(lock, s_must_abort);
        client_state_.override_error(wsrep::e_error_during_commit, cert_ret);
        break;
    case wsrep::provider::error_connection_failed:
        // Galera provider may return CONN_FAIL if the trx is
        // BF aborted O_o. If we see here that the trx was BF aborted,
        // return deadlock error instead of error during commit
        // to reduce number of error state combinations elsewhere.
        if (state() == s_must_abort)
        {
            if (is_xa())
            {
                state(lock, s_must_replay);
            }
            client_state_.override_error(wsrep::e_deadlock_error);
        }
        else
        {
            client_state_.override_error(wsrep::e_error_during_commit,
                                         cert_ret);
            if (is_xa())
            {
                state(lock, s_prepared);
            }
            else
            {
                state(lock, s_must_abort);
            }
        }
        break;
    case wsrep::provider::error_provider_failed:
        if (state() != s_must_abort)
        {
            state(lock, s_must_abort);
        }
        client_state_.override_error(wsrep::e_error_during_commit, cert_ret);
        break;
    case wsrep::provider::error_fatal:
        client_state_.override_error(wsrep::e_error_during_commit, cert_ret);
        state(lock, s_must_abort);
        client_service_.emergency_shutdown();
        break;
    case wsrep::provider::error_not_implemented:
    case wsrep::provider::error_not_allowed:
        client_state_.override_error(wsrep::e_error_during_commit, cert_ret);
        state(lock, s_must_abort);
        wsrep::log_warning() << "Certification operation was not allowed: "
                             << "id: " << id().get()
                             << " flags: " << std::hex << flags() << std::dec;
        break;
    default:
        state(lock, s_must_abort);
        client_state_.override_error(wsrep::e_error_during_commit, cert_ret);
        break;
    }

    return ret;
}

int wsrep::transaction::append_sr_keys_for_commit()
{
    int ret(0);
    assert(client_state_.mode() == wsrep::client_state::m_local);
    for (wsrep::sr_key_set::branch_type::const_iterator
             i(sr_keys_.root().begin());
         ret == 0 && i != sr_keys_.root().end(); ++i)
    {
        for (wsrep::sr_key_set::leaf_type::const_iterator
                 j(i->second.begin());
             ret == 0 && j != i->second.end(); ++j)
        {
            wsrep::key key(wsrep::key::shared);
            key.append_key_part(i->first.data(), i->first.size());
            key.append_key_part(j->data(), j->size());
            ret = provider().append_key(ws_handle_, key);
        }
    }
    return ret;
}

void wsrep::transaction::streaming_rollback(wsrep::unique_lock<wsrep::mutex>& lock)
{
    debug_log_state("streaming_rollback enter");
    assert(state_ != s_must_replay);
    assert(is_streaming());
    if (streaming_context_.rolled_back() == false)
    {
        // We must set rolled_back id before stopping streaming client
        // or converting to applier. Accessing server_state requires
        // releasing the client_state lock in order to avoid violating
        // locking order, and this will open up a possibility for two
        // threads accessing this block simultaneously.
        streaming_context_.rolled_back(id_);
        if (bf_aborted_in_total_order_)
        {
            lock.unlock();
            client_state_.server_state_.stop_streaming_client(&client_state_);
            lock.lock();
        }
        else
        {
            // Create a high priority applier which will handle the
            // rollback fragment or clean up on configuration change.
            // Adopt transaction will copy fragment set and appropriate
            // meta data. Mark current transaction streaming context
            // rolled back.
            lock.unlock();
            client_state_.server_state_.convert_streaming_client_to_applier(
                &client_state_);
            lock.lock();
            streaming_context_.cleanup();
            // Cleanup cleans rolled_back_for from streaming context, but
            // we want to preserve it to avoid executing this block
            // more than once.
            streaming_context_.rolled_back(id_);
            enum wsrep::provider::status ret;
            if ((ret = provider().rollback(id_)))
            {
                wsrep::log_debug()
                    << "Failed to replicate rollback fragment for "
                    << id_ << ": " << ret;
            }
        }
    }
    debug_log_state("streaming_rollback leave");
}

int wsrep::transaction::replay(wsrep::unique_lock<wsrep::mutex>& lock)
{
    int ret(0);
    state(lock, s_replaying);
    // Need to remember streaming state before replay, entering
    // after_commit() after succesful replay will clear
    // fragments.
    const bool was_streaming(is_streaming());
    lock.unlock();
    client_service_.debug_sync("wsrep_before_replay");
    enum wsrep::provider::status replay_ret(client_service_.replay());
    client_service_.signal_replayed();
    if (was_streaming)
    {
        client_state_.server_state_.stop_streaming_client(&client_state_);
    }
    lock.lock();
    switch (replay_ret)
    {
    case wsrep::provider::success:
        if (state() == s_replaying)
        {
            // Replay was done by using different client state, adjust state
            // to committed.
            state(lock, s_committed);
        }
        if (is_streaming())
        {
            clear_fragments();
        }
        provider().release(ws_handle_);
        break;
    case wsrep::provider::error_certification_failed:
        client_state_.override_error(
            wsrep::e_deadlock_error);
        if (is_streaming())
        {
            client_service_.remove_fragments();
            clear_fragments();
        }
        state(lock, s_aborted);
        ret = 1;
        break;
    default:
        client_service_.emergency_shutdown();
        break;
    }

    WSREP_LOG_DEBUG(client_state_.debug_log_level(),
                    wsrep::log::debug_level_transaction,
                    "replay returned" << replay_ret);
    return ret;
}

void wsrep::transaction::clear_fragments()
{
    streaming_context_.cleanup();
}

void wsrep::transaction::cleanup()
{
    debug_log_state("cleanup_enter");
    assert(is_streaming() == false);
    assert(state() == s_committed || state() == s_aborted);
    id_ = wsrep::transaction_id::undefined();
    ws_handle_ = wsrep::ws_handle();
    // Keep the state history for troubleshooting. Reset
    // at start_transaction().
    // state_hist_.clear();
    if (ordered())
    {
        client_state_.update_last_written_gtid(ws_meta_.gtid());
    }
    bf_abort_state_ = s_executing;
    bf_abort_provider_status_ = wsrep::provider::success;
    bf_abort_client_state_ = 0;
    bf_aborted_in_total_order_ = false;
    ws_meta_ = wsrep::ws_meta();
    flags_ = 0;
    certified_ = false;
    pa_unsafe_ = false;
    implicit_deps_ = false;
    sr_keys_.clear();
    streaming_context_.cleanup();
    client_service_.cleanup_transaction();
    apply_error_buf_.clear();
    xid_.clear();
    debug_log_state("cleanup_leave");
}

void wsrep::transaction::debug_log_state(
    const char* context) const
{
    WSREP_LOG_DEBUG(
        client_state_.debug_log_level(),
        wsrep::log::debug_level_transaction,
        context
        << "\n    server: " << server_id_
        << ", client: " << int64_t(client_state_.id().get())
        << ", state: " << wsrep::to_c_string(client_state_.state())
        << ", mode: " << wsrep::to_c_string(client_state_.mode())
        << "\n    trx_id: " << int64_t(id_.get())
        << ", seqno: " << ws_meta_.seqno().get()
        << ", flags: " << flags()
        << "\n"
        << "    state: " << wsrep::to_c_string(state_)
        << ", bfa_state: " << wsrep::to_c_string(bf_abort_state_)
        << ", error: " << wsrep::to_c_string(client_state_.current_error())
        << ", status: " << client_state_.current_error_status()
        << "\n"
        << "    is_sr: " << is_streaming()
        << ", frags: " << streaming_context_.fragments_certified()
        << ", frags size: " << streaming_context_.fragments().size()
        << ", unit: " << streaming_context_.fragment_unit()
        << ", size: " << streaming_context_.fragment_size()
        << ", counter: " << streaming_context_.unit_counter()
        << ", log_pos: " << streaming_context_.log_position()
        << ", sr_rb: " << streaming_context_.rolled_back()
        << "\n    own: " << (client_state_.owning_thread_id_ == wsrep::this_thread::get_id())
        << " thread_id: " << client_state_.owning_thread_id_
        << "");
}

void wsrep::transaction::debug_log_key_append(const wsrep::key& key) const
{
    WSREP_LOG_DEBUG(client_state_.debug_log_level(),
                    wsrep::log::debug_level_transaction,
                    "key_append: "
                    << "trx_id: "
                    << int64_t(id().get())
                    << " append key:\n" << key);
}
