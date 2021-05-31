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

/** @file client_state.hpp
 *
 *
 * Return value conventions:
 *
 * The calls which may alter either client_state or associated
 * transaction state will generally return zero on success and
 * non-zero on failure. More detailed error information is stored
 * into client state and persisted there until explicitly cleared.
 */

#ifndef WSREP_CLIENT_STATE_HPP
#define WSREP_CLIENT_STATE_HPP

#include "server_state.hpp"
#include "server_service.hpp"
#include "provider.hpp"
#include "transaction.hpp"
#include "client_id.hpp"
#include "client_service.hpp"
#include "mutex.hpp"
#include "lock.hpp"
#include "buffer.hpp"
#include "thread.hpp"
#include "xid.hpp"
#include "chrono.hpp"

namespace wsrep
{
    class server_state;
    class provider;

    enum client_error
    {
        e_success,
        e_error_during_commit,
        e_deadlock_error,
        e_interrupted_error,
        e_size_exceeded_error,
        e_append_fragment_error,
        e_not_supported_error,
        e_timeout_error
    };

    static inline const char* to_c_string(enum client_error error)
    {
        switch (error)
        {
        case e_success:               return "success";
        case e_error_during_commit:   return "commit_error";
        case e_deadlock_error:        return "deadlock_error";
        case e_interrupted_error:     return "interrupted_error";
        case e_size_exceeded_error:   return "size_exceeded_error";
        case e_append_fragment_error: return "append_fragment_error";
        case e_not_supported_error:   return "not_supported_error";
        case e_timeout_error:         return "timeout_error";
        }
        return "unknown";
    }

    static inline std::string to_string(enum client_error error)
    {
        return to_c_string(error);
    }
    /**
     * Client State
     */
    class client_state
    {
    public:
        /**
         * Client mode enumeration.
         */
        enum mode
        {
            /** undefined mode */
            m_undefined,
            /** Locally operating client session. */
            m_local,
            /** High priority mode */
            m_high_priority,
            /** Client is in total order isolation mode */
            m_toi,
            /** Client is executing rolling schema upgrade */
            m_rsu,
            /** Client is executing NBO */
            m_nbo
        };

        static const int n_modes_ = m_nbo + 1;
        /**
         * Client state enumeration.
         *
         */
        enum state
        {
            /**
             * Client session has not been initialized yet.
             */
            s_none,
            /**
             * Client is idle, the control is in the application which
             * uses the DBMS system.
             */
            s_idle,
            /**
             * The control of the client processing is inside the DBMS
             * system.
             */
            s_exec,
            /**
             * Client handler is sending result to client.
             */
            s_result,
            /**
             * The client session is terminating.
             */
            s_quitting
        };

        static const int state_max_ = s_quitting + 1;

        /**
         * Aqcuire ownership on the thread.
         *
         * This method should be called every time the thread
         * operating the client state changes. This method is called
         * implicitly from before_command() and
         * wait_rollback_complete_and_acquire_ownership().
         */
        void acquire_ownership()
        {
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
            do_acquire_ownership(lock);
        }

        /**
         * @deprecated Use acquire_ownership() instead.
         */
        void store_globals()
        {
            acquire_ownership();
        }

        /**
         * Destructor.
         */
        virtual ~client_state()
        {
            assert(transaction_.active() == false);
        }

        /** @name Client session handling */
        /** @{ */
        /**
         * This method should be called when opening the client session.
         *
         * Initializes client id and changes the state to s_idle.
         */
        void open(wsrep::client_id);

        /**
         * This method should be called before closing the client session.
         *
         * The state is changed to s_quitting and any open transactions
         * are rolled back.
         */
        void close();

        /**
         * This method should be called after closing the client session
         * to clean up.
         *
         * The state is changed to s_none.
         */
        void cleanup();

        /**
         * Overload of cleanup() method which takes lock as argument.
         * This method does not release the lock during execution, but
         * the lock is needed for debug build sanity checks.
         */
        void cleanup(wsrep::unique_lock<wsrep::mutex>& lock);
        /** @} */

        /** @name Client command handling */
        /** @{ */
        /**
         * This mehod should be called before the processing of command
         * received from DBMS client starts.
         *
         * This method will wait until the possible synchronous
         * rollback for associated transaction has finished unless
         * wait_rollback_complete_and_acquire_ownership() has been
         * called before.
         *
         * The method has a side effect of changing the client
         * context state to executing.
         *
         * The value set by keep_command_error has an effect on
         * how before_command() behaves when it is entered after
         * background rollback has been processed:
         *
         * - If keep_command_error is set true, the current error
         *   is set and success will be returned.
         * - If keep_command_error is set false, the transaction is
         *   cleaned up and the return value will be non-zero to
         *   indicate error.
         *
         * @param keep_command_error Make client state to preserve error
         *        state in command hooks.
         *        This is needed if a current command is not supposed to
         *        return an error status to the client and the protocol must
         *        advance until the next client command to return error status.
         *
         * @return Zero in case of success, non-zero in case of the
         *         associated transaction was BF aborted.
         */
        int before_command(bool keep_command_error);

        int before_command()
        {
            return before_command(false);
        }

        /**
         * This method should be called before returning
         * a result to DBMS client.
         *
         * The method will check if the transaction associated to
         * the connection has been aborted. Rollback is performed
         * if needed. After the call, current_error() will return an error
         * code associated to the client state. If the error code is
         * not success, the transaction associated to the client state
         * has been aborted and rolled back.
         */
        void after_command_before_result();

        /**
         * Method which should be called after returning the
         * control back to DBMS client..
         *
         * The method will do the check if the transaction associated
         * to the connection has been aborted. If so, rollback is
         * performed and the transaction is left to aborted state.
         * The next call to before_command() will return an error and
         * the error state can be examined after after_command_before_resul()
         * is called.
         *
         * This method has a side effect of changing state to
         * idle.
         */
        void after_command_after_result();
        /** @} */

        /** @name Statement level operations */
        /** @{ */
        /**
         * Before statement execution operations.
         *
         * Check if server is synced and if dirty reads are allowed.
         *
         * @return Zero in case of success, non-zero if the statement
         *         is not allowed to be executed due to read or write
         *         isolation requirements.
         */
        int before_statement();

        /**
         * After statement execution operations.
         *
         * * Check for must_replay state
         * * Do rollback if requested
         */
        int after_statement();
        /** @} */

        /**
         * Perform cleanup after applying a transaction.
         *
         * @param err Applying error (empty for no error)
         */
        void after_applying()
        {
            assert(mode_ == m_high_priority);
            transaction_.after_applying();
        }

        /** @name Replication interface */
        /** @{ */
        /**
         * Start a new transaction with a transaction id.
         *
         * @todo This method should
         *       - Register the transaction on server level for bookkeeping
         *       - Isolation levels? Or part of the transaction?
         */
        int start_transaction(const wsrep::transaction_id& id)
        {
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
            assert(state_ == s_exec);
            return transaction_.start_transaction(id);
        }

        /**
         * Establish read view ID of the transaction.
         *
         * This method should be preferably called immediately before any
         * first read or write operation in the transaction is performed,
         * Then it can be called with default NULL parameter and will use
         * the current last committed GTID.
         * Alternatively it can be called at any time before commit with an
         * explicit GTID that corresponds to transaction read view.
         *
         * @param gtid optional explicit GTID of the transaction read view.
         */
        int assign_read_view(const wsrep::gtid* const gtid = NULL)
        {
            assert(mode_ == m_local);
            assert(state_ == s_exec);
            return transaction_.assign_read_view(gtid);
        }

        /**
         * Append a key into transaction write set.
         *
         * @param key Key to be appended
         *
         * @return Zero on success, non-zero on failure.
         */
        int append_key(const wsrep::key& key)
        {
            assert(mode_ == m_local);
            assert(state_ == s_exec);
            return transaction_.append_key(key);
        }

        /**
         * Append keys in key_array into transaction write set.
         *
         * @param keys Array of keys to be appended
         *
         * @return Zero in case of success, non-zero on failure.
         */
        int append_keys(const wsrep::key_array& keys)
        {
            assert(mode_ == m_local || mode_ == m_toi);
            assert(state_ == s_exec);
            for (auto i(keys.begin()); i != keys.end(); ++i)
            {
                if (transaction_.append_key(*i))
                {
                    return 1;
                }
            }
            return 0;
        }

        /**
         * Append data into transaction write set.
         */
        int append_data(const wsrep::const_buffer& data)
        {
            assert(mode_ == m_local);
            assert(state_ == s_exec);
            return transaction_.append_data(data);
        }

        /** @} */

        /** @name Streaming replication interface */
        /** @{ */
        /**
         * This method should be called after every row operation.
         */
        int after_row()
        {
            assert(mode_ == m_local);
            assert(state_ == s_exec);
            return (transaction_.streaming_context().fragment_size() ?
                    transaction_.after_row() : 0);
        }

        /**
         * Set streaming parameters.
         *
         * @param fragment_unit Desired fragment unit
         * @param fragment_size Desired fragment size
         */
        void streaming_params(enum wsrep::streaming_context::fragment_unit
                              fragment_unit,
                              size_t fragment_size);

        /**
         * Enable streaming replication.
         *
         * Currently it is not possible to change the fragment unit
         * for active streaming transaction.
         *
         * @param fragment_unit Desired fragment unit
         * @param fragment_size Desired fragment size
         *
         * @return Zero on success, non-zero if the streaming cannot be
         *         enabled.
         */
        int enable_streaming(
            enum wsrep::streaming_context::fragment_unit
            fragment_unit,
            size_t fragment_size);

        /**
         * Disable streaming for context.
         */
        void disable_streaming();

        void fragment_applied(wsrep::seqno seqno)
        {
            assert(mode_ == m_high_priority);
            transaction_.fragment_applied(seqno);
        }

        /**
         * Prepare write set meta data for ordering.
         * This method should be called before ordered commit or
         * rollback if the commit time meta data was not known
         * at the time of the start of the transaction.
         * This mostly applies to streaming replication.
         *
         * @param ws_handle Write set handle
         * @param ws_meta Write set meta data
         * @param is_commit Boolean to denote whether the operation
         *                  is commit or rollback.
         */
        int prepare_for_ordering(const wsrep::ws_handle& ws_handle,
                                 const wsrep::ws_meta& ws_meta,
                                 bool is_commit)
        {
            assert(state_ == s_exec);
            return transaction_.prepare_for_ordering(
                ws_handle, ws_meta, is_commit);
        }
        /** @} */

        /** @name Applying interface */
        /** @{ */
        int start_transaction(const wsrep::ws_handle& wsh,
                              const wsrep::ws_meta& meta)
        {
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
            assert(owning_thread_id_ == wsrep::this_thread::get_id());
            assert(mode_ == m_high_priority);
            return transaction_.start_transaction(wsh, meta);
        }

        int next_fragment(const wsrep::ws_meta& meta)
        {
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
            assert(mode_ == m_high_priority);
            return transaction_.next_fragment(meta);
        }

        /** @name Commit ordering interface */
        /** @{ */
        int before_prepare()
        {
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
            assert(owning_thread_id_ == wsrep::this_thread::get_id());
            assert(state_ == s_exec);
            return transaction_.before_prepare(lock);
        }

        int after_prepare()
        {
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
            assert(owning_thread_id_ == wsrep::this_thread::get_id());
            assert(state_ == s_exec);
            return transaction_.after_prepare(lock);
        }

        int before_commit()
        {
            assert(owning_thread_id_ == wsrep::this_thread::get_id());
            assert(state_ == s_exec || mode_ == m_local);
            return transaction_.before_commit();
        }

        int ordered_commit()
        {
            assert(owning_thread_id_ == wsrep::this_thread::get_id());
            assert(state_ == s_exec || mode_ == m_local);
            return transaction_.ordered_commit();
        }

        int after_commit()
        {
            assert(owning_thread_id_ == wsrep::this_thread::get_id());
            assert(state_ == s_exec || mode_ == m_local);
            return transaction_.after_commit();
        }
        /** @} */
        int before_rollback()
        {
            assert(owning_thread_id_ == wsrep::this_thread::get_id());
            assert(state_ == s_idle ||
                   state_ == s_exec ||
                   state_ == s_result ||
                   state_ == s_quitting);
            return transaction_.before_rollback();
        }

        int after_rollback()
        {
            assert(owning_thread_id_ == wsrep::this_thread::get_id());
            assert(state_ == s_idle ||
                   state_ == s_exec ||
                   state_ == s_result ||
                   state_ == s_quitting);
            return transaction_.after_rollback();
        }

        /**
         * This method should be called by the background rollbacker
         * thread after the rollback is complete. This will allow
         * the client to proceed through before_command() and
         * wait_rollback_complete_and_acquire_ownership().
         */
        void sync_rollback_complete();

        /**
         * Wait for background rollback to complete. This method can
         * be called before before_command() to verify that the
         * background rollback has been finished. After the call returns,
         * it is guaranteed that BF abort does not launch background
         * rollback process before after_command_after_result() is called.
         * This method is idempotent, it can be called many times
         * by the same thread before before_command() is called.
         */
        void wait_rollback_complete_and_acquire_ownership();
        /** @} */

        //
        // XA
        //
        /**
         * Assign transaction external id.
         *
         * Other than storing the xid, the transaction is marked as XA.
         * This should be called when XA transaction is started.
         *
         * @param xid transaction id
         */
        void assign_xid(const wsrep::xid& xid)
        {
            transaction_.assign_xid(xid);
        }

        /**
         * Restores the client's transaction to prepared state
         *
         * The purpose of this method is to restore transaction state
         * during recovery of a prepared XA transaction.
         */
        int restore_xid(const wsrep::xid& xid)
        {
            return transaction_.restore_to_prepared_state(xid);
        }

        /**
         * Commit transaction with the given xid
         *
         * Sends a commit fragment to terminate the transaction with
         * the given xid. For the fragment to be sent, a streaming
         * applier for the transaction must exist, and the transaction
         * must be in prepared state.
         *
         * @param xid the xid of the the transaction to commit
         *
         * @return Zero on success, non-zero on error. In case of error
         *         the client_state's current_error is set
         */
        int commit_by_xid(const wsrep::xid& xid)
        {
            return transaction_.commit_or_rollback_by_xid(xid, true);
        }

        /**
         * Rollback transaction with the given xid
         *
         * Sends a rollback fragment to terminate the transaction with
         * the given xid. For the fragment to be sent, a streaming
         * applier for the transaction must exist, and the transaction
         * must be in prepared state.
         *
         * @param xid the xid of the the transaction to commit
         *
         * @return Zero on success, non-zero on error. In case of error
         *         the client_state's current_error is set
         */
        int rollback_by_xid(const wsrep::xid& xid)
        {
            return transaction_.commit_or_rollback_by_xid(xid, false);
        }

        /**
         * Detach a prepared XA transaction
         *
         * This method cleans up a local XA transaction in prepared state
         * and converts it to high priority mode.
         * This can be used to handle the case where the client of a XA
         * transaction disconnects, and the transaction must not rollback.
         * After this call, a different client may later attempt to terminate
         * the transaction by calling method commit_by_xid() or rollback_by_xid().
         */
        void xa_detach()
        {
            assert(mode_ == m_local);
            assert(state_ == s_none || state_ == s_exec);
            transaction_.xa_detach();
        }

        /**
         * Replay a XA transaction
         *
         * Replay a XA transaction that is in s_idle state.
         * This may happen if the transaction is BF aborted
         * between prepare and commit.
         * Since the victim is idle, this method can be called
         * by the BF aborter or the backround rollbacker.
         */
        void xa_replay()
        {
            assert(mode_ == m_local);
            assert(state_ == s_idle);
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
            transaction_.xa_replay(lock);
        }

        //
        // BF aborting
        //
        /**
         * Brute force abort a transaction. This method should be
         * called by a transaction which needs to BF abort a conflicting
         * locally processing transaction.
         */
        int bf_abort(wsrep::seqno bf_seqno)
        {
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
            assert(mode_ == m_local || transaction_.is_streaming());
            return transaction_.bf_abort(lock, bf_seqno);
        }
        /**
         * Brute force abort a transaction in total order. This method
         * should be called by the TOI operation which needs to
         * BF abort a transaction.
         */
        int total_order_bf_abort(wsrep::seqno bf_seqno)
        {
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
            assert(mode_ == m_local || transaction_.is_streaming());
            return transaction_.total_order_bf_abort(lock, bf_seqno);
        }

        /**
         * Adopt a streaming transaction state. This is must be
         * called from high_priority_service::adopt_transaction()
         * during streaming transaction rollback. The call will
         * set up enough context for handling the rollback
         * fragment.
         */
        void adopt_transaction(const wsrep::transaction& transaction)
        {
            assert(mode_ == m_high_priority);
            transaction_.adopt(transaction);
        }

        /**
         * Adopt (store) transaction applying error for further processing.
         */
        void adopt_apply_error(wsrep::mutable_buffer& err)
        {
            assert(mode_ == m_high_priority);
            transaction_.adopt_apply_error(err);
        }

        /**
         * Clone enough state from another transaction so that replaing will
         * be possible with a transaction contained in this client state.
         *
         * @param transaction Transaction which is to be replied in this
         *                    client state
         */
        void clone_transaction_for_replay(const wsrep::transaction& transaction)
        {
            // assert(mode_ == m_high_priority);
            transaction_.clone_for_replay(transaction);
        }

        /** @name Non-transactional operations */
        /** @{*/

        /**
         * Enter total order isolation critical section. If the wait_until
         * is given non-default value, the operation is retried until
         * successful, the given time point is reached or the client is
         * interrupted.
         *
         * @param key_array Array of keys
         * @param buffer Buffer containing the action to execute inside
         *               total order isolation section
         * @param flags  Provider flags for TOI operation
         * @param wait_until Time point to wait until for successful
         *                   certification.
         *
         * @return Zero on success, non-zero otherwise.
         */
        int enter_toi_local(
            const wsrep::key_array& key_array,
            const wsrep::const_buffer& buffer,
            std::chrono::time_point<wsrep::clock>
            wait_until =
            std::chrono::time_point<wsrep::clock>());
        /**
         * Enter applier TOI mode
         *
         * @param ws_meta Write set meta data
         */
        void enter_toi_mode(const wsrep::ws_meta& ws_meta);

        /**
         * Return true if the client_state is under TOI operation.
         */
        bool in_toi() const
        {
            return (toi_meta_.seqno().is_undefined() == false);
        }

        /**
         * Return the mode where client entered into TOI mode.
         * The return value can be either m_local or
         * m_high_priority.
         */
        enum mode toi_mode() const
        {
            return toi_mode_;
        }

        /**
         * Leave total order isolation critical section.
         * (for local mode clients)
         *
         * @param err definition of the error that happened during the
         *            execution of TOI operation (empty for no error)
         */
        int leave_toi_local(const wsrep::mutable_buffer& err);

        /**
         * Leave applier TOI mode.
         */
        void leave_toi_mode();

        /**
         * Begin rolling schema upgrade operation.
         *
         * @param timeout Timeout in seconds to wait for committing
         *        connections to finish.
         */
        int begin_rsu(int timeout);

        /**
         * End rolling schema upgrade operation.
         */
        int end_rsu();

        /**
         * Begin non-blocking operation.
         *
         * The NBO operation is started by grabbing TOI critical
         * section. The keys and buffer are certifed as in TOI
         * operation. If the call fails due to error returned by
         * the provider, the provider error code can be retrieved
         * by current_error_status() call.
         *
         * If the wait_until is given non-default value, the operation is
         * retried until successful, the given time point is reached or the
         * client is interrupted.
         *
         * @param keys Array of keys for NBO operation.
         * @param buffer NBO write set
         * @param wait_until Time point to wait until for successful certification.
         * @return Zero in case of success, non-zero in case of failure.
         */
        int begin_nbo_phase_one(
            const wsrep::key_array& keys,
            const wsrep::const_buffer& buffer,
            std::chrono::time_point<wsrep::clock>
            wait_until =
            std::chrono::time_point<wsrep::clock>());

        /**
         * End non-blocking operation phase after aquiring required
         * resources for operation.
         *
         * @param err definition of the error that happened during the
         *            execution of phase one (empty for no error)
         */
        int end_nbo_phase_one(const wsrep::mutable_buffer& err);

        /**
         * Enter in NBO mode. This method should be called when the
         * applier launches the asynchronous process to perform the
         * operation. The purpose of the call is to adjust
         * the state and set write set meta data.
         *
         * @param ws_meta Write set meta data.
         *
         * @return Zero in case of success, non-zero on failure.
         */
        int enter_nbo_mode(const wsrep::ws_meta& ws_meta);

        /**
         * Begin non-blocking operation phase two. The keys argument
         * passed to this call must contain the same keys which were
         * passed to begin_nbo_phase_one().
         *
         * If the wait_until is given non-default value, the operation is
         * retried until successful, the given time point is reached or the
         * client is interrupted.
         *
         * @param keys Key array.
         * @param wait_until Time point to wait until for entering TOI for
         *                   phase two.
         */
        int begin_nbo_phase_two(const wsrep::key_array& keys,
                                std::chrono::time_point<wsrep::clock>
                                wait_until =
                                std::chrono::time_point<wsrep::clock>());

        /**
         * End non-blocking operation phase two. This call will
         * release TOI critical section and set the mode to m_local.
         *
         * @param err definition of the error that happened during the
         *            execution of phase two (empty for no error)
         */
        int end_nbo_phase_two(const wsrep::mutable_buffer& err);

        /**
         * Get reference to the client mutex.
         *
         * @return Reference to the client mutex.
         */
        wsrep::mutex& mutex() { return mutex_; }

        /**
         * Get server context associated the the client session.
         *
         * @return Reference to server context.
         */
        wsrep::server_state& server_state() const
        { return server_state_; }

        wsrep::client_service& client_service() const
        { return client_service_; }
        /**
         * Get reference to the Provider which is associated
         * with the client context.
         *
         * @return Reference to the provider.
         * @throw wsrep::runtime_error if no providers are associated
         *        with the client context.
         *
         * @todo Should be removed.
         */
        wsrep::provider& provider() const;

        /**
         * Get Client identifier.
         *
         * @return Client Identifier
         */
        client_id id() const { return id_; }

        /**
         * Get Client mode.
         *
         * @todo Enforce mutex protection if called from other threads.
         *
         * @return Client mode.
         */
        enum mode mode() const { return mode_; }

        /**
         * Get Client state.
         *
         * @todo Enforce mutex protection if called from other threads.
         *
         * @return Client state
         */
        enum state state() const { return state_; }

        /**
         * Return a const reference to the transaction associated
         * with the client state.
         */
        const wsrep::transaction& transaction() const
        {
            return transaction_;
        }

        const wsrep::ws_meta& toi_meta() const
        {
            return toi_meta_;
        }

        /**
         * Do sync wait operation. If the method fails, current_error()
         * can be inspected about the reason of error.
         *
         * @param Sync wait timeout in seconds.
         *
         * @return Zero on success, non-zero on error.
         */
        int sync_wait(int timeout);

        /**
         * Return the current sync wait GTID.
         *
         * Sync wait GTID is updated on each sync_wait() call and
         * reset to wsrep::gtid::undefined() in after_command_after_result()
         * method. The variable can thus be used to check if a sync wait
         * has been performend for the current client command.
         */
        const wsrep::gtid& sync_wait_gtid() const
        {
            return sync_wait_gtid_;
        }
        /**
         * Return the last written GTID.
         */
        const wsrep::gtid& last_written_gtid() const
        {
            return last_written_gtid_;
        }

        /**
         * Set debug logging level.
         *
         * Levels:
         * 0 - Debug logging is disabled
         * 1..n - Debug logging with increasing verbosity.
         */
        void debug_log_level(int level) { debug_log_level_ = level; }

        /**
         * Return current debug logging level. The return value
         * is a maximum of client state and server state debug log
         * levels.
         *
         * @return Current debug log level.
         */
        int debug_log_level() const
        {
            return std::max(debug_log_level_,
                            wsrep::log::debug_log_level());
        }

        //
        // Error handling
        //

        /**
         * Reset the current error state.
         *
         * @todo There should be some protection about when this can
         *       be done.
         */
        void reset_error()
        {
            current_error_ = wsrep::e_success;
        }

        /**
         * Return current error code.
         *
         * @return Current error code.
         */
        enum wsrep::client_error current_error() const
        {
            return current_error_;
        }

        enum wsrep::provider::status current_error_status() const
        {
            return current_error_status_;
        }
    protected:
        /**
         * Client context constuctor. This is protected so that it
         * can be called from derived class constructors only.
         */
        client_state(wsrep::mutex& mutex,
                     wsrep::condition_variable& cond,
                     wsrep::server_state& server_state,
                     wsrep::client_service& client_service,
                     const client_id& id,
                     enum mode mode)
            : owning_thread_id_(wsrep::this_thread::get_id())
            , rollbacker_active_(false)
            , mutex_(mutex)
            , cond_(cond)
            , server_state_(server_state)
            , client_service_(client_service)
            , id_(id)
            , mode_(mode)
            , toi_mode_(m_undefined)
            , state_(s_none)
            , state_hist_()
            , transaction_(*this)
            , toi_meta_()
            , nbo_meta_()
            , allow_dirty_reads_()
            , sync_wait_gtid_()
            , last_written_gtid_()
            , debug_log_level_(0)
            , current_error_(wsrep::e_success)
            , current_error_status_(wsrep::provider::success)
            , keep_command_error_()
        { }

    private:
        client_state(const client_state&);
        client_state& operator=(client_state&);

        friend class client_state_switch;
        friend class high_priority_context;
        friend class transaction;

        void do_acquire_ownership(wsrep::unique_lock<wsrep::mutex>& lock);
        // Wait for sync rollbacker to finish, with lock. Changes state
        // to exec.
        void do_wait_rollback_complete_and_acquire_ownership(
            wsrep::unique_lock<wsrep::mutex>& lock);
        void update_last_written_gtid(const wsrep::gtid&);
        void debug_log_state(const char*) const;
        void debug_log_keys(const wsrep::key_array& keys) const;
        void state(wsrep::unique_lock<wsrep::mutex>& lock, enum state state);
        void mode(wsrep::unique_lock<wsrep::mutex>& lock, enum mode mode);

        // Override current client error status. Optionally provide
        // an error status from the provider if the error was caused
        // by the provider call.
        void override_error(enum wsrep::client_error error,
                            enum wsrep::provider::status status =
                            wsrep::provider::success);

        // Poll provider::enter_toi() until return status from provider
        // does not indicate certification failure, timeout expires
        // or client is interrupted.
        enum wsrep::provider::status
        poll_enter_toi(wsrep::unique_lock<wsrep::mutex>& lock,
                       const wsrep::key_array& keys,
                       const wsrep::const_buffer& buffer,
                       wsrep::ws_meta& meta,
                       int flags,
                       std::chrono::time_point<wsrep::clock> wait_until,
                       bool& timed_out);
        void enter_toi_common(wsrep::unique_lock<wsrep::mutex>&);
        void leave_toi_common();

        wsrep::thread::id owning_thread_id_;
        bool rollbacker_active_;
        wsrep::mutex& mutex_;
        wsrep::condition_variable& cond_;
        wsrep::server_state& server_state_;
        wsrep::client_service& client_service_;
        wsrep::client_id id_;
        enum mode mode_;
        enum mode toi_mode_;
        enum state state_;
        std::vector<enum state> state_hist_;
        wsrep::transaction transaction_;
        wsrep::ws_meta toi_meta_;
        wsrep::ws_meta nbo_meta_;
        bool allow_dirty_reads_;
        wsrep::gtid sync_wait_gtid_;
        wsrep::gtid last_written_gtid_;
        int debug_log_level_;
        enum wsrep::client_error current_error_;
        enum wsrep::provider::status current_error_status_;
        bool keep_command_error_;

        /**
         * Marks external rollbacker thread for the client
         * as active. This will block client in before_command(), until
         * rolbacker has released the client.
         */
        void set_rollbacker_active(bool value)
        {
            rollbacker_active_ = value;
        }

        bool is_rollbacker_active()
        {
            return rollbacker_active_;
        }
    };

    static inline const char* to_c_string(
        enum wsrep::client_state::state state)
    {
        switch (state)
        {
        case wsrep::client_state::s_none: return "none";
        case wsrep::client_state::s_idle: return "idle";
        case wsrep::client_state::s_exec: return "exec";
        case wsrep::client_state::s_result: return "result";
        case wsrep::client_state::s_quitting: return "quit";
        }
        return "unknown";
    }

    static inline std::string to_string(enum wsrep::client_state::state state)
    {
        return to_c_string(state);
    }

    static inline const char* to_c_string(enum wsrep::client_state::mode mode)
    {
        switch (mode)
        {
        case wsrep::client_state::m_undefined: return "undefined";
        case wsrep::client_state::m_local: return "local";
        case wsrep::client_state::m_high_priority: return "high priority";
        case wsrep::client_state::m_toi: return "toi";
        case wsrep::client_state::m_rsu: return "rsu";
        case wsrep::client_state::m_nbo: return "nbo";
        }
        return "unknown";
    }

    static inline std::string to_string(enum wsrep::client_state::mode mode)
    {
        return to_c_string(mode);
    }

    /**
     * Utility class to switch the client state to high priority
     * mode. The client is switched back to the original mode
     * when the high priority context goes out of scope.
     */
    class high_priority_context
    {
    public:
        high_priority_context(wsrep::client_state& client)
            : client_(client)
            , orig_mode_(client.mode_)
        {
            wsrep::unique_lock<wsrep::mutex> lock(client.mutex_);
            client.mode(lock, wsrep::client_state::m_high_priority);
        }
        virtual ~high_priority_context()
        {
            wsrep::unique_lock<wsrep::mutex> lock(client_.mutex_);
            assert(client_.mode() == wsrep::client_state::m_high_priority);
            client_.mode(lock, orig_mode_);
        }
    private:
        wsrep::client_state& client_;
        enum wsrep::client_state::mode orig_mode_;
    };
}

#endif // WSREP_CLIENT_STATE_HPP
