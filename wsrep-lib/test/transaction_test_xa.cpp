#include "client_state_fixture.hpp"
#include <iostream>

//
// Test a successful XA transaction lifecycle
//
BOOST_FIXTURE_TEST_CASE(transaction_xa,
                        replicating_client_fixture_sync_rm)
{
    wsrep::xid xid(1, 9, 0, "test xid");

    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    cc.assign_xid(xid);

    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    BOOST_REQUIRE(cc.before_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_preparing);
    BOOST_REQUIRE(tc.ordered() == false);
    // certified() only after the last fragment
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.after_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_prepared);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    // XA START + PREPARE fragment
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().fragments() == 1);

    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committing);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_ordered_commit);
    BOOST_REQUIRE(tc.ordered());
    BOOST_REQUIRE(tc.certified());
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committed);
    // XA PREPARE and XA COMMIT fragments
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);

    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}


//
// Test detaching of XA transactions
//
BOOST_FIXTURE_TEST_CASE(transaction_xa_detach_commit_by_xid,
                        replicating_two_clients_fixture_sync_rm)
{
    wsrep::xid xid(1, 1, 1, "id");

    cc1.start_transaction(wsrep::transaction_id(1));
    cc1.assign_xid(xid);
    cc1.before_prepare();
    cc1.after_prepare();
    BOOST_REQUIRE(sc.provider().fragments() == 1);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);

    cc1.xa_detach();

    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
    BOOST_REQUIRE(cc1.after_statement() == 0);

    cc2.start_transaction(wsrep::transaction_id(2));
    cc2.assign_xid(xid);
    BOOST_REQUIRE(cc2.client_state::commit_by_xid(xid) == 0);
    BOOST_REQUIRE(cc2.after_statement() == 0);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);

    // xa_detach() creates a streaming applier, clean it up
    wsrep::mock_high_priority_service* hps(
        static_cast<wsrep::mock_high_priority_service*>(
            sc.find_streaming_applier(xid)));
    BOOST_REQUIRE(hps);
    hps->rollback(wsrep::ws_handle(), wsrep::ws_meta());
    hps->after_apply();
    sc.stop_streaming_applier(sc.id(), wsrep::transaction_id(1));
    server_service.release_high_priority_service(hps);
}

BOOST_FIXTURE_TEST_CASE(transaction_xa_detach_rollback_by_xid,
                        replicating_two_clients_fixture_sync_rm)
{
    wsrep::xid xid(1, 1, 1, "id");

    cc1.start_transaction(wsrep::transaction_id(1));
    cc1.assign_xid(xid);
    cc1.before_prepare();
    cc1.after_prepare();
    BOOST_REQUIRE(sc.provider().fragments() == 1);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);

    cc1.xa_detach();

    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
    BOOST_REQUIRE(cc1.after_statement() == 0);

    cc2.start_transaction(wsrep::transaction_id(2));
    cc2.assign_xid(xid);
    BOOST_REQUIRE(cc2.rollback_by_xid(xid) == 0);
    BOOST_REQUIRE(cc2.after_statement() == 0);
    BOOST_REQUIRE(sc.provider().rollback_fragments() == 1);

    // xa_detach() creates a streaming applier, clean it up
    wsrep::mock_high_priority_service* hps(
        static_cast<wsrep::mock_high_priority_service*>(
            sc.find_streaming_applier(xid)));
    BOOST_REQUIRE(hps);
    hps->rollback(wsrep::ws_handle(), wsrep::ws_meta());
    hps->after_apply();
    sc.stop_streaming_applier(sc.id(), wsrep::transaction_id(1));
    server_service.release_high_priority_service(hps);
}


//
// Test XA replay
//
BOOST_FIXTURE_TEST_CASE(transaction_xa_replay,
                        replicating_client_fixture_sync_rm)
{
    wsrep::xid xid(1, 1, 1, "id");

    cc.start_transaction(wsrep::transaction_id(1));
    cc.assign_xid(xid);
    cc.before_prepare();
    cc.after_prepare();
    cc.after_command_before_result();
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_idle);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_prepared);
    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_replay);

    // this is normally done by rollbacker
    cc.xa_replay();
    cc.sync_rollback_complete();

    BOOST_REQUIRE(cc.unordered_replays() == 1);

    // xa_replay() createa a streaming applier, clean it up
    wsrep::mock_high_priority_service* hps(
        static_cast<wsrep::mock_high_priority_service*>(
            sc.find_streaming_applier(sc.id(), wsrep::transaction_id(1))));
    BOOST_REQUIRE(hps);
    hps->rollback(wsrep::ws_handle(), wsrep::ws_meta());
    hps->after_apply();
    sc.stop_streaming_applier(sc.id(), wsrep::transaction_id(1));
    server_service.release_high_priority_service(hps);
}

//
// Test a successful XA transaction lifecycle (applying side)
//
BOOST_FIXTURE_TEST_CASE(transaction_xa_applying,
                        applying_client_fixture)
{
    wsrep::xid xid(1, 9, 0, "test xid");

    start_transaction(wsrep::transaction_id(1), wsrep::seqno(1));
    cc.assign_xid(xid);

    BOOST_REQUIRE(cc.before_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_preparing);
    BOOST_REQUIRE(tc.ordered());
    BOOST_REQUIRE(tc.certified());
    BOOST_REQUIRE(tc.ws_meta().gtid().is_undefined() == false);
    BOOST_REQUIRE(cc.after_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_prepared);

    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committing);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_ordered_commit);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committed);
    cc.after_applying();
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committed);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

///////////////////////////////////////////////////////////////////////////////
//                       STREAMING REPLICATION                               //
///////////////////////////////////////////////////////////////////////////////

//
// Test a successful XA transaction lifecycle
//
BOOST_FIXTURE_TEST_CASE(transaction_xa_sr,
                        streaming_client_fixture_byte)
{
    wsrep::xid xid(1, 9, 0, "test xid");

    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    cc.assign_xid(xid);

    cc.bytes_generated_ = 1;
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    // XA START fragment with data
    BOOST_REQUIRE(sc.provider().fragments() == 1);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);

    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    BOOST_REQUIRE(cc.before_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_preparing);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.after_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_prepared);
    // XA PREPARE fragment
    BOOST_REQUIRE(sc.provider().fragments() == 2);

    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committing);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_ordered_commit);
    BOOST_REQUIRE(tc.ordered());
    BOOST_REQUIRE(tc.certified());
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committed);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    // XA START fragment (with data), XA PREPARE fragment and XA COMMIT fragment
    BOOST_REQUIRE(sc.provider().fragments() == 3);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}
