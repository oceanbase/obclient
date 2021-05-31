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

// Forward declarations
namespace wsrep
{
    class client_state;
    class mock_server_state;
}

#include "wsrep/transaction.hpp"
#include "wsrep/provider.hpp"

//
// Utility functions
//
namespace wsrep_test
{

    // Simple BF abort method to BF abort unordered transasctions
    void bf_abort_unordered(wsrep::client_state& cc);

    // Simple BF abort method to BF abort unordered transasctions
    void bf_abort_ordered(wsrep::client_state& cc);

    // BF abort method to abort transactions via provider
    void bf_abort_provider(wsrep::mock_server_state& sc,
                           const wsrep::transaction& tc,
                           wsrep::seqno bf_seqno);

    // Terminate streaming applier by applying rollback fragment.
    void terminate_streaming_applier(
        wsrep::mock_server_state& sc,
        const wsrep::id& server_id,
        wsrep::transaction_id transaction_id);
}
