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

#ifndef WSREP_DB_SERVER_HPP
#define WSREP_DB_SERVER_HPP

#include "wsrep/gtid.hpp"
#include "wsrep/client_state.hpp"

#include "db_storage_engine.hpp"
#include "db_server_state.hpp"
#include "db_server_service.hpp"

#include <boost/thread.hpp>

#include <string>
#include <memory>

namespace db
{
    class simulator;
    class client;
    class server
    {
    public:
        server(simulator& simulator,
               const std::string& name,
               const std::string& address);
        void applier_thread();
        void start_applier();
        void stop_applier();
        void start_clients();
        void stop_clients();
        void client_thread(const std::shared_ptr<db::client>& client);
        db::storage_engine& storage_engine() { return storage_engine_; }
        db::server_state& server_state() { return server_state_; }
        wsrep::transaction_id next_transaction_id()
        {
            return wsrep::transaction_id(last_transaction_id_.fetch_add(1) + 1);
        }
        void donate_sst(const std::string&, const  wsrep::gtid&, bool);
        wsrep::client_state* local_client_state();
        void release_client_state(wsrep::client_state*);
        wsrep::high_priority_service* streaming_applier_service();
    private:
        void start_client(size_t id);

        db::simulator& simulator_;
        db::storage_engine storage_engine_;
        wsrep::default_mutex mutex_;
        wsrep::default_condition_variable cond_;
        db::server_service server_service_;
        db::server_state server_state_;
        std::atomic<size_t> last_client_id_;
        std::atomic<size_t> last_transaction_id_;
        std::vector<boost::thread> appliers_;
        std::vector<std::shared_ptr<db::client>> clients_;
        std::vector<boost::thread> client_threads_;
    };
};

#endif // WSREP_DB_SERVER_HPP
