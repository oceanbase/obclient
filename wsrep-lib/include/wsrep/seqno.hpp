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

#ifndef WSREP_SEQNO_HPP
#define WSREP_SEQNO_HPP

#include <iosfwd>

namespace wsrep
{
    /** @class seqno
     *
     * Sequence number type.
     */
    class seqno
    {
    public:
        typedef long long native_type;

        seqno()
            : seqno_(-1)
        { }

        explicit seqno(long long seqno)
            : seqno_(seqno)
        { }

        long long get() const
        {
            return seqno_;
        }

        bool is_undefined() const
        {
            return (seqno_ == -1);
        }

        bool operator<(seqno other) const
        {
            return (seqno_ < other.seqno_);
        }

        bool operator>(seqno other) const
        {
            return (seqno_ > other.seqno_);
        }
        bool operator==(seqno other) const
        {
            return (seqno_ == other.seqno_);
        }
        bool operator!=(seqno other) const
        {
            return !(seqno_ == other.seqno_);
        }
        seqno operator+(seqno other) const
        {
            return (seqno(seqno_ + other.seqno_));
        }
        seqno operator+(long long other) const
        {
            return (*this + seqno(other));
        }
        static seqno undefined() { return seqno(-1); }

    private:
        native_type seqno_;
    };

    std::ostream& operator<<(std::ostream& os, wsrep::seqno seqno);
}

#endif // WSREP_SEQNO_HPP
