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

#ifndef WSREP_DB_CLIENT_SERVICE_HPP
#define WSREP_DB_CLIENT_SERVICE_HPP

#include "wsrep/client_service.hpp"
#include "wsrep/transaction.hpp"

namespace db
{
    class client;
    class client_state;

    class client_service : public wsrep::client_service
    {
    public:
        client_service(db::client& client);

        bool interrupted(wsrep::unique_lock<wsrep::mutex>&)
            const override
        { return false; }
        void reset_globals() override { }
        void store_globals() override { }
        int prepare_data_for_replication() override
        {
            return 0;
        }
        void cleanup_transaction() override { }
        size_t bytes_generated() const override
        {
            return 0;
        }
        bool statement_allowed_for_streaming() const override
        {
            return true;
        }
        int prepare_fragment_for_replication(wsrep::mutable_buffer&,
                                             size_t& position) override
        {
            position = 0;
            return 0;
        }
        int remove_fragments() override { return 0; }
        int bf_rollback() override;
        void will_replay() override { }
        void signal_replayed() override { }
        void wait_for_replayers(wsrep::unique_lock<wsrep::mutex>&) override { }
        enum wsrep::provider::status replay()
            override;

        enum wsrep::provider::status replay_unordered() override
        {
            return wsrep::provider::success;
        }

        void emergency_shutdown() override { ::abort(); }

        enum wsrep::provider::status commit_by_xid() override
        {
            return wsrep::provider::success;
        }

        bool is_explicit_xa() override
        {
            return false;
        }

        bool is_xa_rollback() override
        {
            return false;
        }

        void debug_sync(const char*) override { }
        void debug_crash(const char*) override { }
    private:
        db::client& client_;
        wsrep::client_state& client_state_;
    };
}

#endif // WSREP_DB_CLIENT_SERVICE_HPP
