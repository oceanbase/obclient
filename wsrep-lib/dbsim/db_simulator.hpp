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

#ifndef WSREP_DB_SIMULATOR_HPP
#define WSREP_DB_SIMULATOR_HPP

#include "wsrep/gtid.hpp"
#include "wsrep/mutex.hpp"
#include "wsrep/lock.hpp"

#include "db_params.hpp"
#include "db_server.hpp"

#include <memory>
#include <chrono>
#include <unordered_set>
#include <map>

namespace db
{
    class server;
    class simulator
    {
    public:
        simulator(const params& params)
            : mutex_()
            , params_(params)
            , servers_()
            , clients_start_()
            , clients_stop_()
            , stats_()
        { }

        void run();
        void sst(db::server&,
                 const std::string&, const wsrep::gtid&, bool);
        const db::params& params() const
        { return params_; }
        std::string stats() const;
    private:
        void start();
        void stop();
        std::string server_port(size_t i) const;
        std::string build_cluster_address() const;

        wsrep::default_mutex mutex_;
        const db::params& params_;
        std::map<std::string, std::unique_ptr<db::server>> servers_;
        std::chrono::time_point<std::chrono::steady_clock> clients_start_;
        std::chrono::time_point<std::chrono::steady_clock> clients_stop_;
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
        } stats_;
    };
}
#endif // WSRE_DB_SIMULATOR_HPP
