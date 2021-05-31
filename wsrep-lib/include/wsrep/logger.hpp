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

#ifndef WSREP_LOGGER_HPP
#define WSREP_LOGGER_HPP

#include "mutex.hpp"
#include "lock.hpp"
#include "atomic.hpp"

#include <iosfwd>
#include <sstream>

#define WSREP_LOG_DEBUG(debug_level_fn, debug_level, msg)               \
    do {                                                                \
        if (debug_level_fn >= debug_level) wsrep::log_debug() << msg;   \
    } while (0)

namespace wsrep
{
    class log
    {
    public:
        enum level
        {
            debug,
            info,
            warning,
            error,
            unknown
        };

        enum debug_level
        {
            debug_level_server_state = 1,
            debug_level_transaction,
            debug_level_streaming,
            debug_level_client_state
        };

        /**
         * Signature for user defined logger callback function.
         *
         * @param pfx optional internally defined prefix for the message
         * @param msg message to log
         */
        typedef void (*logger_fn_type)(level l,
                                       const char* pfx, const char* msg);

        static const char* to_c_string(enum level level)
        {
            switch (level)
            {
            case debug:   return "debug";
            case info:    return "info";
            case warning: return "warning";
            case error:   return "error";
            case unknown: break;
            };
            return "unknown";
        }

        log(enum wsrep::log::level level, const char* prefix = "L:")
            : level_(level)
            , prefix_(prefix)
            , oss_()
        { }

        ~log()
        {
            if (logger_fn_)
            {
                logger_fn_(level_, prefix_, oss_.str().c_str());
            }
            else
            {
                wsrep::unique_lock<wsrep::mutex> lock(mutex_);
                os_ << prefix_ << oss_.str() << std::endl;
            }
        }

        template <typename T>
        std::ostream& operator<<(const T& val)
        {
            return (oss_ << val);
        }

        /**
         * Set user defined logger callback function.
         */
        static void logger_fn(logger_fn_type);

        /**
         * Set debug log level from client
         */
        static void debug_log_level(int debug_level);

        /**
         * Get current debug log level
         */
        static int debug_log_level();

    private:
        log(const log&);
        log& operator=(const log&);
        enum level level_;
        const char* prefix_;
        std::ostringstream oss_;
        static wsrep::mutex& mutex_;
        static std::ostream& os_;
        static logger_fn_type logger_fn_;
        static std::atomic_int debug_log_level_;
    };

    class log_error : public log
    {
    public:
        log_error()
            : log(error) { }
    };

    class log_warning : public log
    {
    public:
        log_warning()
            : log(warning) { }
    };

    class log_info : public log
    {
    public:
        log_info()
            : log(info) { }
    };

    class log_debug : public log
    {
    public:
        log_debug()
            : log(debug) { }
    };
}

#endif // WSREP_LOGGER_HPP
