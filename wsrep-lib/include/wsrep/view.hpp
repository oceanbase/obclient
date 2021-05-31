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

/** @file view.hpp
 *
 *
 */


#ifndef WSREP_VIEW_HPP
#define WSREP_VIEW_HPP

#include "id.hpp"
#include "seqno.hpp"
#include "gtid.hpp"
#include <vector>
#include <iostream>

namespace wsrep
{
    class view
    {
    public:
        enum status
        {
            primary,
            non_primary,
            disconnected
        };
        class member
        {
        public:
            member(const wsrep::id& id,
                   const std::string& name,
                   const std::string& incoming)
                : id_(id)
                , name_(name)
                , incoming_(incoming)
            {
            }
            const wsrep::id& id() const { return id_; }
            const std::string& name() const { return name_; }
            const std::string& incoming() const { return incoming_; }
        private:
            wsrep::id id_;
            std::string name_;
            std::string incoming_;
        };

        view()
            : state_id_()
            , view_seqno_()
            , status_(disconnected)
            , capabilities_()
            , own_index_(-1)
            , protocol_version_(0)
            , members_()
        { }
        view(const wsrep::gtid& state_id,
             wsrep::seqno view_seqno,
             enum wsrep::view::status status,
             int capabilities,
             ssize_t own_index,
             int protocol_version,
             const std::vector<wsrep::view::member>& members)
            : state_id_(state_id)
            , view_seqno_(view_seqno)
            , status_(status)
            , capabilities_(capabilities)
            , own_index_(own_index)
            , protocol_version_(protocol_version)
            , members_(members)
        { }

        wsrep::gtid state_id() const
        { return state_id_; }

        wsrep::seqno view_seqno() const
        { return view_seqno_; }

        wsrep::view::status status() const
        { return status_; }

        int capabilities() const
        { return capabilities_; }

        ssize_t own_index() const
        { return own_index_; }

        /**
         * Return true if the two views have the same membership
         */
        bool equal_membership(const wsrep::view& other) const;

        int protocol_version() const
        { return protocol_version_; }
        const std::vector<member>& members() const { return members_; }

        /**
         * Return true if the view is final
         */
        bool final() const
        {
            return (members_.empty() && own_index_ == -1);
        }

        /**
         * Return member index in the view.
         *
         * @return Member index if found, -1 if member is not present
         *         in the view.
         */
        int member_index(const wsrep::id& member_id) const;

        void print(std::ostream& os) const;

    private:
        wsrep::gtid state_id_;
        wsrep::seqno view_seqno_;
        enum wsrep::view::status status_;
        int capabilities_;
        ssize_t own_index_;
        int protocol_version_;
        std::vector<wsrep::view::member> members_;
    };

    static inline
    std::ostream& operator<<(std::ostream& os, const wsrep::view& v)
    {
        v.print(os); return os;
    }

    static inline const char* to_c_string(enum wsrep::view::status status)
    {
        switch(status)
        {
        case wsrep::view::primary:      return "primary";
        case wsrep::view::non_primary:  return "non-primary";
        case wsrep::view::disconnected: return "disconnected";
        }
        return "invalid status";
    }
}

#endif // WSREP_VIEW
