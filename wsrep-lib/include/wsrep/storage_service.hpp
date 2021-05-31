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

/** @file storage_service.hpp
 *
 * Abstract interface which defines required access to DBMS storage
 * service. The service is used for storing streaming replication
 * write set fragments into stable storage. The interface is used
 * from locally processing transaction context only. Corresponding
 * operations for high priority processing can be found from
 * wsrep::high_priority_service interface.
 */
#ifndef WSREP_STORAGE_SERVICE_HPP
#define WSREP_STORAGE_SERVICE_HPP

#include "transaction_id.hpp"
#include "id.hpp"
#include "buffer.hpp"
#include "xid.hpp"

namespace wsrep
{
    // Forward declarations
    class ws_handle;
    class ws_meta;
    class transaction;

    /**
     * Storage service abstract interface.
     */
    class storage_service
    {
    public:
        virtual ~storage_service() { }
        /**
         * Start a new transaction for storage access.
         *
         * @param[out] ws_handle Write set handle for a new transaction
         *
         * @return Zero in case of success, non-zero on error.
         */
        virtual int start_transaction(const wsrep::ws_handle&) = 0;

        virtual void adopt_transaction(const wsrep::transaction&) = 0;
        /**
         * Append fragment into stable storage.
         */
        virtual int append_fragment(const wsrep::id& server_id,
                                    wsrep::transaction_id client_id,
                                    int flags,
                                    const wsrep::const_buffer& data,
                                    const wsrep::xid& xid) = 0;

        /**
         * Update fragment meta data after certification process.
         */
        virtual int update_fragment_meta(const wsrep::ws_meta&) = 0;

        /**
         * Remove fragments from storage. The storage service must have
         * adopted a transaction prior this call.
         */
        virtual int remove_fragments() = 0;

        /**
         * Commit the transaction.
         */
        virtual int commit(const wsrep::ws_handle&, const wsrep::ws_meta&) = 0;

        /**
         * Roll back the transaction.
         */
        virtual int rollback(const wsrep::ws_handle&,
                             const wsrep::ws_meta&) = 0;


        virtual void store_globals() = 0;
        virtual void reset_globals() = 0;
    };
}

#endif // WSREP_STORAGE_SERVICE_HPP
