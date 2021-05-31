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

#include "wsrep/key.hpp"
#include <ostream>
#include <iomanip>

namespace
{
    void print_key_part(std::ostream& os, const void* ptr, size_t len)
    {
        std::ios::fmtflags flags_save(os.flags());
        os << len << ": ";
        for (size_t i(0); i < len; ++i)
        {
            os << std::hex
               << std::setfill('0')
               << std::setw(2)
               << static_cast<int>(
                   *(reinterpret_cast<const unsigned char*>(ptr) + i)) << " ";
        }
        os.flags(flags_save);
    }
}

std::ostream& wsrep::operator<<(std::ostream& os,
                                enum wsrep::key::type key_type)
{
    switch (key_type)
    {
    case wsrep::key::shared: os << "shared"; break;
    case wsrep::key::reference: os << "reference"; break;
    case wsrep::key::update: os << "update"; break;
    case wsrep::key::exclusive: os << "exclusive"; break;
    default: os << "unknown"; break;
    }
    return os;
}

std::ostream& wsrep::operator<<(std::ostream& os, const wsrep::key& key)
{
    os << "type: " << key.type();
    for (size_t i(0); i < key.size(); ++i)
    {
        os << "\n    ";
        print_key_part(os, key.key_parts()[i].data(), key.key_parts()[i].size());
    }
    return os;
}
