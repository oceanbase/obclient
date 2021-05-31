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

#include "wsrep/gtid.hpp"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(gtid_test_scan_from_string_uuid)
{
    std::string gtid_str("6a20d44a-6e17-11e8-b1e2-9061aec0cdad:123456");
    wsrep::gtid gtid;
    ssize_t ret(wsrep::scan_from_c_str(
                      gtid_str.c_str(),
                      gtid_str.size(), gtid));
    BOOST_REQUIRE_MESSAGE(ret == ssize_t(gtid_str.size()),
                          "Expected " << gtid_str.size() << " got " << ret);
    BOOST_REQUIRE(gtid.seqno().get() == 123456);
}

BOOST_AUTO_TEST_CASE(gtid_test_scan_from_string_uuid_too_long)
{
    std::string gtid_str("6a20d44a-6e17-11e8-b1e2-9061aec0cdadx:123456");
    wsrep::gtid gtid;
    ssize_t ret(wsrep::scan_from_c_str(
                      gtid_str.c_str(),
                      gtid_str.size(), gtid));
    BOOST_REQUIRE_MESSAGE(ret == -EINVAL,
                          "Expected " << -EINVAL << " got " << ret);
}

BOOST_AUTO_TEST_CASE(gtid_test_scan_from_string_seqno_out_of_range)
{
    std::string gtid_str("6a20d44a-6e17-11e8-b1e2-9061aec0cdad:9223372036854775808");
    wsrep::gtid gtid;
    ssize_t ret(wsrep::scan_from_c_str(
                    gtid_str.c_str(),
                    gtid_str.size(), gtid));
    BOOST_REQUIRE_MESSAGE(ret == -EINVAL,
                          "Expected " << -EINVAL << " got " << ret);

    gtid_str = "6a20d44a-6e17-11e8-b1e2-9061aec0cdad:-9223372036854775809";
    ret = wsrep::scan_from_c_str(
        gtid_str.c_str(),
        gtid_str.size(), gtid);
    BOOST_REQUIRE_MESSAGE(ret == -EINVAL,
                          "Expected " << -EINVAL << " got " << ret);
}
