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

#include "mock_server_state.hpp"

#include <boost/test/unit_test.hpp>

namespace
{
    struct server_fixture_base
    {
        server_fixture_base()
            : server_service(ss)
            , ss("s1",
                 wsrep::server_state::rm_sync, server_service)
            , cc(ss,
                 wsrep::client_id(1),
                 wsrep::client_state::m_high_priority)
            , hps(ss, &cc, false)
            , ws_handle(wsrep::transaction_id(1), (void*)1)
            , ws_meta(wsrep::gtid(wsrep::id("1"), wsrep::seqno(1)),
                      wsrep::stid(wsrep::id("1"), wsrep::transaction_id(1),
                                  wsrep::client_id(1)),
                      wsrep::seqno(0),
                      wsrep::provider::flag::start_transaction |
                      wsrep::provider::flag::commit)
            , cluster_id("1")
            , bootstrap_view()
            , second_view()
            , third_view()
        {
            wsrep::gtid state_id(cluster_id, wsrep::seqno(0));
            std::vector<wsrep::view::member> members;
            members.push_back(wsrep::view::member(
                                  wsrep::id("s1"), "s1", ""));
            bootstrap_view = wsrep::view(state_id,
                                         wsrep::seqno(1),
                                         wsrep::view::primary,
                                         0, // capabilities
                                         0, // own index
                                         1, // protocol version
                                         members);

            members.push_back(wsrep::view::member(
                                  wsrep::id("s2"), "s2", ""));
            second_view = wsrep::view(wsrep::gtid(cluster_id, wsrep::seqno(1)),
                                      wsrep::seqno(2),
                                      wsrep::view::primary,
                                      0, // capabilities
                                      1, // own index
                                      1, // protocol version
                                      members);

            members.push_back(wsrep::view::member(
                                  wsrep::id("s3"), "s3", ""));

            third_view = wsrep::view(wsrep::gtid(cluster_id, wsrep::seqno(2)),
                                     wsrep::seqno(3),
                                     wsrep::view::primary,
                                     0, // capabilities
                                     1, // own index
                                     1, // protocol version
                                     members);

            cc.open(cc.id());
            BOOST_REQUIRE(cc.before_command() == 0);
        }
        wsrep::mock_server_service server_service;
        wsrep::mock_server_state ss;
        wsrep::mock_client cc;
        wsrep::mock_high_priority_service hps;
        wsrep::ws_handle ws_handle;
        wsrep::ws_meta ws_meta;
        wsrep::id cluster_id;
        wsrep::view bootstrap_view;
        wsrep::view second_view;
        wsrep::view third_view;

        void connect_in_view(const wsrep::view& view)
        {
            BOOST_REQUIRE(ss.connect("cluster", "local", "0", false) == 0);
            ss.on_connect(view);
            BOOST_REQUIRE(ss.state() == wsrep::server_state::s_connected);
        }

        void prepare_for_sst()
        {
            ss.prepare_for_sst();
            BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joiner);
        }

        void final_view()
        {
            BOOST_REQUIRE(ss.state() != wsrep::server_state::s_disconnected);
            wsrep::view view(wsrep::gtid(),                     // state_id
                             wsrep::seqno::undefined(),         // view seqno
                             wsrep::view::disconnected,         // status
                             0,                                 // capabilities
                             -1,                                // own_index
                             0,                                 // protocol ver
                             std::vector<wsrep::view::member>() // members
                );
            ss.on_view(view, &hps);
        }

        void disconnect()
        {
            BOOST_REQUIRE(ss.state() != wsrep::server_state::s_disconnecting);
            ss.disconnect();
            BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnecting);
            final_view();
            BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnected);
        }

    };

    struct applying_server_fixture : server_fixture_base
    {
        applying_server_fixture()
            : server_fixture_base()
        {
            ss.mock_connect();
        }
    };

    struct sst_first_server_fixture : server_fixture_base
    {
        sst_first_server_fixture()
            : server_fixture_base()
        {
            server_service.sst_before_init_ = true;
        }

        void sst_received_action()
        {
            server_service.sync_point_enabled_ = "on_view_wait_initialized";
            server_service.sync_point_action_  = server_service.spa_initialize;
        }

        void initialization_failure_action()
        {
            server_service.sync_point_enabled_ = "on_view_wait_initialized";
            server_service.sync_point_action_ =
                server_service.spa_initialize_error;
        }

        void clear_sync_point_action()
        {
            server_service.sync_point_enabled_ = "";
            server_service.sync_point_action_ = server_service.spa_none;
        }

        // Helper method to bootstrap the server with bootstrap view
        void bootstrap()
        {
            connect_in_view(bootstrap_view);

            sst_received_action();
            ss.on_view(bootstrap_view, &hps);
            clear_sync_point_action();
            BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joined);
            ss.on_sync();
            BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
        }

    };

    struct init_first_server_fixture : server_fixture_base
    {
        init_first_server_fixture()
            : server_fixture_base()
        {
            server_service.sst_before_init_ = false;
        }

        // Helper method to bootstrap the server with bootstrap view
        void bootstrap()
        {
            ss.initialized();
            BOOST_REQUIRE(ss.state() == wsrep::server_state::s_initialized);
            BOOST_REQUIRE(ss.connect("cluster", "local", "0", false) == 0);
            ss.on_connect(bootstrap_view);
            BOOST_REQUIRE(ss.state() == wsrep::server_state::s_connected);
            ss.on_view(bootstrap_view, &hps);
            BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joined);
            ss.on_sync();
            BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
        }
    };

    // Helper to pass to BOOST_REQUIRE_EXCEPTION. Always returns true.
    bool exception_check(const wsrep::runtime_error&) { return true; }
}

// Test on_apply() method for 1pc
BOOST_FIXTURE_TEST_CASE(server_state_applying_1pc,
                        applying_server_fixture)
{
    char buf[1] = { 1 };
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer(buf, 1)) == 0);
    const wsrep::transaction& txc(cc.transaction());
    // ::abort();
    BOOST_REQUIRE_MESSAGE(
        txc.state() == wsrep::transaction::s_committed,
        "Transaction state " << txc.state() << " not committed");
}

// Test on_apply() method for 2pc
BOOST_FIXTURE_TEST_CASE(server_state_applying_2pc,
                        applying_server_fixture)
{
    hps.do_2pc_ = true;
    char buf[1] = { 1 };
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer(buf, 1)) == 0);
    const wsrep::transaction& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == wsrep::transaction::s_committed);
}

// Test on_apply() method for 1pc transaction which
// fails applying and rolls back
BOOST_FIXTURE_TEST_CASE(server_state_applying_1pc_rollback,
                        applying_server_fixture)
{
    /* make sure default success result is flipped to error_fatal */
    ss.provider().commit_order_leave_result_ = wsrep::provider::success;
    hps.fail_next_applying_ = true;
    char buf[1] = { 1 };
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer(buf, 1)) == 1);
    const wsrep::transaction& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == wsrep::transaction::s_aborted);
}

// Test on_apply() method for 2pc transaction which
// fails applying and rolls back
BOOST_FIXTURE_TEST_CASE(server_state_applying_2pc_rollback,
                        applying_server_fixture)
{
    /* make sure default success result is flipped to error_fatal */
    ss.provider().commit_order_leave_result_ = wsrep::provider::success;
    hps.do_2pc_ = true;
    hps.fail_next_applying_ = true;
    char buf[1] = { 1 };
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer(buf, 1)) == 1);
    const wsrep::transaction& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == wsrep::transaction::s_aborted);
}

BOOST_FIXTURE_TEST_CASE(server_state_streaming, applying_server_fixture)
{
    ws_meta = wsrep::ws_meta(wsrep::gtid(wsrep::id("1"), wsrep::seqno(1)),
                             wsrep::stid(wsrep::id("1"),
                                         wsrep::transaction_id(1),
                                         wsrep::client_id(1)),
                             wsrep::seqno(0),
                             wsrep::provider::flag::start_transaction);
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer("1", 1)) == 0);
    BOOST_REQUIRE(ss.find_streaming_applier(
                      ws_meta.server_id(), ws_meta.transaction_id()));
    ws_meta = wsrep::ws_meta(wsrep::gtid(wsrep::id("1"), wsrep::seqno(2)),
                             wsrep::stid(wsrep::id("1"),
                                         wsrep::transaction_id(1),
                                         wsrep::client_id(1)),
                             wsrep::seqno(1),
                             0);
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer("1", 1)) == 0);
    ws_meta = wsrep::ws_meta(wsrep::gtid(wsrep::id("1"), wsrep::seqno(2)),
                             wsrep::stid(wsrep::id("1"),
                                         wsrep::transaction_id(1),
                                         wsrep::client_id(1)),
                             wsrep::seqno(1),
                             wsrep::provider::flag::commit);
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer("1", 1)) == 0);
    BOOST_REQUIRE(ss.find_streaming_applier(
                      ws_meta.server_id(), ws_meta.transaction_id()) == 0);
}


BOOST_AUTO_TEST_CASE(server_state_state_strings)
{
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_disconnected) == "disconnected");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_initializing) == "initializing");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_initialized) == "initialized");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_connected) == "connected");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_joiner) == "joiner");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_joined) == "joined");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_donor) == "donor");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_synced) == "synced");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_disconnecting) == "disconnecting");
}

///////////////////////////////////////////////////////////////////////////////
//                     Test cases for SST first                              //
///////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_CASE(server_state_sst_first_boostrap,
                        sst_first_server_fixture)
{
    bootstrap();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
}


BOOST_FIXTURE_TEST_CASE(server_state_sst_first_join_with_sst,
                        sst_first_server_fixture)
{
    connect_in_view(second_view);
    prepare_for_sst();
    sst_received_action();
    // Mock server service get_view() gets view from logged_view_.
    // Get_view() is called from sst_received(). This emulates the
    // case where SST contains the view in which SST happens.
    server_service.logged_view(second_view);
    server_service.position(wsrep::gtid(cluster_id, wsrep::seqno(2)));
    ss.sst_received(cc, 0);
    clear_sync_point_action();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joined);
    ss.on_sync();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
}

BOOST_FIXTURE_TEST_CASE(server_state_sst_first_join_with_ist,
                        sst_first_server_fixture)
{
    connect_in_view(second_view);
    // Mock server service get_view() gets view from logged_view_.
    // Get_view() is called from sst_received(). This emulates the
    // case where the view is stored in stable storage.
    server_service.logged_view(second_view);
    sst_received_action();
    ss.on_view(second_view, &hps);
    clear_sync_point_action();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joined);
    ss.on_view(third_view, &hps);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joined);
    ss.on_sync();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
}


// Cycle from synced state to disconnected and back to synced. Server
// storage engines remain initialized.
BOOST_FIXTURE_TEST_CASE(
    server_state_sst_first_synced_disconnected_synced_no_sst,
    sst_first_server_fixture)
{
    bootstrap();
    ss.disconnect();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnecting);
    final_view();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnected);

    // Connect back as a sole member in the cluster
    BOOST_REQUIRE(ss.connect("cluster", "local", "0", false) == 0);
    // @todo: s_connecting state would be good to have
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnected);
    // Server state must keep the initialized state
    BOOST_REQUIRE(ss.is_initialized() == true);
    std::vector<wsrep::view::member> members;
    members.push_back(wsrep::view::member(wsrep::id("s1"), "name", ""));
    wsrep::view view(wsrep::gtid(cluster_id, wsrep::seqno(1)),
                     wsrep::seqno(2),
                     wsrep::view::primary,
                     0, // capabilities
                     0, // own index
                     1, // protocol version
                     members);
    ss.on_connect(view);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_connected);
    // As storage engines have been initialized, there should not be
    // any reason to wait for initialization. State should jump directly
    // to s_joined after handling the view.
    ss.on_view(view, &hps);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joined);
    ss.on_sync();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
}

//
// Error after connecting to cluster. This scenario may happen if SST
// request preparation fails.
//
BOOST_FIXTURE_TEST_CASE(
    server_state_sst_first_error_on_connect,
    sst_first_server_fixture)
{
    connect_in_view(second_view);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_connected);
    disconnect();
}

// Error during SST.
BOOST_FIXTURE_TEST_CASE(
    server_state_sst_first_error_on_joiner,
    sst_first_server_fixture)
{
    connect_in_view(second_view);
    ss.prepare_for_sst();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joiner);
    server_service.position(wsrep::gtid::undefined());
    ss.sst_received(cc, 1);
    disconnect();
}

BOOST_FIXTURE_TEST_CASE(
    server_state_sst_first_error_on_initializing,
    sst_first_server_fixture)
{
    connect_in_view(second_view);
    ss.prepare_for_sst();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joiner);
    initialization_failure_action();
    server_service.position(wsrep::gtid(second_view.state_id()));
    BOOST_REQUIRE_EXCEPTION(ss.sst_received(cc, 0),
                            wsrep::runtime_error, exception_check);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_initializing);
    BOOST_REQUIRE_EXCEPTION(ss.on_view(second_view, &hps),
                            wsrep::runtime_error, exception_check);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_initializing);
    disconnect();
}

// Error or shutdown happens during catchup phase after receiving
// SST succesfully.
BOOST_FIXTURE_TEST_CASE(
    server_state_sst_first_error_on_joined,
    sst_first_server_fixture)
{
    connect_in_view(second_view);
    ss.prepare_for_sst();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joiner);
    sst_received_action();
    // Mock server service get_view() gets view from logged_view_.
    // Get_view() is called from sst_received(). This emulates the
    // case where SST contains the view in which SST happens.
    server_service.logged_view(second_view);
    server_service.position(wsrep::gtid(cluster_id, wsrep::seqno(2)));
    ss.sst_received(cc, 0);
    clear_sync_point_action();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joined);
    disconnect();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnected);
}

// Error or shutdown happens when donating a snapshot.
BOOST_FIXTURE_TEST_CASE(
    server_state_sst_first_error_on_donor,
    sst_first_server_fixture)
{
    bootstrap();
    ss.start_sst("", wsrep::gtid(cluster_id, wsrep::seqno(2)), false);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_donor);
    disconnect();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnected);
}

///////////////////////////////////////////////////////////////////////////////
//                     Test cases for init first                             //
///////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_CASE(server_state_init_first_boostrap,
                        init_first_server_fixture)
{
    bootstrap();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
}

BOOST_FIXTURE_TEST_CASE(server_state_init_first_join_with_sst,
                        init_first_server_fixture)
{
    ss.initialized();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_initialized);
    connect_in_view(second_view);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_connected);
    prepare_for_sst();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joiner);
    server_service.logged_view(second_view);
    server_service.position(wsrep::gtid(cluster_id, wsrep::seqno(2)));
    ss.sst_received(cc, 0);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joined);
    ss.on_sync();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
}

BOOST_FIXTURE_TEST_CASE(server_state_init_first_join_with_ist,
                        init_first_server_fixture)
{
    ss.initialized();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_initialized);
    connect_in_view(second_view);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_connected);
    server_service.logged_view(second_view);
    ss.on_view(second_view, &hps);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joined);
    ss.on_view(third_view, &hps);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joined);
    ss.on_sync();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
}


// Cycle from synced state to disconnected and back to synced. Server
// storage engines remain initialized.
BOOST_FIXTURE_TEST_CASE(
    server_state_init_first_synced_disconnected_synced_no_sst,
    init_first_server_fixture)
{
    bootstrap();
    ss.disconnect();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnecting);
    final_view();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnected);

    // Connect back as a sole member in the cluster
    BOOST_REQUIRE(ss.connect("cluster", "local", "0", false) == 0);
    // @todo: s_connecting state would be good to have
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnected);
    // Server state must keep the initialized state
    BOOST_REQUIRE(ss.is_initialized() == true);
    std::vector<wsrep::view::member> members;
    members.push_back(wsrep::view::member(wsrep::id("s1"), "name", ""));
    wsrep::view view(wsrep::gtid(cluster_id, wsrep::seqno(1)),
                     wsrep::seqno(2),
                     wsrep::view::primary,
                     0, // capabilities
                     0, // own index
                     1, // protocol version
                     members);
    ss.on_connect(view);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_connected);
    // As storage engines have been initialized, there should not be
    // any reason to wait for initialization. State should jump directly
    // to s_joined after handling the view.
    ss.on_view(view, &hps);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joined);
    ss.on_sync();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
}

/////////////////////////////////////////////////////////////////////////////
//                          Donor state transitions                        //
/////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_CASE(
    server_state_sst_first_donate_success,
    sst_first_server_fixture)
{
    bootstrap();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
    ss.start_sst("", wsrep::gtid(cluster_id, wsrep::seqno(2)), false);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_donor);
    ss.sst_sent(wsrep::gtid(cluster_id, wsrep::seqno(2)), 0);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joined);
    ss.on_sync();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
}

BOOST_FIXTURE_TEST_CASE(
    server_state_sst_first_donate_error,
    sst_first_server_fixture)
{
    bootstrap();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
    ss.start_sst("", wsrep::gtid(cluster_id, wsrep::seqno(2)), false);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_donor);
    ss.sst_sent(wsrep::gtid(cluster_id, wsrep::seqno(2)), 1);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joined);
    ss.on_sync();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
}

/////////////////////////////////////////////////////////////////////////////
//                    Pause/Resume and Desync/Resync                       //
/////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_CASE(
    server_state_sst_first_desync_and_pause_resync_and_resume,
    sst_first_server_fixture)
{
    bootstrap();
    ss.desync_and_pause();
    // @todo: Should we have here different state than synced
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
    ss.resume_and_resync();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
}

/////////////////////////////////////////////////////////////////////////////
//                               Disconnect                                //
/////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_CASE(
    server_state_disconnect,
    sst_first_server_fixture)
{
    bootstrap();
    ss.disconnect();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnecting);
    final_view();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnected);
}

// This test case verifies that the disconnect can be initiated
// concurrently by several callers. This may happen in failure situations
// where provider shutdown causes cascading failures and the failing operations
// try to disconnect the provider.
BOOST_FIXTURE_TEST_CASE(
    server_state_disconnect_twice,
    sst_first_server_fixture)
{
    bootstrap();
    ss.disconnect();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnecting);
    ss.disconnect();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnecting);
    final_view();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnected);
    ss.disconnect();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_disconnected);
}
