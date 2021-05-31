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

/** @file db_client.hpp */

#ifndef WSREP_DB_CLIENT_HPP
#define WSREP_DB_CLIENT_HPP

#include "db_server_state.hpp"
#include "db_storage_engine.hpp"
#include "db_client_state.hpp"
#include "db_client_service.hpp"
#include "db_high_priority_service.hpp"

#include <random>

namespace db
{
    class server;
    class client
    {
    public:
        struct stats
        {
            long long commits;
            long long rollbacks;
            long long replays;
            stats()
                : commits(0)
                , rollbacks(0)
                , replays(0)
            { }
        };
        client(db::server&,
               wsrep::client_id,
               enum wsrep::client_state::mode,
               const db::params&);
        bool bf_abort(wsrep::seqno);
        const struct stats stats() const { return stats_; }
        void store_globals()
        {
            client_state_.store_globals();
        }
        void reset_globals()
        { }
        void start();
        wsrep::client_state& client_state() { return client_state_; }
        wsrep::client_service& client_service() { return client_service_; }
        bool do_2pc() const { return false; }
    private:
        friend class db::server_state;
        friend class db::client_service;
        friend class db::high_priority_service;
        template <class F> int client_command(F f);
        void run_one_transaction();
        void reset_error();
        void report_progress(size_t) const;
        wsrep::default_mutex mutex_;
        wsrep::default_condition_variable cond_;
        const db::params& params_;
        db::server& server_;
        db::server_state& server_state_;
        db::client_state client_state_;
        db::client_service client_service_;
        db::storage_engine::transaction se_trx_;
        wsrep::mutable_buffer data_;
        std::random_device random_device_;
        std::default_random_engine random_engine_;
        struct stats stats_;
    };
}

#endif // WSREP_DB_CLIENT_HPP
