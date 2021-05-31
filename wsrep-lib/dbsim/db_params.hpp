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

#ifndef WSREP_DB_PARAMS_HPP
#define WSREP_DB_PARAMS_HPP

#include <cstddef>
#include <string>

namespace db
{
    struct params
    {
        size_t n_servers;
        size_t n_clients;
        size_t n_transactions;
        size_t n_rows;
        size_t max_data_size; // Maximum size of write set data payload.
        bool random_data_size; // If true, randomize data payload size.
        size_t alg_freq;
        bool sync_wait;
        std::string topology;
        std::string wsrep_provider;
        std::string wsrep_provider_options;
        int debug_log_level;
        int fast_exit;
        int thread_instrumentation;
        bool cond_checks;
        params()
            : n_servers(0)
            , n_clients(0)
            , n_transactions(0)
            , n_rows(1000)
            , max_data_size(8)
            , random_data_size(false)
            , alg_freq(0)
            , sync_wait(false)
            , topology()
            , wsrep_provider()
            , wsrep_provider_options()
            , debug_log_level(0)
            , fast_exit(0)
            , thread_instrumentation()
            , cond_checks()
        { }
    };

    params parse_args(int argc, char** argv);
}

#endif // WSREP_DB_PARAMS_HPP
