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

#ifndef WSREP_TEST_CLIENT_CONTEXT_FIXTURE_HPP
#define WSREP_TEST_CLIENT_CONTEXT_FIXTURE_HPP

#include "mock_server_state.hpp"
#include "mock_client_state.hpp"


#include <boost/test/unit_test.hpp>

namespace
{
    struct replicating_client_fixture_sync_rm
    {
        replicating_client_fixture_sync_rm()
            : server_service(sc)
            , sc("s1", wsrep::server_state::rm_sync, server_service)
            , cc(sc, wsrep::client_id(1),
                 wsrep::client_state::m_local)
            , tc(cc.transaction())
        {
            sc.mock_connect();
            cc.open(cc.id());
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
        }
        wsrep::mock_server_service server_service;
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct replicating_two_clients_fixture_sync_rm
    {
        replicating_two_clients_fixture_sync_rm()
            : server_service(sc)
            , sc("s1", wsrep::server_state::rm_sync, server_service)
            , cc1(sc, wsrep::client_id(1),
                  wsrep::client_state::m_local)
            , cc2(sc, wsrep::client_id(2),
                  wsrep::client_state::m_local)
            , tc(cc1.transaction())
        {
            sc.mock_connect();
            cc1.open(cc1.id());
            BOOST_REQUIRE(cc1.before_command() == 0);
            BOOST_REQUIRE(cc1.before_statement() == 0);
            cc2.open(cc2.id());
            BOOST_REQUIRE(cc2.before_command() == 0);
            BOOST_REQUIRE(cc2.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
        }
        wsrep::mock_server_service server_service;
        wsrep::mock_server_state sc;
        wsrep::mock_client cc1;
        wsrep::mock_client cc2;
        const wsrep::transaction& tc;
    };

    struct replicating_client_fixture_async_rm
    {
        replicating_client_fixture_async_rm()
            : server_service(sc)
            , sc("s1", wsrep::server_state::rm_async, server_service)
            , cc(sc, wsrep::client_id(1),
                 wsrep::client_state::m_local)
            , tc(cc.transaction())
        {
            sc.mock_connect();
            cc.open(cc.id());
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
        }
        wsrep::mock_server_service server_service;
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct replicating_client_fixture_2pc
    {
        replicating_client_fixture_2pc()
            : server_service(sc)
            , sc("s1", wsrep::server_state::rm_sync, server_service)
            , cc(sc,  wsrep::client_id(1),
                 wsrep::client_state::m_local)
            , tc(cc.transaction())
        {
            sc.mock_connect();
            cc.open(cc.id());
            cc.do_2pc_ = true;
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
        }
        wsrep::mock_server_service server_service;
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct replicating_client_fixture_autocommit
    {
        replicating_client_fixture_autocommit()
            : server_service(sc)
            , sc("s1", wsrep::server_state::rm_sync, server_service)
            , cc(sc, wsrep::client_id(1),
                 wsrep::client_state::m_local)
            , tc(cc.transaction())
        {
            sc.mock_connect();
            cc.open(cc.id());
            cc.is_autocommit_ = true;
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
        }
        wsrep::mock_server_service server_service;
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct applying_client_fixture
    {
        applying_client_fixture()
            : server_service(sc)
            , sc("s1",
                 wsrep::server_state::rm_async, server_service)
            , cc(sc,
                 wsrep::client_id(1),
                 wsrep::client_state::m_high_priority)
            , tc(cc.transaction())
        {
            sc.mock_connect();
            cc.open(cc.id());
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
        }
        void start_transaction(wsrep::transaction_id id,
                               wsrep::seqno seqno)
        {
            wsrep::ws_handle ws_handle(id, (void*)1);
            wsrep::ws_meta ws_meta(wsrep::gtid(wsrep::id("1"), seqno),
                                   wsrep::stid(sc.id(),
                                               wsrep::transaction_id(1),
                                               cc.id()),
                                   wsrep::seqno(0),
                                   wsrep::provider::flag::start_transaction |
                                   wsrep::provider::flag::commit);
            BOOST_REQUIRE(cc.start_transaction(ws_handle, ws_meta) == 0);
            BOOST_REQUIRE(tc.active() == true);
            BOOST_REQUIRE(tc.certified() == true);
            BOOST_REQUIRE(tc.ordered() == true);
        }

        wsrep::mock_server_service server_service;
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct applying_client_fixture_2pc
    {
        applying_client_fixture_2pc()
            : server_service(sc)
            , sc("s1",
                 wsrep::server_state::rm_async, server_service)
            , cc(sc,
                 wsrep::client_id(1),
                 wsrep::client_state::m_high_priority)
            , tc(cc.transaction())
        {
            sc.mock_connect();
            cc.open(cc.id());
            cc.do_2pc_ = true;
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            wsrep::ws_handle ws_handle(wsrep::transaction_id(1), (void*)1);
            wsrep::ws_meta ws_meta(wsrep::gtid(wsrep::id("1"), wsrep::seqno(1)),
                                   wsrep::stid(sc.id(),
                                               wsrep::transaction_id(1),
                                               cc.id()),
                                   wsrep::seqno(0),
                                   wsrep::provider::flag::start_transaction |
                                   wsrep::provider::flag::commit);
            BOOST_REQUIRE(cc.start_transaction(ws_handle, ws_meta) == 0);
            BOOST_REQUIRE(tc.active() == true);
            BOOST_REQUIRE(tc.certified() == true);
            BOOST_REQUIRE(tc.ordered() == true);
        }
        wsrep::mock_server_service server_service;
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct streaming_client_fixture_row
    {
        streaming_client_fixture_row()
            : server_service(sc)
            , sc("s1", wsrep::server_state::rm_sync, server_service)
            , cc(sc,
                 wsrep::client_id(1),
                 wsrep::client_state::m_local)
            , tc(cc.transaction())
        {
            sc.mock_connect();
            cc.open(cc.id());
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
            cc.enable_streaming(wsrep::streaming_context::row, 1);
        }

        wsrep::mock_server_service server_service;
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct streaming_client_fixture_byte
    {
        streaming_client_fixture_byte()
            : server_service(sc)
            , sc("s1", wsrep::server_state::rm_sync, server_service)
            , cc(sc,
                 wsrep::client_id(1),
                 wsrep::client_state::m_local)
            , tc(cc.transaction())
        {
            sc.mock_connect();
            cc.open(cc.id());
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
            cc.enable_streaming(wsrep::streaming_context::bytes, 1);
        }
        wsrep::mock_server_service server_service;
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct streaming_client_fixture_statement
    {
        streaming_client_fixture_statement()
            : server_service(sc)
            , sc("s1", wsrep::server_state::rm_sync, server_service)
            , cc(sc,
                 wsrep::client_id(1),
                 wsrep::client_state::m_local)
            , tc(cc.transaction())
        {
            sc.mock_connect();
            cc.open(cc.id());
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
            cc.enable_streaming(wsrep::streaming_context::statement, 1);
        }

        wsrep::mock_server_service server_service;
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };
}
#endif // WSREP_TEST_CLIENT_CONTEXT_FIXTURE_HPP
