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

#ifndef WSREP_TRANSACTION_ID_HPP
#define WSREP_TRANSACTION_ID_HPP

#include <iostream>
#include <limits>

namespace wsrep
{
    class transaction_id
    {
    public:
        typedef unsigned long long type;


        transaction_id()
            : id_(std::numeric_limits<type>::max())
        { }

        template <typename I>
        explicit transaction_id(I id)
            : id_(static_cast<type>(id))
        { }
        type get() const { return id_; }
        static transaction_id undefined() { return transaction_id(-1); }
        bool is_undefined() const { return (id_ == type(-1)); }
        bool operator<(const transaction_id& other) const
        {
            return (id_ < other.id_);
        }
        bool operator==(const transaction_id& other) const
        { return (id_ == other.id_); }
        bool operator!=(const transaction_id& other) const
        { return (id_ != other.id_); }
    private:
        type id_;
    };

    static inline std::ostream& operator<<(std::ostream& os, transaction_id id)
    {
        return (os << id.get());
    }
}

#endif // WSREP_TRANSACTION_ID_HPP
