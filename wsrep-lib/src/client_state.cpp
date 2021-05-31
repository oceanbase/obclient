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

#include "wsrep/client_state.hpp"
#include "wsrep/compiler.hpp"
#include "wsrep/logger.hpp"

#include <unistd.h> // usleep()
#include <sstream>
#include <iostream>

wsrep::provider& wsrep::client_state::provider() const
{
    return server_state_.provider();
}

void wsrep::client_state::open(wsrep::client_id id)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    assert(state_ == s_none);
    assert(keep_command_error_ == false);
    debug_log_state("open: enter");
    owning_thread_id_ = wsrep::this_thread::get_id();
    rollbacker_active_ = false;
    sync_wait_gtid_ = wsrep::gtid::undefined();
    last_written_gtid_ = wsrep::gtid::undefined();
    state(lock, s_idle);
    id_ = id;
    debug_log_state("open: leave");
}

void wsrep::client_state::close()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    debug_log_state("close: enter");
    state(lock, s_quitting);
    keep_command_error_ = false;
    lock.unlock();
    if (transaction_.active() &&
        (mode_ != m_local ||
         transaction_.state() != wsrep::transaction::s_prepared))
    {
        client_service_.bf_rollback();
        transaction_.after_statement();
    }
    if (mode_ == m_local)
    {
        disable_streaming();
    }
    debug_log_state("close: leave");
}

void wsrep::client_state::cleanup()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    cleanup(lock);
}

void wsrep::client_state::cleanup(wsrep::unique_lock<wsrep::mutex>& lock)
{
    debug_log_state("cleanup: enter");
    state(lock, s_none);
    debug_log_state("cleanup: leave");
}

void wsrep::client_state::override_error(enum wsrep::client_error error,
                                         enum wsrep::provider::status status)
{
    assert(wsrep::this_thread::get_id() == owning_thread_id_);
    // Error state should not be cleared with success code without
    // explicit reset_error() call.
    assert(current_error_ == wsrep::e_success ||
           error != wsrep::e_success);
    current_error_ = error;
    current_error_status_ = status;
}

int wsrep::client_state::before_command(bool keep_command_error)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    debug_log_state("before_command: enter");
    // If the state is s_exec, the processing thread has already grabbed
    // control with wait_rollback_complete_and_acquire_ownership()
    if (state_ != s_exec)
    {
        assert(state_ == s_idle);
        do_wait_rollback_complete_and_acquire_ownership(lock);
        assert(state_ == s_exec);
        client_service_.store_globals();
    }
    else
    {
        // This thread must have acquired control by other means,
        // for example via wait_rollback_complete_and_acquire_ownership().
        assert(wsrep::this_thread::get_id() == owning_thread_id_);
    }

    keep_command_error_ = keep_command_error;

    // If the transaction is active, it must be either executing,
    // aborted as rolled back by rollbacker, or must_abort if the
    // client thread gained control via
    // wait_rollback_complete_and_acquire_ownership()
    // just before BF abort happened.
    assert(transaction_.active() == false ||
           (transaction_.state() == wsrep::transaction::s_executing ||
            transaction_.state() == wsrep::transaction::s_prepared ||
            transaction_.state() == wsrep::transaction::s_aborted ||
            transaction_.state() == wsrep::transaction::s_must_abort));

    if (transaction_.active())
    {
        if (transaction_.state() == wsrep::transaction::s_must_abort ||
            transaction_.state() == wsrep::transaction::s_aborted)
        {
            if (transaction_.is_xa())
            {
                // Client will rollback explicitly, return error.
                debug_log_state("before_command: error");
                return 1;
            }

            override_error(wsrep::e_deadlock_error);
            if (transaction_.state() == wsrep::transaction::s_must_abort)
            {
                lock.unlock();
                client_service_.bf_rollback();
                lock.lock();

            }

            if (keep_command_error_)
            {
                // Keep the error for the next command
                debug_log_state("before_command: keep error");
                return 0;
            }

            // Clean up the transaction and return error.
            lock.unlock();
            (void)transaction_.after_statement();
            lock.lock();

            assert(transaction_.active() == false);
            assert(transaction_.state() == wsrep::transaction::s_aborted);
            assert(current_error() != wsrep::e_success);

            debug_log_state("before_command: error");
            return 1;
        }
    }
    debug_log_state("before_command: success");
    return 0;
}

void wsrep::client_state::after_command_before_result()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    debug_log_state("after_command_before_result: enter");
    assert(state() == s_exec);
    if (transaction_.active() &&
        transaction_.state() == wsrep::transaction::s_must_abort)
    {
        override_error(wsrep::e_deadlock_error);
        lock.unlock();
        client_service_.bf_rollback();
        // If keep current error is set, the result will be propagated
        // back to client with some future command, so keep the transaction
        // open here so that error handling can happen in before_command()
        // hook.
        if (not keep_command_error_)
        {
            (void)transaction_.after_statement();
        }
        lock.lock();
        assert(transaction_.state() == wsrep::transaction::s_aborted);
        assert(current_error() != wsrep::e_success);
    }
    state(lock, s_result);
    debug_log_state("after_command_before_result: leave");
}

void wsrep::client_state::after_command_after_result()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    debug_log_state("after_command_after_result_enter");
    assert(state() == s_result);
    assert(transaction_.state() != wsrep::transaction::s_aborting);
    if (transaction_.active() &&
        transaction_.state() == wsrep::transaction::s_must_abort)
    {
        lock.unlock();
        client_service_.bf_rollback();
        lock.lock();
        assert(transaction_.state() == wsrep::transaction::s_aborted);
        override_error(wsrep::e_deadlock_error);
    }
    else if (transaction_.active() == false && not keep_command_error_)
    {
        current_error_ = wsrep::e_success;
        current_error_status_ = wsrep::provider::success;
    }
    keep_command_error_ = false;
    sync_wait_gtid_ = wsrep::gtid::undefined();
    state(lock, s_idle);
    debug_log_state("after_command_after_result: leave");
}

int wsrep::client_state::before_statement()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    debug_log_state("before_statement: enter");
#if 0
    /**
     * @todo It might be beneficial to implement timed wait for
     *       server synced state.
     */
    if (allow_dirty_reads_ == false &&
        server_state_.state() != wsrep::server_state::s_synced)
    {
        return 1;
    }
#endif // 0

    if (transaction_.active() &&
        transaction_.state() == wsrep::transaction::s_must_abort)
    {
        // Rollback and cleanup will happen in after_command_before_result()
        debug_log_state("before_statement_error");
        return 1;
    }
    debug_log_state("before_statement: success");
    return 0;
}

int wsrep::client_state::after_statement()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    debug_log_state("after_statement: enter");
    assert(state() == s_exec);
    assert(mode() == m_local);

    if (transaction_.active() &&
        transaction_.state() == wsrep::transaction::s_must_abort)
    {
        lock.unlock();
        client_service_.bf_rollback();
        lock.lock();
        assert(transaction_.state() == wsrep::transaction::s_aborted);
        // Error may be set already. For example, if fragment size
        // exceeded the maximum size in certify_fragment(), then
        // we already have wsrep::e_error_during_commit
        if (current_error() == wsrep::e_success)
        {
            override_error(wsrep::e_deadlock_error);
        }
    }
    lock.unlock();

    (void)transaction_.after_statement();
    if (current_error() == wsrep::e_deadlock_error)
    {
        if (mode_ == m_local)
        {
            debug_log_state("after_statement: may_retry");
            return 1;
        }
        else
        {
            debug_log_state("after_statement: error");
            return 1;
        }
    }
    debug_log_state("after_statement: success");
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
//                      Rollbacker synchronization                          //
//////////////////////////////////////////////////////////////////////////////

void wsrep::client_state::sync_rollback_complete()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    debug_log_state("sync_rollback_complete: enter");
    assert(state_ == s_idle && mode_ == m_local &&
           transaction_.state() == wsrep::transaction::s_aborted);
    set_rollbacker_active(false);
    cond_.notify_all();
    debug_log_state("sync_rollback_complete: leave");
}

void wsrep::client_state::wait_rollback_complete_and_acquire_ownership()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    debug_log_state("wait_rollback_complete_and_acquire_ownership: enter");
    if (state_ == s_idle)
    {
        do_wait_rollback_complete_and_acquire_ownership(lock);
    }
    assert(state_ == s_exec);
    debug_log_state("wait_rollback_complete_and_acquire_ownership: leave");
}

//////////////////////////////////////////////////////////////////////////////
//                             Streaming                                    //
//////////////////////////////////////////////////////////////////////////////

void wsrep::client_state::streaming_params(
    enum wsrep::streaming_context::fragment_unit fragment_unit,
    size_t fragment_size)
{
    assert(mode_ == m_local);
    transaction_.streaming_context().params(fragment_unit, fragment_size);
}

int wsrep::client_state::enable_streaming(
    enum wsrep::streaming_context::fragment_unit
    fragment_unit,
    size_t fragment_size)
{
    assert(mode_ == m_local);
    if (transaction_.is_streaming() &&
        transaction_.streaming_context().fragment_unit() !=
        fragment_unit)
    {
        wsrep::log_error()
            << "Changing fragment unit for active streaming transaction "
            << "not allowed";
        return 1;
    }
    transaction_.streaming_context().enable(fragment_unit, fragment_size);
    return 0;
}

void wsrep::client_state::disable_streaming()
{
    assert(mode_ == m_local);
    assert(state_ == s_exec || state_ == s_quitting);
    transaction_.streaming_context().disable();
}

//////////////////////////////////////////////////////////////////////////////
//                                 TOI                                      //
//////////////////////////////////////////////////////////////////////////////

enum wsrep::provider::status
wsrep::client_state::poll_enter_toi(
    wsrep::unique_lock<wsrep::mutex>& lock,
    const wsrep::key_array& keys,
    const wsrep::const_buffer& buffer,
    wsrep::ws_meta& meta,
    int flags,
    std::chrono::time_point<wsrep::clock> wait_until,
    bool& timed_out)
{
    WSREP_LOG_DEBUG(debug_log_level(),
                    wsrep::log::debug_level_client_state,
                    "poll_enter_toi: "
                    << flags
                    << ","
                    << wait_until.time_since_epoch().count());
    enum wsrep::provider::status status;
    timed_out = false;
    wsrep::ws_meta poll_meta; // tmp var for polling, as enter_toi may clear meta arg on errors
    do
    {
        lock.unlock();
        poll_meta = meta;
        status = provider().enter_toi(id_, keys, buffer, poll_meta, flags);
        if (status != wsrep::provider::success &&
            not poll_meta.gtid().is_undefined())
        {
            // Successfully entered TOI, but the provider reported failure.
            // This may happen for example if certification fails.
            // Leave TOI before proceeding.
            if (provider().leave_toi(id_, wsrep::mutable_buffer()))
            {
                wsrep::log_warning()
                    << "Failed to leave TOI after failure in "
                    << "poll_enter_toi()";
            }
            poll_meta = wsrep::ws_meta();
        }
        if (status == wsrep::provider::error_certification_failed ||
            status == wsrep::provider::error_connection_failed)
        {
            ::usleep(300000);
        }
        lock.lock();
        timed_out = !(wait_until.time_since_epoch().count() &&
                      wsrep::clock::now() < wait_until);
    }
    while ((status == wsrep::provider::error_certification_failed ||
            status == wsrep::provider::error_connection_failed) &&
           not timed_out &&
           not client_service_.interrupted(lock));
    meta = poll_meta;
    return status;
}

void wsrep::client_state::enter_toi_common(
    wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());
    toi_mode_ = mode_;
    mode(lock, m_toi);
}

int wsrep::client_state::enter_toi_local(const wsrep::key_array& keys,
                                         const wsrep::const_buffer& buffer,
                                         std::chrono::time_point<wsrep::clock> wait_until)
{
    debug_log_state("enter_toi_local: enter");
    assert(state_ == s_exec);
    assert(mode_ == m_local);
    int ret;

    wsrep::unique_lock<wsrep::mutex> lock(mutex_);

    bool timed_out;
    auto const status(poll_enter_toi(
                          lock, keys, buffer,
                          toi_meta_,
                          wsrep::provider::flag::start_transaction |
                          wsrep::provider::flag::commit,
                          wait_until,
                          timed_out));
    switch (status)
    {
    case wsrep::provider::success:
    {
        enter_toi_common(lock);
        ret = 0;
        break;
    }
    case wsrep::provider::error_certification_failed:
        override_error(e_deadlock_error, status);
        ret = 1;
        break;
    default:
        if (timed_out) {
            override_error(e_timeout_error);
        } else {
            override_error(e_error_during_commit, status);
        }
        ret = 1;
        break;
    }

    debug_log_state("enter_toi_local: leave");
    return ret;
}

void wsrep::client_state::enter_toi_mode(const wsrep::ws_meta& ws_meta)
{
    debug_log_state("enter_toi_mode: enter");
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    assert(mode_ == m_high_priority);
    enter_toi_common(lock);
    toi_meta_ = ws_meta;
    debug_log_state("enter_toi_mode: leave");
}

void wsrep::client_state::leave_toi_common()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    mode(lock, toi_mode_);
    toi_mode_ = m_undefined;
    if (toi_meta_.gtid().is_undefined() == false)
    {
        update_last_written_gtid(toi_meta_.gtid());
    }
    toi_meta_ = wsrep::ws_meta();
}

int wsrep::client_state::leave_toi_local(const wsrep::mutable_buffer& err)
{
    debug_log_state("leave_toi_local: enter");
    assert(toi_mode_ == m_local);
    leave_toi_common();

    debug_log_state("leave_toi_local: leave");
    return (provider().leave_toi(id_, err) == provider::success ? 0 : 1);
}

void wsrep::client_state::leave_toi_mode()
{
    debug_log_state("leave_toi_mode: enter");
    assert(toi_mode_ == m_high_priority);
    leave_toi_common();
    debug_log_state("leave_toi_mode: leave");
}

///////////////////////////////////////////////////////////////////////////////
//                                 RSU                                       //
///////////////////////////////////////////////////////////////////////////////

int wsrep::client_state::begin_rsu(int timeout)
{
    if (server_state_.desync())
    {
        wsrep::log_warning() << "Failed to desync server";
        return 1;
    }
    if (server_state_.server_service().wait_committing_transactions(timeout))
    {
        wsrep::log_warning() << "RSU failed due to pending transactions";
        server_state_.resync();
        return 1;
    }
    wsrep::seqno pause_seqno(server_state_.pause());
    if (pause_seqno.is_undefined())
    {
        wsrep::log_warning() << "Failed to pause provider";
        server_state_.resync();
        return 1;
    }
    wsrep::log_info() << "Provider paused at: " << pause_seqno;
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    toi_mode_ = mode_;
    mode(lock, m_rsu);
    return 0;
}

int wsrep::client_state::end_rsu()
{
    int ret(0);
    try
    {
        server_state_.resume();
        server_state_.resync();
    }
    catch (const wsrep::runtime_error& e)
    {
        wsrep::log_warning() << "End RSU failed: " << e.what();
        ret = 1;
    }
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    mode(lock, toi_mode_);
    return ret;
}

///////////////////////////////////////////////////////////////////////////////
//                                 NBO                                       //
///////////////////////////////////////////////////////////////////////////////

int wsrep::client_state::begin_nbo_phase_one(
    const wsrep::key_array& keys,
    const wsrep::const_buffer& buffer,
    std::chrono::time_point<wsrep::clock> wait_until)
{
    debug_log_state("begin_nbo_phase_one: enter");
    debug_log_keys(keys);
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    assert(state_ == s_exec);
    assert(mode_ == m_local);
    assert(toi_mode_ == m_undefined);

    int ret;
    bool timed_out;
    auto const status(poll_enter_toi(
                          lock, keys, buffer,
                          toi_meta_,
                          wsrep::provider::flag::start_transaction,
                          wait_until,
                          timed_out));
    switch (status)
    {
    case wsrep::provider::success:
        toi_mode_ = mode_;
        mode(lock, m_nbo);
        ret= 0;
        break;
    case wsrep::provider::error_certification_failed:
        override_error(e_deadlock_error, status);
        ret = 1;
        break;
    default:
        if (timed_out) {
            override_error(e_timeout_error);
        } else {
            override_error(e_error_during_commit, status);
        }
        ret = 1;
        break;
    }

    debug_log_state("begin_nbo_phase_one: leave");
    return ret;
}

int wsrep::client_state::end_nbo_phase_one(const wsrep::mutable_buffer& err)
{
    debug_log_state("end_nbo_phase_one: enter");
    assert(state_ == s_exec);
    assert(mode_ == m_nbo);
    assert(in_toi());

    enum wsrep::provider::status status(provider().leave_toi(id_, err));
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    int ret;
    switch (status)
    {
    case wsrep::provider::success:
        ret = 0;
        break;
    default:
        override_error(e_error_during_commit, status);
        ret = 1;
        break;
    }
    nbo_meta_ = toi_meta_;
    toi_meta_ = wsrep::ws_meta();
    toi_mode_ = m_undefined;
    debug_log_state("end_nbo_phase_one: leave");
    return ret;
}

int wsrep::client_state::enter_nbo_mode(const wsrep::ws_meta& ws_meta)
{
    assert(state_ == s_exec);
    assert(mode_ == m_local);
    assert(toi_mode_ == m_undefined);
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    nbo_meta_ = ws_meta;
    mode(lock, m_nbo);
    return 0;
}

int wsrep::client_state::begin_nbo_phase_two(
    const wsrep::key_array& keys,
    std::chrono::time_point<wsrep::clock> wait_until)
{
    debug_log_state("begin_nbo_phase_two: enter");
    debug_log_keys(keys);
    assert(state_ == s_exec);
    assert(mode_ == m_nbo);
    assert(toi_mode_ == m_undefined);
    assert(!in_toi());

    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    // Note: nbo_meta_ is passed to enter_toi() as it is
    // an input param containing gtid of NBO begin.
    // Output stored in nbo_meta_ is copied to toi_meta_ for
    // phase two end.
    bool timed_out;
    enum wsrep::provider::status status(
        poll_enter_toi(lock, keys,
                       wsrep::const_buffer(),
                       nbo_meta_,
                       wsrep::provider::flag::commit,
                       wait_until,
                       timed_out));
    int ret;
    switch (status)
    {
    case wsrep::provider::success:
        ret= 0;
        toi_meta_ = nbo_meta_;
        toi_mode_ = m_local;
        break;
    case wsrep::provider::error_provider_failed:
        override_error(e_interrupted_error, status);
        ret= 1;
        break;
    default:
        if (timed_out)
        {
            override_error(e_timeout_error, status);
        }
        else
        {
            override_error(e_error_during_commit, status);
        }
        ret= 1;
        break;
    }

    // Failed to grab TOI for completing NBO in order. This means that
    // the operation cannot be ended in total order, so we end the
    // NBO mode and let the DBMS to deal with the error.
    if (ret)
    {
        mode(lock, m_local);
        nbo_meta_ = wsrep::ws_meta();
    }

    debug_log_state("begin_nbo_phase_two: leave");
    return ret;
}

int wsrep::client_state::end_nbo_phase_two(const wsrep::mutable_buffer& err)
{
    debug_log_state("end_nbo_phase_two: enter");
    assert(state_ == s_exec);
    assert(mode_ == m_nbo);
    assert(toi_mode_ == m_local);
    assert(in_toi());
    enum wsrep::provider::status status(
        provider().leave_toi(id_, err));
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    int ret;
    switch (status)
    {
    case wsrep::provider::success:
        ret = 0;
        break;
    default:
        override_error(e_error_during_commit, status);
        ret = 1;
        break;
    }
    toi_meta_ = wsrep::ws_meta();
    toi_mode_ = m_undefined;
    nbo_meta_ = wsrep::ws_meta();
    mode(lock, m_local);
    debug_log_state("end_nbo_phase_two: leave");
    return ret;
}
///////////////////////////////////////////////////////////////////////////////
//                                 Misc                                      //
///////////////////////////////////////////////////////////////////////////////

int wsrep::client_state::sync_wait(int timeout)
{
    std::pair<wsrep::gtid, enum wsrep::provider::status> result(
        server_state_.causal_read(timeout));
    int ret(1);
    switch (result.second)
    {
    case wsrep::provider::success:
        sync_wait_gtid_ = result.first;
        ret = 0;
        break;
    case wsrep::provider::error_not_implemented:
        override_error(wsrep::e_not_supported_error);
        break;
    default:
        override_error(wsrep::e_timeout_error);
        break;
    }
    return ret;
}

///////////////////////////////////////////////////////////////////////////////
//                               Private                                     //
///////////////////////////////////////////////////////////////////////////////

void wsrep::client_state::do_acquire_ownership(
    wsrep::unique_lock<wsrep::mutex>& lock WSREP_UNUSED)
{
    assert(lock.owns_lock());
    // Be strict about client state for clients in local mode. The
    // owning_thread_id_ is used to detect bugs which are caused by
    // more than one thread operating the client state at the time,
    // for example thread handling the client session and background
    // rollbacker.
    assert(state_ == s_idle || mode_ != m_local);
    owning_thread_id_ = wsrep::this_thread::get_id();
}

void wsrep::client_state::do_wait_rollback_complete_and_acquire_ownership(
    wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());
    assert(state_ == s_idle);
    while (is_rollbacker_active())
    {
        cond_.wait(lock);
    }
    do_acquire_ownership(lock);
    state(lock, s_exec);
}

void wsrep::client_state::update_last_written_gtid(const wsrep::gtid& gtid)
{
    assert(last_written_gtid_.is_undefined() ||
           (last_written_gtid_.id() == gtid.id() &&
            !(last_written_gtid_.seqno() > gtid.seqno())));
    last_written_gtid_ = gtid;
}

void wsrep::client_state::debug_log_state(const char* context) const
{
    WSREP_LOG_DEBUG(debug_log_level(),
                    wsrep::log::debug_level_client_state,
                    context
                    << "(" << id_.get()
                    << "," << to_c_string(state_)
                    << "," << to_c_string(mode_)
                    << "," << wsrep::to_string(current_error_)
                    << "," << current_error_status_
                    << ",toi: " << toi_meta_.seqno()
                    << ",nbo: " << nbo_meta_.seqno() << ")");
}

void wsrep::client_state::debug_log_keys(const wsrep::key_array& keys) const
{
    for (size_t i(0); i < keys.size(); ++i)
    {
        WSREP_LOG_DEBUG(debug_log_level(),
                        wsrep::log::debug_level_client_state,
                        "TOI keys: "
                        << " id: " << id_
                        << "key: " << keys[i]);
    }
}

void wsrep::client_state::state(
    wsrep::unique_lock<wsrep::mutex>& lock WSREP_UNUSED,
    enum wsrep::client_state::state state)
{
    // Verify that the current thread has gained control to the
    // connection by calling before_command()
    assert(wsrep::this_thread::get_id() == owning_thread_id_);
    assert(lock.owns_lock());
    static const char allowed[state_max_][state_max_] =
        {
            /* none idle exec result quit */
            {  0,   1,   0,   0,     0}, /* none */
            {  0,   0,   1,   0,     1}, /* idle */
            {  0,   0,   0,   1,     0}, /* exec */
            {  0,   1,   0,   0,     0}, /* result */
            {  1,   0,   0,   0,     0}  /* quit */
        };
    if (!allowed[state_][state])
    {
        wsrep::log_warning() << "client_state: Unallowed state transition: "
                             << state_ << " -> " << state;
        assert(0);
    }
    state_hist_.push_back(state_);
    state_ = state;
    if (state_hist_.size() > 10)
    {
        state_hist_.erase(state_hist_.begin());
    }

}

void wsrep::client_state::mode(
    wsrep::unique_lock<wsrep::mutex>& lock WSREP_UNUSED,
    enum mode mode)
{
    assert(lock.owns_lock());

    static const char allowed[n_modes_][n_modes_] =
        {   /* u  l  h  t  r  n */
            {  0, 0, 0, 0, 0, 0 }, /* undefined */
            {  0, 0, 1, 1, 1, 1 }, /* local */
            {  0, 1, 0, 1, 0, 1 }, /* high prio */
            {  0, 1, 1, 0, 0, 0 }, /* toi */
            {  0, 1, 0, 0, 0, 0 }, /* rsu */
            {  0, 1, 1, 0, 0, 0 }  /* nbo */
        };
    if (!allowed[mode_][mode])
    {
        wsrep::log_warning() << "client_state: Unallowed mode transition: "
                             << mode_ << " -> " << mode;
        assert(0);
    }
    mode_ = mode;
}
