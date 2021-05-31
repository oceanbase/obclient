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

#include "wsrep/transaction.hpp"
#include "wsrep/provider.hpp"

#include "test_utils.hpp"
#include "client_state_fixture.hpp"

#include <boost/mpl/vector.hpp>

namespace
{
    typedef
    boost::mpl::vector<replicating_client_fixture_sync_rm,
                       replicating_client_fixture_async_rm>
    replicating_fixtures;
}

BOOST_FIXTURE_TEST_CASE(transaction_append_key_data,
                        replicating_client_fixture_sync_rm)
{
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.is_empty());
    int vals[3] = {1, 2, 3};
    wsrep::key key(wsrep::key::exclusive);
    for (int i(0); i < 3; ++i)
    {
        key.append_key_part(&vals[i], sizeof(vals[i]));
    }
    BOOST_REQUIRE(cc.append_key(key) == 0);
    BOOST_REQUIRE(tc.is_empty() == false);
    wsrep::const_buffer data(&vals[2], sizeof(vals[2]));
    BOOST_REQUIRE(cc.append_data(data) == 0);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    cc.after_statement();
}
//
// Test a succesful 1PC transaction lifecycle
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(transaction_1pc, T,
                                 replicating_fixtures, T)
{
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);
    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    // Establish default read view
    BOOST_REQUIRE(0 == cc.assign_read_view(NULL));

    // Verify that the commit can be succesfully executed in separate command
    BOOST_REQUIRE(cc.after_statement() == 0);
    cc.after_command_before_result();
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    BOOST_REQUIRE(cc.before_command() == 0);
    BOOST_REQUIRE(cc.before_statement() == 0);
    // Run before commit
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committing);

    // Run ordered commit
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_ordered_commit);

    // Run after commit
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committed);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}


//
// Test a voluntary rollback
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(transaction_rollback, T,
                                 replicating_fixtures, T)
{
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    // Run before commit
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborting);

    // Run after commit
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

//
// Test a 1PC transaction which gets BF aborted before before_commit
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_bf_before_before_commit, T,
    replicating_fixtures, T)
{
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    wsrep_test::bf_abort_unordered(cc);

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}



//
// Test a 1PC transaction which gets BF aborted during before_commit via
// provider before the write set was ordered and certified.
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_bf_during_before_commit_uncertified, T,
    replicating_fixtures, T)
{
    wsrep::mock_server_state& sc(T::sc);
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    wsrep_test::bf_abort_provider(sc, tc, wsrep::seqno::undefined());

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_cert_failed);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a 1PC transaction which gets BF aborted during before_commit
// when waiting for replayers
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_bf_during_commit_wait_for_replayers, T,
    replicating_fixtures, T)
{
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    cc.bf_abort_during_wait_ = true;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_bf_during_commit_order_enter, T,
    replicating_fixtures, T)
{
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);
    auto& sc(T::sc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    sc.provider().commit_order_enter_result_ = wsrep::provider::error_bf_abort;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_replay);
    BOOST_REQUIRE(cc.will_replay_called() == true);
    BOOST_REQUIRE(tc.certified() == true);
    BOOST_REQUIRE(tc.ordered() == true);

    sc.provider().commit_order_enter_result_ = wsrep::provider::success;

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_replay);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_replay);

    // Replay from after_statement()
    cc.after_statement();
    BOOST_REQUIRE(cc.replays() > 0);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(not cc.current_error());
}

//
// Test a 1PC transaction for which prepare data fails
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_error_during_prepare_data, T,
    replicating_fixtures, T)
{
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    cc.error_during_prepare_data_ = true;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_size_exceeded_error);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a 1PC transaction which gets killed by DBMS before certification
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_killed_before_certify, T,
    replicating_fixtures, T)
{
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    cc.killed_before_certify_ = true;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_interrupted_error);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a transaction which gets BF aborted inside provider before
// certification result is known. Replaying will be successful
//
BOOST_FIXTURE_TEST_CASE(
    transaction_bf_before_cert_result_replay_success,
    replicating_client_fixture_sync_rm)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    sc.provider().certify_result_ = wsrep::provider::error_bf_abort;
    sc.provider().replay_result_ = wsrep::provider::success;

    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_replay);
    BOOST_REQUIRE(cc.will_replay_called() == true);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committed);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

//
// Test a transaction which gets BF aborted inside provider before
// certification result is known. Replaying will fail because of
// certification failure.
//
BOOST_FIXTURE_TEST_CASE(
    transaction_bf_before_cert_result_replay_cert_fail,
    replicating_client_fixture_sync_rm)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    sc.provider().certify_result_ = wsrep::provider::error_bf_abort;
    sc.provider().replay_result_ = wsrep::provider::error_certification_failed;

    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_replay);
    BOOST_REQUIRE(cc.will_replay_called() == true);
    BOOST_REQUIRE(cc.after_statement() );
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    BOOST_REQUIRE(tc.active() == false);
}

//
// Test a 1PC transaction which gets BF aborted during before_commit via
// provider after the write set was ordered and certified. This must
// result replaying of transaction.
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_bf_during_before_commit_certified, T,
    replicating_fixtures, T)
{
    wsrep::mock_server_state& sc(T::sc);
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    wsrep_test::bf_abort_provider(sc, tc, wsrep::seqno(1));

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_replay);
    BOOST_REQUIRE(cc.will_replay_called() == true);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == true);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_replay);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_replay);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    BOOST_REQUIRE(cc.replays() == 1);
}

//
// Test a 1PC transaction which gets BF aborted simultaneously with
// certification failure. BF abort should not succeed as the
// transaction is going to roll back anyway. Certification failure
// should not generate seqno for write set meta.
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_bf_before_unordered_cert_failure, T,
    replicating_fixtures, T)
{
    wsrep::mock_server_state& sc(T::sc);
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
    cc.sync_point_enabled_ = "wsrep_before_certification";
    cc.sync_point_action_ = wsrep::mock_client_service::spa_bf_abort_unordered;
    sc.provider().certify_result_ = wsrep::provider::error_certification_failed;
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_cert_failed);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
    BOOST_REQUIRE(cc.after_statement() );
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
}

//
// Test a 1PC transaction which gets "warning error" from certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_warning_error_from_certify, T,
    replicating_fixtures, T)
{
    wsrep::mock_server_state& sc(T::sc);
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    sc.provider().certify_result_ = wsrep::provider::error_warning;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    sc.provider().certify_result_ = wsrep::provider::success;

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_error_during_commit);
}

//
// Test a 1PC transaction which gets transaction missing from certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_transaction_missing_from_certify, T,
    replicating_fixtures, T)
{
    wsrep::mock_server_state& sc(T::sc);
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    sc.provider().certify_result_ = wsrep::provider::error_transaction_missing;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    sc.provider().certify_result_ = wsrep::provider::success;

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_error_during_commit);
}

//
// Test a 1PC transaction which gets size exceeded error from certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_size_exceeded_from_certify, T,
    replicating_fixtures, T)
{
    wsrep::mock_server_state& sc(T::sc);
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    sc.provider().certify_result_ = wsrep::provider::error_size_exceeded;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    sc.provider().certify_result_ = wsrep::provider::success;

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_error_during_commit);
}

//
// Test a 1PC transaction which gets connection failed error from certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_connection_failed_from_certify, T,
    replicating_fixtures, T)
{
    wsrep::mock_server_state& sc(T::sc);
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    sc.provider().certify_result_ = wsrep::provider::error_connection_failed;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    sc.provider().certify_result_ = wsrep::provider::success;

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_error_during_commit);
}

//
// Test a 1PC transaction which gets not allowed error from certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_no_allowed_from_certify, T,
    replicating_fixtures, T)
{
    wsrep::mock_server_state& sc(T::sc);
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    sc.provider().certify_result_ = wsrep::provider::error_not_allowed;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    sc.provider().certify_result_ = wsrep::provider::success;

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_error_during_commit);
}

//
// Test a 1PC transaction which gets fatal error from certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_fatal_from_certify, T,
    replicating_fixtures, T)
{
    wsrep::mock_server_state& sc(T::sc);
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    sc.provider().certify_result_ = wsrep::provider::error_fatal;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    sc.provider().certify_result_ = wsrep::provider::success;

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_error_during_commit);
    BOOST_REQUIRE(cc.aborts() == 1);
}

//
// Test a 1PC transaction which gets unknown from certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_unknown_from_certify, T,
    replicating_fixtures, T)
{
    wsrep::mock_server_state& sc(T::sc);
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    sc.provider().certify_result_ = wsrep::provider::error_unknown;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    sc.provider().certify_result_ = wsrep::provider::success;

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_error_during_commit);
}

//
// Test a 1PC transaction which gets BF aborted before grabbing lock
// after certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_bf_abort_before_certify_regain_lock, T,
    replicating_fixtures, T)
{
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    cc.sync_point_enabled_ = "wsrep_after_certification";
    cc.sync_point_action_ = wsrep::mock_client_service::spa_bf_abort_ordered;
    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_replay);
    BOOST_REQUIRE(cc.will_replay_called() == true);
    BOOST_REQUIRE(tc.certified() == true);
    BOOST_REQUIRE(tc.ordered() == true);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_replay);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_replay);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(cc.replays() == 1);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

//
// Test a transaction which gets BF aborted before before_statement.
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_bf_before_before_statement, T,
    replicating_fixtures, T)
{
    wsrep::client_state& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
    cc.after_statement();
    cc.after_command_before_result();
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.before_command() == 0);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(cc.before_statement() == 1);
    BOOST_REQUIRE(tc.active());
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_after_result();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
}

//
// Test a transaction which gets BF aborted before after_statement.
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_bf_before_after_statement, T,
    replicating_fixtures, T)
{
    wsrep::client_state& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    wsrep_test::bf_abort_unordered(cc);

    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_bf_abort_after_after_statement, T,
    replicating_fixtures, T)
{
    wsrep::client_state& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    cc.after_statement();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_exec);
    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
}

BOOST_FIXTURE_TEST_CASE(
    transaction_1pc_autocommit_retry_bf_aborted,
    replicating_client_fixture_autocommit)
{

    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(cc.after_statement());
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_exec);
}

BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_1pc_bf_abort_after_after_command_before_result, T,
    replicating_fixtures, T)
{
    wsrep::client_state& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    cc.after_statement();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_exec);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_result);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    wsrep_test::bf_abort_unordered(cc);
    // The result is being sent to client. We need to mark transaction
    // as must_abort but not override error yet as this might cause
    // a race condition resulting incorrect result returned to the DBMS client.
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    // After the result has been sent to the DBMS client, the after result
    // processing should roll back the transaction and set the error.
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_idle);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    BOOST_REQUIRE(tc.active() == true);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
    cc.sync_rollback_complete();
    BOOST_REQUIRE(cc.before_command() == 1);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

BOOST_FIXTURE_TEST_CASE(
    transaction_1pc_bf_abort_after_after_command_after_result_sync_rm,
    replicating_client_fixture_sync_rm)
{
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    cc.after_statement();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_exec);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_result);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_idle);
    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
    BOOST_REQUIRE(tc.active());
    cc.sync_rollback_complete();
    BOOST_REQUIRE(cc.before_command() == 1);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    BOOST_REQUIRE(tc.active() == false);
}

// Check the case where client program calls wait_rollback_complete() to
// gain control before before_command().
BOOST_FIXTURE_TEST_CASE(
    transaction_1pc_bf_abort_after_after_command_after_result_sync_rm_wait_rollback,
    replicating_client_fixture_sync_rm)
{
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    cc.after_statement();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_exec);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_result);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_idle);
    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
    BOOST_REQUIRE(tc.active());
    cc.sync_rollback_complete();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_idle);
    cc.wait_rollback_complete_and_acquire_ownership();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_exec);
    // Idempotent
    cc.wait_rollback_complete_and_acquire_ownership();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_exec);
    BOOST_REQUIRE(cc.before_command() == 1);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    BOOST_REQUIRE(tc.active() == false);
}

// Check the case where BF abort happens between client calls to
// wait_rollback_complete_and_acquire_ownership()
// and before before_command().
BOOST_FIXTURE_TEST_CASE(
    transaction_1pc_bf_abort_after_acquire_before_before_command_sync_rm,
    replicating_client_fixture_sync_rm)
{
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    cc.after_statement();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_exec);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_result);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_idle);
    cc.wait_rollback_complete_and_acquire_ownership();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_exec);
    // As the control is now on client, the BF abort must just change
    // the state to s_must_abort.
    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(cc.before_command() == 1);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    BOOST_REQUIRE(tc.active() == false);
}

BOOST_FIXTURE_TEST_CASE(
    transaction_1pc_bf_abort_after_after_command_after_result_async_rm,
    replicating_client_fixture_async_rm)
{
    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    cc.after_statement();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_exec);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_result);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_idle);
    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(cc.before_command() == 1);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    BOOST_REQUIRE(tc.active() == false);
}

//
// Test before_command() with keep_command_error param
// Failure free case is not affected by keep_command_error
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(transaction_keep_error, T,
                                 replicating_fixtures, T)
{
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    cc.start_transaction(wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.active());
    cc.after_statement();
    cc.after_command_before_result();
    cc.after_command_after_result();

    bool keep_command_error(true);
    BOOST_REQUIRE(cc.before_command(keep_command_error) == 0);
    BOOST_REQUIRE(tc.active());
    cc.after_command_before_result();
    cc.after_command_after_result();

    keep_command_error = false;
    BOOST_REQUIRE(cc.before_command(keep_command_error) == 0);
    BOOST_REQUIRE(cc.before_statement() == 0);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    cc.after_statement();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

//
// Test before_command() with keep_command_error param
// BF abort while idle
//
BOOST_FIXTURE_TEST_CASE(transaction_keep_error_bf_idle_sync_rm,
                        replicating_client_fixture_sync_rm)
{
    cc.start_transaction(wsrep::transaction_id(1));
    cc.after_statement();
    cc.after_command_before_result();
    cc.after_command_after_result();

    BOOST_REQUIRE(cc.state() == wsrep::client_state::s_idle);
    wsrep_test::bf_abort_unordered(cc);
    cc.sync_rollback_complete();
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);

    bool keep_command_error(true);
    BOOST_REQUIRE(cc.before_command(keep_command_error) == 0);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_before_result();
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);

    keep_command_error = false;
    BOOST_REQUIRE(cc.before_command(keep_command_error) == 1);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_before_result();
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

//
// Test before_command() with keep_command_error param
// BF abort after ownership is acquired and before before_command()
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(transaction_keep_error_bf_after_ownership, T,
                                 replicating_fixtures, T)
{
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    cc.start_transaction(wsrep::transaction_id(1));
    cc.after_statement();
    cc.after_command_before_result();
    cc.after_command_after_result();

    cc.wait_rollback_complete_and_acquire_ownership();
    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);

    bool keep_command_error(true);
    BOOST_REQUIRE(cc.before_command(keep_command_error) == 0);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_before_result();
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);

    keep_command_error = false;
    BOOST_REQUIRE(cc.before_command(keep_command_error) == 1);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_before_result();
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

//
// Test before_command() with keep_command_error param
// BF abort right after before_command()
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(transaction_keep_error_bf_after_before_command, T,
                                 replicating_fixtures, T)
{
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    cc.start_transaction(wsrep::transaction_id(1));
    cc.after_statement();
    cc.after_command_before_result();
    cc.after_command_after_result();

    bool keep_command_error(true);
    BOOST_REQUIRE(cc.before_command(keep_command_error) == 0);
    BOOST_REQUIRE(tc.active());

    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    cc.after_command_before_result();
    cc.after_command_after_result();
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);

    keep_command_error = false;
    BOOST_REQUIRE(cc.before_command(keep_command_error) == 1);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_before_result();
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

//
// Test before_command() with keep_command_error param
// BF abort right after after_command_before_result()
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(transaction_keep_error_bf_after_after_command_before_result, T,
                                 replicating_fixtures, T)
{
    wsrep::mock_client& cc(T::cc);
    const wsrep::transaction& tc(T::tc);

    cc.start_transaction(wsrep::transaction_id(1));
    cc.after_statement();
    cc.after_command_before_result();
    cc.after_command_after_result();

    bool keep_command_error(true);
    BOOST_REQUIRE(cc.before_command(keep_command_error) == 0);
    BOOST_REQUIRE(tc.active());

    cc.after_command_before_result();

    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);

    cc.after_command_after_result();
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);

    keep_command_error = false;
    BOOST_REQUIRE(cc.before_command(keep_command_error) == 1);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_before_result();
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

BOOST_FIXTURE_TEST_CASE(transaction_1pc_applying,
                        applying_client_fixture)
{
    start_transaction(wsrep::transaction_id(1),
                      wsrep::seqno(1));
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


BOOST_FIXTURE_TEST_CASE(transaction_applying_rollback,
                        applying_client_fixture)
{
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
    cc.after_applying();
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

///////////////////////////////////////////////////////////////////////////////
//                       STREAMING REPLICATION                               //
///////////////////////////////////////////////////////////////////////////////

//
// Test 1PC with row streaming with one row
//
BOOST_FIXTURE_TEST_CASE(transaction_row_streaming_1pc_commit,
                        streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}

//
// Test 1PC with row streaming with one row
//
BOOST_FIXTURE_TEST_CASE(transaction_row_batch_streaming_1pc_commit,
                        streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.enable_streaming(
                      wsrep::streaming_context::row, 2) == 0);
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}

//
// Test 1PC row streaming with two separate statements
//
BOOST_FIXTURE_TEST_CASE(
    transaction_row_streaming_1pc_commit_two_statements,
    streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(cc.before_statement() == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 2);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(sc.provider().fragments() == 3);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}

//
// Fragments are removed in before_prepare in running transaction context.
// In 1pc the before_prepare() is called from before_commit().
// However, the BF abort may arrive during this removal and the
// client_service::remove_fragments() may roll back the transaction
// internally. This will cause the transaction to leave before_prepare()
// in aborted state.
//
BOOST_FIXTURE_TEST_CASE(transaction_streaming_1pc_bf_abort_during_fragment_removal,
                        streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    cc.bf_abort_during_fragment_removal_ = true;
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
    BOOST_REQUIRE(cc.after_statement());
    BOOST_REQUIRE(tc.active() == false);
    wsrep_test::terminate_streaming_applier(sc, sc.id(),
                                            wsrep::transaction_id(1));
}

//
// Test streaming rollback
//
BOOST_FIXTURE_TEST_CASE(transaction_row_streaming_rollback,
                        streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().rollback_fragments() == 1);

    wsrep::high_priority_service* hps(
        sc.find_streaming_applier(
            sc.id(), wsrep::transaction_id(1)));
    BOOST_REQUIRE(hps);
    hps->rollback(wsrep::ws_handle(), wsrep::ws_meta());
    hps->after_apply();
    sc.stop_streaming_applier(sc.id(), wsrep::transaction_id(1));
    server_service.release_high_priority_service(hps);
}

//
// Test streaming BF abort in executing state.
//
BOOST_FIXTURE_TEST_CASE(transaction_row_streaming_bf_abort_executing,
                        streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(tc.streaming_context().rolled_back());
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(cc.after_statement());
    wsrep_test::terminate_streaming_applier(sc, sc.id(),
                                            wsrep::transaction_id(1));

}
//
// Test streaming certification failure during fragment replication
//
BOOST_FIXTURE_TEST_CASE(transaction_row_streaming_cert_fail_non_commit,
                        streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    sc.provider().certify_result_ = wsrep::provider::error_certification_failed;
    BOOST_REQUIRE(cc.after_row() == 1);
    sc.provider().certify_result_ = wsrep::provider::success;
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(cc.after_statement() == 1);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().rollback_fragments() == 1);

    wsrep::high_priority_service* hps(
        sc.find_streaming_applier(
            sc.id(), wsrep::transaction_id(1)));
    BOOST_REQUIRE(hps);
    hps->rollback(wsrep::ws_handle(), wsrep::ws_meta());
    hps->after_apply();
    sc.stop_streaming_applier(sc.id(), wsrep::transaction_id(1));
    server_service.release_high_priority_service(hps);
}

//
// Test streaming certification failure during commit
//
BOOST_FIXTURE_TEST_CASE(transaction_row_streaming_cert_fail_commit,
                        streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    sc.provider().certify_result_ = wsrep::provider::error_certification_failed;
    BOOST_REQUIRE(cc.before_commit() == 1);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_cert_failed);
    sc.provider().certify_result_ = wsrep::provider::success;
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(cc.after_statement() );
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_aborted);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().rollback_fragments() == 1);

    wsrep::high_priority_service* hps(
        sc.find_streaming_applier(
            sc.id(), wsrep::transaction_id(1)));
    BOOST_REQUIRE(hps);
    hps->rollback(wsrep::ws_handle(), wsrep::ws_meta());
    hps->after_apply();
    sc.stop_streaming_applier(sc.id(), wsrep::transaction_id(1));
    server_service.release_high_priority_service(hps);
}

//
// Test streaming BF abort after succesful certification
//
BOOST_FIXTURE_TEST_CASE(transaction_row_streaming_bf_abort_committing,
                        streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committing);
    wsrep_test::bf_abort_ordered(cc);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_abort);
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_must_replay);
    BOOST_REQUIRE(cc.will_replay_called() == true);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committed);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}



BOOST_FIXTURE_TEST_CASE(transaction_byte_streaming_1pc_commit,
                        streaming_client_fixture_byte)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    cc.bytes_generated_ = 1;
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}

BOOST_FIXTURE_TEST_CASE(transaction_byte_batch_streaming_1pc_commit,
                        streaming_client_fixture_byte)
{
    BOOST_REQUIRE(
        cc.enable_streaming(
            wsrep::streaming_context::bytes, 2) == 0);
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}


BOOST_FIXTURE_TEST_CASE(transaction_statement_streaming_statement_with_no_effect,
                        streaming_client_fixture_statement)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 0);
    BOOST_REQUIRE(cc.before_statement() == 0);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(cc.before_statement() == 0);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_statement() == 0);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}

BOOST_FIXTURE_TEST_CASE(transaction_statement_streaming_1pc_commit,
                        streaming_client_fixture_statement)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 0);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_statement() == 0);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}

BOOST_FIXTURE_TEST_CASE(transaction_statement_batch_streaming_1pc_commit,
                        streaming_client_fixture_statement)
{
    BOOST_REQUIRE(
        cc.enable_streaming(
            wsrep::streaming_context::statement, 2) == 0);
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 0);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 0);
    BOOST_REQUIRE(cc.before_statement() == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 0);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_statement() == 0);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}

BOOST_FIXTURE_TEST_CASE(transaction_statement_streaming_cert_fail,
                        streaming_client_fixture_statement)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 0);
    sc.provider().certify_result_ = wsrep::provider::error_certification_failed;
    BOOST_REQUIRE(cc.after_statement());
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    // Note: Due to possible limitation in wsrep-API error codes
    // or a bug in current Galera provider, rollback fragment may be
    // replicated even in case of certification failure.
    // If the limitation is lifted later on or the provider is fixed,
    // the above check should be change for fragments == 0,
    // rollback_fragments == 0.
    BOOST_REQUIRE(sc.provider().fragments() == 1);
    BOOST_REQUIRE(sc.provider().start_fragments() == 0);
    BOOST_REQUIRE(sc.provider().rollback_fragments() == 1);

    wsrep::high_priority_service* hps(
        sc.find_streaming_applier(
            sc.id(), wsrep::transaction_id(1)));
    BOOST_REQUIRE(hps);
    hps->rollback(wsrep::ws_handle(), wsrep::ws_meta());
    hps->after_apply();
    sc.stop_streaming_applier(sc.id(), wsrep::transaction_id(1));
    server_service.release_high_priority_service(hps);
}

///////////////////////////////////////////////////////////////////////////////
//                             misc                                          //
///////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(transaction_state_strings)
{
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::transaction::s_executing) == "executing");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::transaction::s_preparing) == "preparing");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction::s_certifying) == "certifying");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction::s_committing) == "committing");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction::s_ordered_commit) == "ordered_commit");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction::s_committed) == "committed");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction::s_cert_failed) == "cert_failed");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction::s_must_abort) == "must_abort");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction::s_aborting) == "aborting");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction::s_aborted) == "aborted");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction::s_must_replay) == "must_replay");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction::s_replaying) == "replaying");
}
