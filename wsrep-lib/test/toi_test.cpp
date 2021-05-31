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

BOOST_FIXTURE_TEST_CASE(test_toi_mode,
                        replicating_client_fixture_sync_rm)
{
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_local);
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_undefined);
    wsrep::key key(wsrep::key::exclusive);
    key.append_key_part("k1", 2);
    key.append_key_part("k2", 2);
    wsrep::key_array keys{key};
    wsrep::const_buffer buf("toi", 3);
    BOOST_REQUIRE(cc.enter_toi_local(keys, buf) == 0);
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_toi);
    BOOST_REQUIRE(cc.in_toi());
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_local);
    wsrep::mutable_buffer err;
    BOOST_REQUIRE(cc.leave_toi_local(err) == 0);
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_local);
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_undefined);
    BOOST_REQUIRE(sc.provider().toi_write_sets() == 1);
    BOOST_REQUIRE(sc.provider().toi_start_transaction() == 1);
    BOOST_REQUIRE(sc.provider().toi_commit() == 1);
}

BOOST_FIXTURE_TEST_CASE(test_toi_applying,
                        applying_client_fixture)
{
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_undefined);
    wsrep::ws_meta ws_meta(wsrep::gtid(wsrep::id("1"), wsrep::seqno(2)),
                           wsrep::stid(sc.id(),
                                       wsrep::transaction_id::undefined(),
                                       cc.id()),
                           wsrep::seqno(1),
                           wsrep::provider::flag::start_transaction |
                           wsrep::provider::flag::commit);
    cc.enter_toi_mode(ws_meta);
    BOOST_REQUIRE(cc.in_toi());
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_high_priority);
    cc.leave_toi_mode();
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_undefined);
    cc.after_applying();
}
