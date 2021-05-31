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

#ifndef WSREP_MOCK_CLIENT_CONTEXT_HPP
#define WSREP_MOCK_CLIENT_CONTEXT_HPP

#include "wsrep/client_state.hpp"
#include "wsrep/mutex.hpp"
#include "wsrep/compiler.hpp"

#include "test_utils.hpp"

namespace wsrep
{
    class mock_client_state : public wsrep::client_state
    {
    public:
        mock_client_state(wsrep::server_state& server_state,
                            wsrep::client_service& client_service,
                            const wsrep::client_id& id,
                            enum wsrep::client_state::mode mode)
            : wsrep::client_state(mutex_, cond_, server_state, client_service,
                                  id, mode)
            , mutex_()
            , cond_()
        { }
        ~mock_client_state()
        {
            if (transaction().active())
            {
                (void)client_service().bf_rollback();
            }
        }
    private:
        wsrep::default_mutex mutex_;
        wsrep::default_condition_variable cond_;
    public:
    private:
    };


    class mock_client_service : public wsrep::client_service
    {
    public:
        mock_client_service(wsrep::mock_client_state& client_state)
            : wsrep::client_service()
            , is_autocommit_()
            , do_2pc_()
              // , fail_next_applying_()
              // , fail_next_toi_()
            , bf_abort_during_wait_()
            , bf_abort_during_fragment_removal_()
            , error_during_prepare_data_()
            , killed_before_certify_()
            , sync_point_enabled_()
            , sync_point_action_()
            , bytes_generated_()
            , client_state_(client_state)
            , will_replay_called_()
            , replays_()
            , unordered_replays_()
            , aborts_()
        { }

        int bf_rollback() WSREP_OVERRIDE;

        bool interrupted(wsrep::unique_lock<wsrep::mutex>&)
            const WSREP_OVERRIDE
        { return killed_before_certify_; }


        void emergency_shutdown() WSREP_OVERRIDE { ++aborts_; }

        int remove_fragments() WSREP_OVERRIDE
        {
            if (bf_abort_during_fragment_removal_)
            {
                client_state_.before_rollback();
                client_state_.after_rollback();
                return 1;
            }
            else
            {
                return 0;
            }
        }

        void will_replay() WSREP_OVERRIDE { will_replay_called_ = true; }

        void signal_replayed() WSREP_OVERRIDE { }

        enum wsrep::provider::status replay() WSREP_OVERRIDE;

        enum wsrep::provider::status replay_unordered() WSREP_OVERRIDE
        {
            unordered_replays_++;
            return wsrep::provider::success;
        }

        void wait_for_replayers(
            wsrep::unique_lock<wsrep::mutex>& lock)
            WSREP_OVERRIDE
        {
            lock.unlock();
            if (bf_abort_during_wait_)
            {
                wsrep_test::bf_abort_unordered(client_state_);
            }
            lock.lock();
        }

        int prepare_data_for_replication() WSREP_OVERRIDE
        {
            if (error_during_prepare_data_)
            {
                return 1;
            }
            static const char buf[1] = { 1 };
            wsrep::const_buffer data = wsrep::const_buffer(buf, 1);
            return client_state_.append_data(data);
        }

        void cleanup_transaction() WSREP_OVERRIDE { }

        size_t bytes_generated() const WSREP_OVERRIDE
        {
            return bytes_generated_;
        }

        bool statement_allowed_for_streaming() const WSREP_OVERRIDE
        { return true; }
        int prepare_fragment_for_replication(wsrep::mutable_buffer& buffer, size_t& position)
            WSREP_OVERRIDE
        {
            if (error_during_prepare_data_)
            {
                return 1;
            }
            static const char buf[1] = { 1 };
            buffer.push_back(&buf[0], &buf[1]);
            wsrep::const_buffer data(buffer.data(), buffer.size());
            position = buffer.size();
            return client_state_.append_data(data);
        }

        void store_globals() WSREP_OVERRIDE { }
        void reset_globals() WSREP_OVERRIDE { }

        enum wsrep::provider::status commit_by_xid() WSREP_OVERRIDE
        {
            return wsrep::provider::success;
        }

        bool is_explicit_xa() WSREP_OVERRIDE
        {
            return false;
        }

        bool is_xa_rollback() WSREP_OVERRIDE
        {
            return false;
        }

        void debug_sync(const char* sync_point) WSREP_OVERRIDE
        {
            if (sync_point_enabled_ == sync_point)
            {
                switch (sync_point_action_)
                {
                case spa_bf_abort_unordered:
                    wsrep_test::bf_abort_unordered(client_state_);
                    break;
                case spa_bf_abort_ordered:
                    wsrep_test::bf_abort_ordered(client_state_);
                    break;
                }
            }
        }

        void debug_crash(const char*) WSREP_OVERRIDE
        {
            // Not going to do this while unit testing
        }


        //
        // Knobs to tune the behavior
        //
        bool is_autocommit_;
        bool do_2pc_;
        // bool fail_next_applying_;
        // bool fail_next_toi_;
        bool bf_abort_during_wait_;
        bool bf_abort_during_fragment_removal_;
        bool error_during_prepare_data_;
        bool killed_before_certify_;
        std::string sync_point_enabled_;
        enum sync_point_action
        {
            spa_bf_abort_unordered,
            spa_bf_abort_ordered
        } sync_point_action_;
        size_t bytes_generated_;

        //
        // Verifying the state
        //
        bool will_replay_called() const { return will_replay_called_; }
        size_t replays() const { return replays_; }
        size_t unordered_replays() const { return unordered_replays_; }
        size_t aborts() const { return aborts_; }
    private:
        wsrep::mock_client_state& client_state_;
        bool will_replay_called_;
        size_t replays_;
        size_t unordered_replays_;
        size_t aborts_;
    };

    class mock_client
        : public mock_client_state
        , public mock_client_service
    {
    public:
        mock_client(wsrep::server_state& server_state,
                    const wsrep::client_id& id,
                    enum wsrep::client_state::mode mode)
            : mock_client_state(server_state, *this, id, mode)
            , mock_client_service(static_cast<mock_client_state&>(*this))
        { }

        int after_row()
        {
            bytes_generated_++;
            return wsrep::client_state::after_row();
        }
    };
}

#endif // WSREP_MOCK_CLIENT_CONTEXT_HPP
