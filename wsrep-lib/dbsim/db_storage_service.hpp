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

#ifndef WSREP_DB_STORAGE_SERVICE_HPP
#define WSREP_DB_STORAGE_SERVICE_HPP

#include "wsrep/storage_service.hpp"
#include "wsrep/exception.hpp"

namespace db
{
    class storage_service : public wsrep::storage_service
    {
        int start_transaction(const wsrep::ws_handle&) override
        { throw wsrep::not_implemented_error(); }
        void adopt_transaction(const wsrep::transaction&) override
        { throw wsrep::not_implemented_error(); }
        int append_fragment(const wsrep::id&,
                            wsrep::transaction_id,
                            int,
                            const wsrep::const_buffer&,
                            const wsrep::xid&) override
        { throw wsrep::not_implemented_error(); }
        int update_fragment_meta(const wsrep::ws_meta&) override
        { throw wsrep::not_implemented_error(); }
        int remove_fragments() override
        { throw wsrep::not_implemented_error(); }
        int commit(const wsrep::ws_handle&, const wsrep::ws_meta&) override
        { throw wsrep::not_implemented_error(); }
        int rollback(const wsrep::ws_handle&, const wsrep::ws_meta&)
            override
        { throw wsrep::not_implemented_error(); }
        void store_globals() override { }
        void reset_globals() override { }
    };
}

#endif // WSREP_DB_STORAGE_SERVICE_HPP
