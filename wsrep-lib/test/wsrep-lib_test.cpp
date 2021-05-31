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

/** @file wsrep-lib_test.cpp
 *
 * Run wsrep-lib unit tests.
 *
 * Commandline arguments:
 *
 * --wsrep-log-file=<file>   Write log from wsrep-lib logging facility
 *                           into <file>. If <file> is left empty, the
 *                           log is written into stdout.
 * --wsrep-debug-level=<int> Set debug level
 *                           See wsrep::log::debug_level for valid values
 */

#include "wsrep/logger.hpp"
#include <fstream>

#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>

// Log file to write messages logged via wsrep-lib logging facility.
static std::string log_file_name("wsrep-lib_test.log");
static std::ofstream log_file;
// Debug log level for wsrep-lib logging
static std::string debug_log_level;


static void log_fn(wsrep::log::level level,
                   const char* pfx,
                   const char* msg)
{
    log_file << wsrep::log::to_c_string(level) << " " << pfx << msg << std::endl;
}

static bool parse_arg(const std::string& arg)
{
    const std::string delim("=");
    auto delim_pos(arg.find(delim));
    const auto parm(arg.substr(0, delim_pos));
    std::string val;
    if (delim_pos != std::string::npos)
    {
        val = arg.substr(delim_pos + 1);
    }

    if (parm == "--wsrep-log-file")
    {
        log_file_name = val;
    }
    else if (parm == "--wsrep-debug-level")
    {
        debug_log_level = val;
    }
    else
    {
        std::cerr << "Error: Unknown argument " << arg << std::endl;
        return false;
    }
    return true;
}

static bool setup_env(int argc, char* argv[])
{
    for (int i(1); i < argc; ++i)
    {
        if (parse_arg(argv[i]) == false)
        {
            return false;
        }
    }

    if (log_file_name.size())
    {
        log_file.open(log_file_name);
        if (!log_file)
        {
            int err(errno);
            std::cerr << "Failed to open '" << log_file_name
                      << "': '" << ::strerror(err) << "'" << std::endl;
            return false;
        }
        std::cout << "Writing wsrep-lib log into '" << log_file_name << "'"
                  << std::endl;
        wsrep::log::logger_fn(log_fn);
    }

    if (debug_log_level.size())
    {
        int level = std::stoi(debug_log_level);
        std::cout << "Setting debug level '" << level << "'" << std::endl;
        wsrep::log::debug_log_level(level);
    }

    return true;
}

bool init_unit_test()
{
    return setup_env(boost::unit_test::framework::master_test_suite().argc,
                     boost::unit_test::framework::master_test_suite().argv);
}
