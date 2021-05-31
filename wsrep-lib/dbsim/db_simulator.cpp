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

#include "db_simulator.hpp"
#include "db_client.hpp"
#include "db_threads.hpp"

#include "wsrep/logger.hpp"

#include <boost/filesystem.hpp>
#include <sstream>

static db::ti thread_instrumentation;

void db::simulator::run()
{
    start();
    stop();
    std::flush(std::cerr);
    std::cout << "Results:\n";
    std::cout << stats() << std::endl;
    std::cout << db::ti::stats() << std::endl;
}

void db::simulator::sst(db::server& server,
                        const std::string& request,
                        const wsrep::gtid& gtid,
                        bool bypass)
{
    // The request may contain extra trailing '\0' after it goes
    // through the provider, strip it first.
    std::string name(request);
    name.erase(std::find(name.begin(), name.end(), '\0'), name.end());

    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    auto i(servers_.find(name));
    wsrep::log_info() << "SST request '" << name << "'";
    if (i == servers_.end())
    {
        wsrep::log_error() << "Server " << request << " not found";
        wsrep::log_info() << "servers:";
        for (const auto& s : servers_)
        {
            wsrep::log_info() << "server: " << s.first;
        }
        throw wsrep::runtime_error("Server " + request + " not found");
    }
    if (bypass == false)
    {
        wsrep::log_info() << "SST "
                          << server.server_state().name()
                          << " -> " << request;
        i->second->storage_engine().store_position(gtid);
        i->second->storage_engine().store_view(
            server.storage_engine().get_view());
    }

    db::client dummy(*(i->second), wsrep::client_id(-1),
                     wsrep::client_state::m_local, params());

    i->second->server_state().sst_received(dummy.client_service(), 0);
    server.server_state().sst_sent(gtid, 0);
}

std::string db::simulator::stats() const
{
    auto duration(std::chrono::duration<double>(
                      clients_stop_ - clients_start_).count());
    long long transactions(stats_.commits + stats_.rollbacks);
    long long bf_aborts(0);
    for (const auto& s : servers_)
    {
        bf_aborts += s.second->storage_engine().bf_aborts();
    }
    std::ostringstream os;
    os << "Number of transactions: " << transactions
       << "\n"
       << "Seconds: " << duration
       << " \n"
       << "Transactions per second: " << double(transactions)/double(duration)
       << "\n"
       << "BF aborts: "
       << bf_aborts
       << "\n"
       << "Client commits: " << stats_.commits
       << "\n"
       << "Client rollbacks: " << stats_.rollbacks
       << "\n"
       << "Client replays: " << stats_.replays;
    return os.str();
}

////////////////////////////////////////////////////////////////////////////////
//                              Private                                       //
////////////////////////////////////////////////////////////////////////////////

void db::simulator::start()
{
    thread_instrumentation.level(params_.thread_instrumentation);
    thread_instrumentation.cond_checks(params_.cond_checks);
    wsrep::log_info() << "Provider: " << params_.wsrep_provider;

    std::string cluster_address(build_cluster_address());
    wsrep::log_info() << "Cluster address: " << cluster_address;
    for (size_t i(0); i < params_.n_servers; ++i)
    {
        std::ostringstream name_os;
        name_os << (i + 1);
        std::ostringstream id_os;
        id_os << (i + 1);
        std::ostringstream address_os;
        address_os << "127.0.0.1:" << server_port(i);
        wsrep::id server_id(id_os.str());
        auto it(servers_.insert(
                    std::make_pair(
                        name_os.str(),
                        std::make_unique<db::server>(
                            *this,
                            name_os.str(),
                            address_os.str()))));
        if (it.second == false)
        {
            throw wsrep::runtime_error("Failed to add server");
        }
        boost::filesystem::path dir("dbsim_" + id_os.str() + "_data");
        boost::filesystem::create_directory(dir);

        db::server& server(*it.first->second);
        server.server_state().debug_log_level(params_.debug_log_level);
        std::string server_options(params_.wsrep_provider_options);

        wsrep::provider::services services;
        services.thread_service = params_.thread_instrumentation
                                      ? &thread_instrumentation
                                      : nullptr;
        if (server.server_state().load_provider(params_.wsrep_provider,
                                                server_options, services))
        {
            throw wsrep::runtime_error("Failed to load provider");
        }
        if (server.server_state().connect("sim_cluster", cluster_address, "",
                                          i == 0))
        {
            throw wsrep::runtime_error("Failed to connect");
        }
        wsrep::log_debug() << "main: Starting applier";
        server.start_applier();
        wsrep::log_debug() << "main: Waiting initializing state";
        server.server_state().wait_until_state(wsrep::server_state::s_initializing);
        wsrep::log_debug() << "main: Calling initialized";
        server.server_state().initialized();
        wsrep::log_debug() << "main: Waiting for synced state";
        server.server_state().wait_until_state(
            wsrep::server_state::s_synced);
        wsrep::log_debug() << "main: Server synced";
    }

    // Start client threads
    wsrep::log_info() << "####################### Starting client load";
    clients_start_ = std::chrono::steady_clock::now();
    size_t index(0);
    for (auto& i : servers_)
    {
        if (params_.topology.size() == 0 || params_.topology[index]  == 'm')
        {
            i.second->start_clients();
        }
        ++index;
    }
}

void db::simulator::stop()
{
    for (auto& i : servers_)
    {
        db::server& server(*i.second);
        server.stop_clients();
    }
    clients_stop_ = std::chrono::steady_clock::now();
    wsrep::log_info() << "######## Stats ############";
    wsrep::log_info()  << stats();
    std::cout << db::ti::stats() << std::endl;
    wsrep::log_info() << "######## Stats ############";
    if (params_.fast_exit)
    {
        exit(0);
    }
    for (auto& i : servers_)
    {
        db::server& server(*i.second);
        wsrep::log_info() << "Status for server: "
                          << server.server_state().id();
        auto status(server.server_state().provider().status());
        for_each(status.begin(), status.end(),
                 [](const wsrep::provider::status_variable& sv)
                 {
                     wsrep::log_info() << sv.name() << " = " << sv.value();
                 });
        server.server_state().disconnect();
        server.server_state().wait_until_state(
            wsrep::server_state::s_disconnected);
        server.stop_applier();
        server.server_state().unload_provider();
    }
}

std::string db::simulator::server_port(size_t i) const
{
    std::ostringstream os;
    os << (10000 + (i + 1)*10);
    return os.str();
}

std::string db::simulator::build_cluster_address() const
{
    std::string ret;
    if (params_.wsrep_provider.find("galera_smm") != std::string::npos)
    {
        ret += "gcomm://";
    }

    for (size_t i(0); i < params_.n_servers; ++i)
    {
        std::ostringstream sa_os;
        sa_os << "127.0.0.1:";
        sa_os << server_port(i);
        ret += sa_os.str();
        if (i < params_.n_servers - 1) ret += ",";
    }
    return ret;
}
