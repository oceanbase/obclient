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

#ifndef WSREP_DB_SERVER_CONTEXT_HPP
#define WSREP_DB_SERVER_CONTEXT_HPP

#include "wsrep/server_state.hpp"
#include "wsrep/server_service.hpp"
#include "wsrep/client_state.hpp"

#include <atomic>

namespace db
{
    class server;
    class server_state : public wsrep::server_state
    {
    public:
        server_state(wsrep::server_service& server_service,
                     const std::string& name,
                     const std::string& address,
                     const std::string& working_dir)
            : wsrep::server_state(
                mutex_,
                cond_,
                server_service,
                nullptr,
                name,
                "",
                address,
                working_dir,
                wsrep::gtid::undefined(),
                1,
                wsrep::server_state::rm_async)
            , mutex_()
            , cond_()
        { }
    private:
        wsrep::default_mutex mutex_;
        wsrep::default_condition_variable cond_;
    };
}

#endif // WSREP_DB_SERVER_CONTEXT_HPP
