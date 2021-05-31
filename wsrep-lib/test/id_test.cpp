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

#include "wsrep/id.hpp"
#include <boost/test/unit_test.hpp>

#include <sstream>

namespace
{
    bool exception_check(const wsrep::runtime_error&) { return true; }
}

BOOST_AUTO_TEST_CASE(id_test_uuid)
{
    std::string uuid_str("6a20d44a-6e17-11e8-b1e2-9061aec0cdad");
    wsrep::id id(uuid_str);
    std::ostringstream os;
    os << id;
    BOOST_REQUIRE(uuid_str == os.str());
}

BOOST_AUTO_TEST_CASE(id_test_string)
{
    std::string id_str("1234567890123456");
    wsrep::id id(id_str);
    std::ostringstream os;
    os << id;
    BOOST_REQUIRE(id_str == os.str());
}

BOOST_AUTO_TEST_CASE(id_test_string_too_long)
{
    std::string id_str("12345678901234567");
    BOOST_REQUIRE_EXCEPTION(wsrep::id id(id_str), wsrep::runtime_error,
                            exception_check);
}

BOOST_AUTO_TEST_CASE(id_test_binary)
{
    char data[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4 ,5 ,6};
    wsrep::id id(data, sizeof(data));
    std::ostringstream os;
    os << id;
    BOOST_REQUIRE(os.str() == "01020304-0506-0708-0900-010203040506");
}

BOOST_AUTO_TEST_CASE(id_test_binary_too_long)
{
    char data[17] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4 ,5 ,6, 7};
    BOOST_REQUIRE_EXCEPTION(wsrep::id id(data, sizeof(data)),
                            wsrep::runtime_error, exception_check);;
}
