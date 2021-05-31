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

#include "db_high_priority_service.hpp"
#include "db_server.hpp"
#include "db_client.hpp"

db::high_priority_service::high_priority_service(
    db::server& server, db::client& client)
    : wsrep::high_priority_service(server.server_state())
    , server_(server)
    , client_(client)
{ }

int db::high_priority_service::start_transaction(
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta)
{
    return client_.client_state().start_transaction(ws_handle, ws_meta);
}

int db::high_priority_service::next_fragment(const wsrep::ws_meta& ws_meta)
{
    return client_.client_state().next_fragment(ws_meta);
}

const wsrep::transaction& db::high_priority_service::transaction() const
{
    return client_.client_state().transaction();
}

int db::high_priority_service::adopt_transaction(const wsrep::transaction&)
{
    throw wsrep::not_implemented_error();
}

int db::high_priority_service::apply_write_set(
    const wsrep::ws_meta&,
    const wsrep::const_buffer&,
    wsrep::mutable_buffer&)
{
    client_.se_trx_.start(&client_);
    client_.se_trx_.apply(client_.client_state().transaction());
    return 0;
}

int db::high_priority_service::apply_toi(
    const wsrep::ws_meta&,
    const wsrep::const_buffer&,
    wsrep::mutable_buffer&)
{
    throw wsrep::not_implemented_error();
}

int db::high_priority_service::apply_nbo_begin(
    const wsrep::ws_meta&,
    const wsrep::const_buffer&,
    wsrep::mutable_buffer&)
{
    throw wsrep::not_implemented_error();
}

int db::high_priority_service::commit(const wsrep::ws_handle& ws_handle,
                                      const wsrep::ws_meta& ws_meta)
{
    client_.client_state_.prepare_for_ordering(ws_handle, ws_meta, true);
    int ret(client_.client_state_.before_commit());
    if (ret == 0) client_.se_trx_.commit(ws_meta.gtid());
    ret = ret || client_.client_state_.ordered_commit();
    ret = ret || client_.client_state_.after_commit();
    return ret;
}

int db::high_priority_service::rollback(const wsrep::ws_handle& ws_handle,
                                        const wsrep::ws_meta& ws_meta)
{
    client_.client_state_.prepare_for_ordering(ws_handle, ws_meta, false);
    int ret(client_.client_state_.before_rollback());
    assert(ret == 0);
    client_.se_trx_.rollback();
    ret = client_.client_state_.after_rollback();
    assert(ret == 0);
    return ret;
}

void db::high_priority_service::adopt_apply_error(wsrep::mutable_buffer& err)
{
    client_.client_state_.adopt_apply_error(err);
}

void db::high_priority_service::after_apply()
{
    client_.client_state_.after_applying();
}

int db::high_priority_service::log_dummy_write_set(
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta,
    wsrep::mutable_buffer& err)
{
    int ret(client_.client_state_.start_transaction(ws_handle, ws_meta));
    assert(ret == 0);
    if (ws_meta.ordered())
    {
        client_.client_state_.adopt_apply_error(err);
        client_.client_state_.prepare_for_ordering(ws_handle, ws_meta, true);
        ret = client_.client_state_.before_commit();
        assert(ret == 0);
        ret = client_.client_state_.ordered_commit();
        assert(ret == 0);
        ret = client_.client_state_.after_commit();
        assert(ret == 0);
    }
    client_.client_state_.after_applying();
    return ret;
}

bool db::high_priority_service::is_replaying() const
{
    return false;
}
