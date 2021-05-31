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

#include "db_client_service.hpp"
#include "db_high_priority_service.hpp"
#include "db_client.hpp"

db::client_service::client_service(db::client& client)
    : wsrep::client_service()
    , client_(client)
    , client_state_(client_.client_state())
{ }

int db::client_service::bf_rollback()
{
    int ret(client_state_.before_rollback());
    assert(ret == 0);
    client_.se_trx_.rollback();
    ret = client_state_.after_rollback();
    assert(ret == 0);
    return ret;
}

enum wsrep::provider::status
db::client_service::replay()
{
    wsrep::high_priority_context high_priority_context(client_state_);
    db::replayer_service replayer_service(
        client_.server_, client_);
    auto ret(client_state_.provider().replay(
                 client_state_.transaction().ws_handle(),
                 &replayer_service));
    if (ret == wsrep::provider::success)
    {
        ++client_.stats_.replays;
    }
    return ret;
}
