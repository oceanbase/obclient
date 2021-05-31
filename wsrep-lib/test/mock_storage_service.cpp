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

#include "mock_storage_service.hpp"
#include "mock_server_state.hpp"

#include "wsrep/client_state.hpp"

wsrep::mock_storage_service::mock_storage_service(
    wsrep::server_state& server_state,
    wsrep::client_id client_id)
    : client_service_(client_state_)
    , client_state_(server_state, client_service_, client_id,
                    wsrep::client_state::m_high_priority)
{
    client_state_.open(client_id);
    client_state_.before_command();
}


wsrep::mock_storage_service::~mock_storage_service()
{
    client_state_.after_command_before_result();
    client_state_.after_command_after_result();
    client_state_.close();
    client_state_.cleanup();
}

int wsrep::mock_storage_service::start_transaction(
    const wsrep::ws_handle& ws_handle)
{
    return client_state_.start_transaction(ws_handle.transaction_id());
}

void wsrep::mock_storage_service::adopt_transaction(
    const wsrep::transaction& transaction)
{
    client_state_.adopt_transaction(transaction);
}

int wsrep::mock_storage_service::commit(const wsrep::ws_handle& ws_handle,
                                        const wsrep::ws_meta& ws_meta)
{
    // the logic here matches mariadb's wsrep_storage_service::commit
    bool ret = 0;
    const bool is_ordered = !ws_meta.seqno().is_undefined();
    if (is_ordered)
    {
        ret = ret || client_state_.prepare_for_ordering(ws_handle, ws_meta, true);
        ret = ret || client_state_.before_commit();
        ret = ret || client_state_.ordered_commit();
        ret = ret || client_state_.after_commit();
    }

    if (!is_ordered)
    {
        client_state_.before_rollback();
        client_state_.after_rollback();
    }
    else if (ret)
    {
        client_state_.prepare_for_ordering(wsrep::ws_handle(), wsrep::ws_meta(), false);
    }

    client_state_.after_applying();
    return ret;
}

int wsrep::mock_storage_service::rollback(const wsrep::ws_handle& ws_handle,
                                          const wsrep::ws_meta& ws_meta)
{
    int ret(client_state_.prepare_for_ordering(
                ws_handle, ws_meta, false) ||
            client_state_.before_rollback() ||
            client_state_.after_rollback());
    client_state_.after_applying();
    return ret;
}
