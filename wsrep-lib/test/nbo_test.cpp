/*
 * Copyright (C) 2019 Codership Oy <info@codership.com>
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

#include "wsrep/client_state.hpp"

#include "client_state_fixture.hpp"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_CASE(test_local_nbo,
                        replicating_client_fixture_sync_rm)
{
    // NBO is executed in two consecutive TOI operations
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_local);
    // First phase certifies the write set and enters TOI
    wsrep::key key(wsrep::key::exclusive);
    key.append_key_part("k1", 2);
    key.append_key_part("k2", 2);
    wsrep::key_array keys{key};
    std::string data("data");
    BOOST_REQUIRE(cc.begin_nbo_phase_one(
                      keys,
                      wsrep::const_buffer(data.data(),
                                          data.size())) == 0);
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_nbo);
    BOOST_REQUIRE(cc.in_toi());
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_local);
    // After required resoureces have been grabbed, NBO leave should
    // end TOI but leave the client state in m_nbo.
    const wsrep::mutable_buffer err;
    BOOST_REQUIRE(cc.end_nbo_phase_one(err) == 0);
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_nbo);
    BOOST_REQUIRE(cc.in_toi() == false);
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_undefined);
    // Second phase replicates the NBO end event and grabs TOI
    // again for finalizing the NBO.
    BOOST_REQUIRE(cc.begin_nbo_phase_two(keys) == 0);
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_nbo);
    BOOST_REQUIRE(cc.in_toi());
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_local);
    // End of NBO phase will leave TOI and put the client state back to
    // m_local
    BOOST_REQUIRE(cc.end_nbo_phase_two(err) == 0);
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_local);
    BOOST_REQUIRE(cc.in_toi() == false);
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_undefined);

    // There must have been two toi write sets, one with
    // start transaction flag, another with commit flag.
    BOOST_REQUIRE(sc.provider().toi_write_sets() == 2);
    BOOST_REQUIRE(sc.provider().toi_start_transaction() == 1);
    BOOST_REQUIRE(sc.provider().toi_commit() == 1);
}

BOOST_FIXTURE_TEST_CASE(test_local_nbo_cert_failure,
                        replicating_client_fixture_sync_rm)
{
    // NBO is executed in two consecutive TOI operations
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_local);
    // First phase certifies the write set and enters TOI
    wsrep::key key(wsrep::key::exclusive);
    key.append_key_part("k1", 2);
    key.append_key_part("k2", 2);
    wsrep::key_array keys{key};
    std::string data("data");
    sc.provider().certify_result_ = wsrep::provider::error_certification_failed;
    BOOST_REQUIRE(cc.begin_nbo_phase_one(
                      keys,
                      wsrep::const_buffer(data.data(),
                                          data.size())) == 1);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    BOOST_REQUIRE(cc.current_error_status() ==
                  wsrep::provider::error_certification_failed);
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_local);
    BOOST_REQUIRE(cc.in_toi() == false);
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_undefined);
}

// This test case operates through server_state object in order to
// verify that the high priority service is called with appropriate
// arguments.
BOOST_FIXTURE_TEST_CASE(test_applying_nbo,
                        applying_client_fixture)
{

    wsrep::mock_high_priority_service hps(sc, &cc, false);
    wsrep::ws_handle ws_handle(wsrep::transaction_id::undefined(), (void*)(1));
    const int nbo_begin_flags(wsrep::provider::flag::start_transaction |
                              wsrep::provider::flag::isolation);
    wsrep::ws_meta ws_meta(wsrep::gtid(wsrep::id("cluster1"),
                                       wsrep::seqno(1)),
                           wsrep::stid(wsrep::id("s1"),
                                       wsrep::transaction_id::undefined(),
                                       wsrep::client_id(1)),
                           wsrep::seqno(0),
                           nbo_begin_flags);
    std::string nbo_begin("nbo_begin");
    BOOST_REQUIRE(sc.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer(nbo_begin.data(),
                                                  nbo_begin.size())) == 0);
    wsrep::mock_client* nbo_cs(hps.nbo_cs());
    BOOST_REQUIRE(nbo_cs);
    BOOST_REQUIRE(nbo_cs->toi_mode() == wsrep::client_state::m_undefined);
    BOOST_REQUIRE(nbo_cs->mode() == wsrep::client_state::m_nbo);

    // After this point the control is on local process and applier
    // has released toi critical section.
    wsrep::key key(wsrep::key::exclusive);
    key.append_key_part("k1", 2);
    key.append_key_part("k2", 2);
    wsrep::key_array keys{key};
    // Starting phase two should put nbo_cs in toi mode.
    BOOST_REQUIRE(nbo_cs->begin_nbo_phase_two(keys) == 0);
    BOOST_REQUIRE(nbo_cs->mode() == wsrep::client_state::m_nbo);
    BOOST_REQUIRE(nbo_cs->in_toi());
    BOOST_REQUIRE(nbo_cs->toi_mode() == wsrep::client_state::m_local);
    // Ending phase two should make nbo_cs leave TOI mode and
    // return to m_local mode.
    const wsrep::mutable_buffer err;
    BOOST_REQUIRE(nbo_cs->end_nbo_phase_two(err) == 0);
    BOOST_REQUIRE(nbo_cs->mode() == wsrep::client_state::m_local);
    BOOST_REQUIRE(nbo_cs->in_toi() == false);
    BOOST_REQUIRE(nbo_cs->toi_mode() == wsrep::client_state::m_undefined);

    // There must have been one toi write set with commit flag.
    BOOST_REQUIRE(sc.provider().toi_write_sets() == 1);
    BOOST_REQUIRE(sc.provider().toi_start_transaction() == 0);
    BOOST_REQUIRE(sc.provider().toi_commit() == 1);
}

// This test case operates through server_state object in order to
// verify that the high priority service is called with appropriate
// arguments. The test checks that error is returned in the case if
// launching asynchronous process for NBO fails.
BOOST_FIXTURE_TEST_CASE(test_applying_nbo_fail,
                        applying_client_fixture)
{

    wsrep::mock_high_priority_service hps(sc, &cc, false);
    wsrep::ws_handle ws_handle(wsrep::transaction_id::undefined(), (void*)(1));
    const int nbo_begin_flags(wsrep::provider::flag::start_transaction |
                              wsrep::provider::flag::isolation);
    wsrep::ws_meta ws_meta(wsrep::gtid(wsrep::id("cluster1"),
                                       wsrep::seqno(1)),
                           wsrep::stid(wsrep::id("s1"),
                                       wsrep::transaction_id::undefined(),
                                       wsrep::client_id(1)),
                           wsrep::seqno(0),
                           nbo_begin_flags);
    std::string nbo_begin("nbo_begin");
    hps.fail_next_toi_ = true;
    BOOST_REQUIRE(sc.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer(nbo_begin.data(),
                                                  nbo_begin.size())) == 1);
}
