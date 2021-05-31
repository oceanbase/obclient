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

#ifndef WSREP_CLIENT_ID_HPP
#define WSREP_CLIENT_ID_HPP

#include <ostream>
#include <limits>

namespace wsrep
{
    class client_id
    {
    public:
        typedef unsigned long long type;
        client_id()
            : id_(std::numeric_limits<type>::max())
        { }
        template <typename I>
        explicit client_id(I id)
            : id_(static_cast<type>(id))
        { }
        type get() const { return id_; }
        static type undefined() { return std::numeric_limits<type>::max(); }
        bool operator<(const client_id& other) const
        {
            return (id_ < other.id_);
        }
        bool operator==(const client_id& other) const
        {
            return (id_ == other.id_);
        }
    private:
        type id_;
    };
    static inline std::ostream& operator<<(
        std::ostream& os, const wsrep::client_id& client_id)
    {
        return (os << client_id.get());
    }
}


#endif // WSREP_CLIENT_ID_HPP
