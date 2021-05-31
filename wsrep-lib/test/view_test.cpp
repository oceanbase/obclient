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

#include "wsrep/view.hpp"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(view_test_member_index)
{
    std::vector<wsrep::view::member> members;
    members.push_back(wsrep::view::member(wsrep::id("1"), "", ""));
    members.push_back(wsrep::view::member(wsrep::id("2"), "", ""));
    members.push_back(wsrep::view::member(wsrep::id("3"), "", ""));

    wsrep::view view(wsrep::gtid(wsrep::id("cluster"), wsrep::seqno(1)),
                     wsrep::seqno(1),
                     wsrep::view::primary,
                     0,
                     1,
                     0,
                     members);
    BOOST_REQUIRE(view.member_index(wsrep::id("1")) == 0);
    BOOST_REQUIRE(view.member_index(wsrep::id("2")) == 1);
    BOOST_REQUIRE(view.member_index(wsrep::id("3")) == 2);
    BOOST_REQUIRE(view.member_index(wsrep::id("4")) == -1);
}

BOOST_AUTO_TEST_CASE(view_test_equal_membership)
{
    std::vector<wsrep::view::member> m1;
    m1.push_back(wsrep::view::member(wsrep::id("1"), "", ""));
    m1.push_back(wsrep::view::member(wsrep::id("2"), "", ""));
    m1.push_back(wsrep::view::member(wsrep::id("3"), "", ""));

    std::vector<wsrep::view::member> m2;
    m2.push_back(wsrep::view::member(wsrep::id("2"), "", ""));
    m2.push_back(wsrep::view::member(wsrep::id("3"), "", ""));
    m2.push_back(wsrep::view::member(wsrep::id("1"), "", ""));

    std::vector<wsrep::view::member> m3;
    m3.push_back(wsrep::view::member(wsrep::id("1"), "", ""));
    m3.push_back(wsrep::view::member(wsrep::id("2"), "", ""));
    m3.push_back(wsrep::view::member(wsrep::id("3"), "", ""));
    m3.push_back(wsrep::view::member(wsrep::id("4"), "", ""));

    wsrep::view v1(wsrep::gtid(wsrep::id("cluster"), wsrep::seqno(1)),
                   wsrep::seqno(1),
                   wsrep::view::primary,
                   0,
                   1,
                   0,
                   m1);

    wsrep::view v2(wsrep::gtid(wsrep::id("cluster"), wsrep::seqno(1)),
                   wsrep::seqno(1),
                   wsrep::view::primary,
                   0,
                   1,
                   0,
                   m2);

    wsrep::view v3(wsrep::gtid(wsrep::id("cluster"), wsrep::seqno(1)),
                   wsrep::seqno(1),
                   wsrep::view::primary,
                   0,
                   1,
                   0,
                   m3);

    BOOST_REQUIRE(v1.equal_membership(v2));
    BOOST_REQUIRE(v2.equal_membership(v1));
    BOOST_REQUIRE(!v1.equal_membership(v3));
    BOOST_REQUIRE(!v3.equal_membership(v1));
}
