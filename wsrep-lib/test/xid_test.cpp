/*
 * Copyright (C) 2020 Codership Oy <info@codership.com>
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

#include "wsrep/xid.hpp"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(xid_test_is_null)
{
    wsrep::xid null_xid;
    BOOST_REQUIRE(null_xid.is_null());
    wsrep::xid test_xid(1,0,0,nullptr);
    BOOST_REQUIRE(!test_xid.is_null());
}

BOOST_AUTO_TEST_CASE(xid_test_equal)
{
    wsrep::xid a(1,1,1,"ab");
    wsrep::xid b(1,1,1,"ab");
    BOOST_REQUIRE(a == b);
}

BOOST_AUTO_TEST_CASE(xid_test_null_equal)
{
    wsrep::xid a;
    wsrep::xid b;
    BOOST_REQUIRE(a == b);
    BOOST_REQUIRE(a.is_null());
}

BOOST_AUTO_TEST_CASE(xid_test_not_equal)
{
    wsrep::xid a(1,1,0,"a");
    wsrep::xid b(1,0,1,"a");
    wsrep::xid c(-1,1,0,"a");
    wsrep::xid d(1,1,0,"b");
    BOOST_REQUIRE(!(a == b));
    BOOST_REQUIRE(!(a == c));
    BOOST_REQUIRE(!(a == d));
}

BOOST_AUTO_TEST_CASE(xid_clear)
{
    wsrep::xid null_xid;
    wsrep::xid to_clear(1, 1, 0, "a");
    to_clear.clear();
    BOOST_REQUIRE(to_clear.is_null());
    BOOST_REQUIRE(null_xid == to_clear);
}

BOOST_AUTO_TEST_CASE(xid_to_string)
{
    wsrep::xid null_xid;
    std::stringstream null_xid_str;
    null_xid_str << null_xid;
    BOOST_REQUIRE(null_xid_str.str() == "");

    wsrep::xid test_xid(1,4,0,"test");
    std::string xid_str(to_string(test_xid));
    BOOST_REQUIRE(xid_str == "test");
}

static bool exception_check(const wsrep::runtime_error&)
{
    return true;
}

BOOST_AUTO_TEST_CASE(xid_too_big)
{
    std::string s(65,'a');
    BOOST_REQUIRE_EXCEPTION(wsrep::xid a(1, 65, 0, s.c_str()),
                            wsrep::runtime_error, exception_check);
    BOOST_REQUIRE_EXCEPTION(wsrep::xid b(1, 0, 65, s.c_str()),
                            wsrep::runtime_error, exception_check);
}
