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

#include <cerrno>
#include <iostream>
#include <sstream>

const wsrep::gtid wsrep::gtid::undefined_ = wsrep::gtid();

std::ostream& wsrep::operator<<(std::ostream& os, const wsrep::gtid& gtid)
{
    return (os << gtid.id() << ":" << gtid.seqno());
}

std::istream& wsrep::operator>>(std::istream& is, wsrep::gtid& gtid)
{
    std::string id_str;
    std::getline(is, id_str, ':');
    long long seq;
    is >> seq;
    if (!is)
    {
        is.clear(std::ios_base::failbit);
        return is;
    }
    try
    {
        // wsrep::id constructor will throw if it cannot parse the
        // id_str.
        gtid = wsrep::gtid(wsrep::id(id_str), wsrep::seqno(seq));
    }
    catch (const wsrep::runtime_error& e)
    {
        // Formatting or extraction error. Clear the istream state and
        // set failibit.
        is.clear(std::ios_base::failbit);
    }
    return is;
}

ssize_t wsrep::scan_from_c_str(
    const char* buf, size_t buf_len, wsrep::gtid& gtid)
{
    std::istringstream is(std::string(buf, buf_len));
    is >> gtid;
    // Some failure occurred
    if (!is)
    {
        return -EINVAL;
    }
    // Whole string was consumed without failures
    if (is.eof())
    {
        return static_cast<ssize_t>(buf_len);
    }
    // The string was not consumed completely, return current position
    // of the istream.
    return static_cast<ssize_t>(is.tellg());
}

ssize_t wsrep::print_to_c_str(
    const wsrep::gtid& gtid, char* buf, size_t buf_len)
{
    std::ostringstream os;
    os << gtid;
    if (os.str().size() > buf_len)
    {
        return -ENOBUFS;
    }
    std::strncpy(buf, os.str().c_str(), os.str().size());
    return static_cast<ssize_t>(os.str().size());
}
