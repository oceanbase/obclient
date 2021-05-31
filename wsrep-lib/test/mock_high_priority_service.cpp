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

#include "mock_high_priority_service.hpp"
#include "mock_server_state.hpp"
#include <sstream>

int wsrep::mock_high_priority_service::start_transaction(
    const wsrep::ws_handle& ws_handle, const wsrep::ws_meta& ws_meta)
{
    return client_state_->start_transaction(ws_handle, ws_meta);
}

int wsrep::mock_high_priority_service::next_fragment(
    const wsrep::ws_meta& ws_meta)
{
    return client_state_->next_fragment(ws_meta);
}

int wsrep::mock_high_priority_service::adopt_transaction(
    const wsrep::transaction& transaction)
{
    client_state_->adopt_transaction(transaction);
    if (transaction.state() == wsrep::transaction::s_prepared)
    {
        client_state_->restore_xid(transaction.xid());
    }
    return 0;
}

int wsrep::mock_high_priority_service::apply_write_set(
    const wsrep::ws_meta& meta,
    const wsrep::const_buffer&,
    wsrep::mutable_buffer& err)
{
    assert(client_state_->toi_meta().seqno().is_undefined());
    assert(client_state_->transaction().state() == wsrep::transaction::s_executing ||
           client_state_->transaction().state() == wsrep::transaction::s_replaying);
    if (fail_next_applying_)
    {
        std::ostringstream os;
        os << "failed " << meta;
        err.push_back(os.str());
        assert(err.size() > 0);
        return 1;
    }
    else
    {
        return 0;
    };
}

int wsrep::mock_high_priority_service::commit(
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta)
{
    int ret(0);
    client_state_->prepare_for_ordering(ws_handle, ws_meta, true);
    if (do_2pc_)
    {
        ret = client_state_->before_prepare() ||
            client_state_->after_prepare();
    }
    return (ret || client_state_->before_commit() ||
            client_state_->ordered_commit() ||
            client_state_->after_commit());
}

int wsrep::mock_high_priority_service::rollback(
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta)
{
    client_state_->prepare_for_ordering(ws_handle, ws_meta, false);
    return (client_state_->before_rollback() ||
            client_state_->after_rollback());
}

int wsrep::mock_high_priority_service::apply_toi(const wsrep::ws_meta&,
                                                 const wsrep::const_buffer&,
                                                 wsrep::mutable_buffer&)
{
    assert(client_state_->transaction().active() == false);
    assert(client_state_->toi_meta().seqno().is_undefined() == false);
    return (fail_next_toi_ ? 1 : 0);
}

int wsrep::mock_high_priority_service::apply_nbo_begin(
    const wsrep::ws_meta& ws_meta,
    const wsrep::const_buffer&,
    wsrep::mutable_buffer&)
{
    const int nbo_begin_flags __attribute__((unused))
        (wsrep::provider::flag::isolation |
         wsrep::provider::flag::start_transaction);
    assert(ws_meta.flags() & nbo_begin_flags);
    assert((ws_meta.flags() & ~nbo_begin_flags) == 0);

    if (fail_next_toi_)
    {
        return 1;
    }
    else
    {
        nbo_cs_ = std::unique_ptr<wsrep::mock_client>(
            new wsrep::mock_client(client_state_->server_state(),
                                   wsrep::client_id(1),
                                   wsrep::client_state::m_local));
        nbo_cs_->open(wsrep::client_id(1));
        nbo_cs_->before_command();
        nbo_cs_->before_statement();
        return nbo_cs_->enter_nbo_mode(ws_meta);
    }
}

void wsrep::mock_high_priority_service::adopt_apply_error(
    wsrep::mutable_buffer& err)
{
    client_state_->adopt_apply_error(err);
}

void wsrep::mock_high_priority_service::after_apply()
{
    client_state_->after_applying();
}

int wsrep::mock_high_priority_service::log_dummy_write_set(
    const wsrep::ws_handle&,
    const wsrep::ws_meta&,
    wsrep::mutable_buffer& err)
{
    return err.size() > 0;
}
