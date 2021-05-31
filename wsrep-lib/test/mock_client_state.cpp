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

#include "wsrep/transaction.hpp"
#include "mock_client_state.hpp"
#include "mock_high_priority_service.hpp"

int wsrep::mock_client_service::bf_rollback()
{
    int ret(0);
    if (client_state_.before_rollback())
    {
        ret = 1;
    }
    else if (client_state_.after_rollback())
    {
        ret = 1;
    }
    return ret;
}

enum wsrep::provider::status
wsrep::mock_client_service::replay()
{
    wsrep::mock_high_priority_service hps(client_state_.server_state(),
                                          &client_state_, true);
    enum wsrep::provider::status ret(
        client_state_.provider().replay(
            client_state_.transaction().ws_handle(),
            &hps));
    ++replays_;
    return ret;
}
