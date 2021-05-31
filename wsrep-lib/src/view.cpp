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
#include "wsrep/provider.hpp"

int wsrep::view::member_index(const wsrep::id& member_id) const
{
    for (std::vector<member>::const_iterator i(members_.begin());
         i != members_.end(); ++i)
    {
        if (i->id() == member_id)
        {
            return static_cast<int>(i - members_.begin());
        }
    }
    return -1;
}

bool wsrep::view::equal_membership(const wsrep::view& other) const
{
    if (members_.size() != other.members_.size())
    {
        return false;
    }
    // we can't assume members ordering
    for (std::vector<member>::const_iterator i(members_.begin());
         i != members_.end(); ++i)
    {
        if (other.member_index(i->id()) == -1)
        {
            return false;
        }
    }
    return true;
}

void wsrep::view::print(std::ostream& os) const
{
    os << "  id: " << state_id() << "\n"
       << "  status: " << to_c_string(status()) << "\n"
       << "  protocol_version: " << protocol_version() << "\n"
       << "  capabilities: " << provider::capability::str(capabilities())<<"\n"
       << "  final: " << (final() ? "yes" : "no") << "\n"
       << "  own_index: " << own_index() << "\n"
       << "  members(" << members().size() << "):\n";

    for (std::vector<wsrep::view::member>::const_iterator i(members().begin());
         i != members().end(); ++i)
    {
        os << "\t" << (i - members().begin()) /* ordinal index */
           << ": " << i->id()
           << ", " << i->name() << "\n";
    }
}
