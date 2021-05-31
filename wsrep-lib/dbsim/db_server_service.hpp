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

#ifndef WSREP_DB_SERVER_SERVICE_HPP
#define WSREP_DB_SERVER_SERVICE_HPP

#include "wsrep/server_service.hpp"
#include <string>

namespace db
{
    class server;
    class server_service : public wsrep::server_service
    {
    public:
        server_service(db::server& server);
        wsrep::storage_service* storage_service(wsrep::client_service&) override;
        wsrep::storage_service* storage_service(wsrep::high_priority_service&) override;

        void release_storage_service(wsrep::storage_service*) override;
        wsrep::high_priority_service* streaming_applier_service(wsrep::client_service&) override;
        wsrep::high_priority_service* streaming_applier_service(wsrep::high_priority_service&) override;
        void release_high_priority_service(wsrep::high_priority_service*) override;

        bool sst_before_init() const override;
        int start_sst(const std::string&, const wsrep::gtid&, bool) override;
        std::string sst_request() override;
        void background_rollback(wsrep::client_state&) override;
        void bootstrap() override;
        void log_message(enum wsrep::log::level, const char* message) override;
        void log_dummy_write_set(wsrep::client_state&, const wsrep::ws_meta&)
            override;
        void log_view(wsrep::high_priority_service*,
                      const wsrep::view&) override;
        void recover_streaming_appliers(wsrep::client_service&) override;
        void recover_streaming_appliers(wsrep::high_priority_service&) override;
        wsrep::view get_view(wsrep::client_service&, const wsrep::id&)
            override;
        wsrep::gtid get_position(wsrep::client_service&) override;
        void set_position(wsrep::client_service&, const wsrep::gtid&) override;
        void log_state_change(enum wsrep::server_state::state,
                              enum wsrep::server_state::state) override;
        int wait_committing_transactions(int) override;
        void debug_sync(const char*) override;
    private:
        db::server& server_;
    };
}

#endif // WSREP_DB_SERVER_SERVICE_HPP
