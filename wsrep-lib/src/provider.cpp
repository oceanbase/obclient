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

#include "wsrep/provider.hpp"
#include "wsrep/logger.hpp"

#include "wsrep_provider_v26.hpp"

#include <dlfcn.h>
#include <memory>

wsrep::provider* wsrep::provider::make_provider(
    wsrep::server_state& server_state,
    const std::string& provider_spec,
    const std::string& provider_options,
    const wsrep::provider::services& services)
{
    try
    {
        return new wsrep::wsrep_provider_v26(
            server_state, provider_options, provider_spec, services);
    }
    catch (const wsrep::runtime_error& e)
    {
        wsrep::log_error() << "Failed to create a new provider '"
                           << provider_spec << "'"
                           << " with options '" << provider_options
                           << "': " << e.what();
    }
    catch (...)
    {
        wsrep::log_error() << "Caught unknown exception when trying to "
                           << "create a new provider '"
                           << provider_spec << "'"
                           << " with options '" << provider_options;
    }
    return 0;
}

std::string
wsrep::provider::to_string(enum wsrep::provider::status const val)
{
    switch(val)
    {
    case success:
        return "Success";
    case error_warning:
        return "Warning";
    case error_transaction_missing:
        return "Transaction not registered with provider";
    case error_certification_failed:
        return "Certification failed";
    case error_bf_abort:
        return "Transaction was BF aborted";
    case error_size_exceeded:
        return "Transaction size exceeded";
    case error_connection_failed:
        return "Not connected to Primary Component";
    case error_provider_failed:
        return "Provider in bad state, needs to be reinitialized.";
    case error_fatal:
        return "Fatal error, must abort.";
    case error_not_implemented:
        return "Function not implemented";
    case error_not_allowed:
        return "Operation not allowed";
    case error_unknown:
        return "Unknown error";
    }

    assert(0);

    std::ostringstream os;
    os << "Invalid error code: " << val;
    return os.str();
}

std::string wsrep::provider::capability::str(int caps)
{
    std::ostringstream os;

#define WSREP_PRINT_CAPABILITY(cap_value, cap_string) \
    if (caps & cap_value) {                           \
        os << cap_string ", ";                        \
        caps &= ~cap_value;                           \
    }

    WSREP_PRINT_CAPABILITY(multi_master,         "MULTI-MASTER");
    WSREP_PRINT_CAPABILITY(certification,        "CERTIFICATION");
    WSREP_PRINT_CAPABILITY(parallel_applying,    "PARALLEL_APPLYING");
    WSREP_PRINT_CAPABILITY(transaction_replay,   "REPLAY");
    WSREP_PRINT_CAPABILITY(isolation,            "ISOLATION");
    WSREP_PRINT_CAPABILITY(pause,                "PAUSE");
    WSREP_PRINT_CAPABILITY(causal_reads,         "CAUSAL_READ");
    WSREP_PRINT_CAPABILITY(causal_transaction,   "CAUSAL_TRX");
    WSREP_PRINT_CAPABILITY(incremental_writeset, "INCREMENTAL_WS");
    WSREP_PRINT_CAPABILITY(session_locks,        "SESSION_LOCK");
    WSREP_PRINT_CAPABILITY(distributed_locks,    "DISTRIBUTED_LOCK");
    WSREP_PRINT_CAPABILITY(consistency_check,    "CONSISTENCY_CHECK");
    WSREP_PRINT_CAPABILITY(unordered,            "UNORDERED");
    WSREP_PRINT_CAPABILITY(annotation,           "ANNOTATION");
    WSREP_PRINT_CAPABILITY(preordered,           "PREORDERED");
    WSREP_PRINT_CAPABILITY(streaming,            "STREAMING");
    WSREP_PRINT_CAPABILITY(snapshot,             "READ_VIEW");
    WSREP_PRINT_CAPABILITY(nbo,                  "NBO");

#undef WSREP_PRINT_CAPABILITY

    if (caps)
    {
        assert(caps == 0); // to catch missed capabilities
        os << "UNKNOWN(" << caps << ")  ";
    }

    std::string ret(os.str());
    if (ret.size() > 2) ret.erase(ret.size() - 2);
    return ret;
}

std::string wsrep::flags_to_string(int flags)
{
    std::ostringstream oss;
    if (flags & provider::flag::start_transaction)
        oss << "start_transaction | ";
    if (flags & provider::flag::commit)
        oss << "commit | ";
    if (flags & provider::flag::rollback)
        oss << "rollback | ";
    if (flags & provider::flag::isolation)
        oss << "isolation | ";
    if (flags & provider::flag::pa_unsafe)
        oss << "pa_unsafe | ";
    if (flags & provider::flag::prepare)
        oss << "prepare | ";
    if (flags & provider::flag::snapshot)
        oss << "read_view | ";
    if (flags & provider::flag::implicit_deps)
        oss << "implicit_deps | ";

    std::string ret(oss.str());
    if (ret.size() > 3) ret.erase(ret.size() - 3);
    return ret;
}

std::ostream& wsrep::operator<<(std::ostream& os, const wsrep::ws_meta& ws_meta)
{
    os <<  "gtid: "      << ws_meta.gtid()
       << " server_id: " << ws_meta.server_id()
       << " client_id: " << ws_meta.client_id()
       << " trx_id: "    << ws_meta.transaction_id()
       << " flags: "     << ws_meta.flags()
       << " (" << wsrep::flags_to_string(ws_meta.flags()) << ")";
    return os;
}
