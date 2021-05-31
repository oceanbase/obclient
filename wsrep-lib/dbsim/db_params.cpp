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

#include "db_params.hpp"

#include <boost/program_options.hpp>
#include <iostream>
#include <stdexcept>

namespace
{
    void validate_params(const db::params& params)
    {
        std::ostringstream os;
        if (params.n_servers != params.topology.size())
        {
            if (params.topology.size() > 0)
            {
                os << "Error: --topology=" << params.topology << " does not "
                   << "match the number of server --servers="
                   << params.n_servers << "\n";
            }
        }
        if (os.str().size())
        {
            throw std::invalid_argument(os.str());
        }
    }
}

db::params db::parse_args(int argc, char** argv)
{
    namespace po = boost::program_options;
    db::params params;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("wsrep-provider",
         po::value<std::string>(&params.wsrep_provider)->required(),
         "wsrep provider to load")
        ("wsrep-provider-options",
         po::value<std::string>(&params.wsrep_provider_options),
         "wsrep provider options")
        ("servers", po::value<size_t>(&params.n_servers)->required(),
         "number of servers to start")
        ("topology", po::value<std::string>(&params.topology),
         "replication topology (e.g. mm for multi master, ms for master/slave")
        ("clients", po::value<size_t>(&params.n_clients)->required(),
         "number of clients to start per master")
        ("transactions", po::value<size_t>(&params.n_transactions),
         "number of transactions run by a client")
        ("rows", po::value<size_t>(&params.n_rows),
         "number of rows per table")
        ("max-data-size", po::value<size_t>(&params.max_data_size),
         "maximum size of data payload (default 8)")
        ("random-data-size", po::value<bool>(&params.random_data_size),
         "randomized payload data size (default 0)")
        ("alg-freq", po::value<size_t>(&params.alg_freq),
         "ALG frequency")
        ("sync-wait", po::value<bool>(&params.sync_wait),
         "Turn on sync wait for each transaction")
        ("debug-log-level", po::value<int>(&params.debug_log_level),
         "debug logging level: 0 - none, 1 - verbose")
        ("fast-exit", po::value<int>(&params.fast_exit),
         "exit from simulation without graceful shutdown")
        ("ti",
         po::value<int>(&params.thread_instrumentation),
         "use instrumentation for threads/mutexes/condition variables"
         "(0 default disabled, 1 total counts, 2 per object)")
        ("ti-cond-checks",
         po::value<bool>(&params.cond_checks),
         "Enable checks for correct condition variable use. "
         " Effective only if thread-instrumentation is enabled")
        ;
    try
    {
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help"))
        {
            std::cerr << desc << "\n";
            exit(0);
        }
        po::notify(vm);
        validate_params(params);
    }
    catch (const po::error& e)
    {
        std::cerr << "Error parsing arguments: " << e.what() << "\n";
        std::cerr << desc << "\n";
        exit(1);
    }
    catch (...)
    {
        std::cerr << "Error parsing arguments\n";
        std::cerr << desc << "\n";
        exit(1);
    }
    return params;
}
