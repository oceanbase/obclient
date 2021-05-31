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

#include "db_server.hpp"
#include "db_server_service.hpp"
#include "db_high_priority_service.hpp"
#include "db_client.hpp"
#include "db_simulator.hpp"

#include "wsrep/logger.hpp"

db::server::server(simulator& simulator,
                   const std::string& name,
                   const std::string& address)
    : simulator_(simulator)
    , storage_engine_(simulator_.params())
    , mutex_()
    , cond_()
    , server_service_(*this)
    , server_state_(server_service_,
                    name, address, "dbsim_" + name + "_data")
    , last_client_id_(0)
    , last_transaction_id_(0)
    , appliers_()
    , clients_()
    , client_threads_()
{ }

void db::server::applier_thread()
{
    wsrep::client_id client_id(last_client_id_.fetch_add(1) + 1);
    db::client applier(*this, client_id,
                       wsrep::client_state::m_high_priority,
                       simulator_.params());
    wsrep::client_state* cc(static_cast<wsrep::client_state*>(
                                &applier.client_state()));
    db::high_priority_service hps(*this, applier);
    cc->open(cc->id());
    cc->before_command();
    enum wsrep::provider::status ret(
        server_state_.provider().run_applier(&hps));
    wsrep::log_info() << "Applier thread exited with error code " << ret;
    cc->after_command_before_result();
    cc->after_command_after_result();
    cc->close();
    cc->cleanup();
}

void db::server::start_applier()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    appliers_.push_back(boost::thread(&server::applier_thread, this));
}

void db::server::stop_applier()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    appliers_.front().join();
    appliers_.erase(appliers_.begin());
}


void db::server::start_clients()
{
    size_t n_clients(simulator_.params().n_clients);
    for (size_t i(0); i < n_clients; ++i)
    {
        start_client(i + 1);
    }
}

void db::server::stop_clients()
{
    for (auto& i : client_threads_)
    {
        i.join();
    }
    for (const auto& i : clients_)
    {
        const struct db::client::stats& stats(i->stats());
        simulator_.stats_.commits += stats.commits;
        simulator_.stats_.rollbacks  += stats.rollbacks;
        simulator_.stats_.replays += stats.replays;
    }
}

void db::server::client_thread(const std::shared_ptr<db::client>& client)
{
    client->start();
}

void db::server::start_client(size_t id)
{
    auto client(std::make_shared<db::client>(
                    *this, wsrep::client_id(id),
                    wsrep::client_state::m_local,
                    simulator_.params()));
    clients_.push_back(client);
    client_threads_.push_back(
        boost::thread(&db::server::client_thread, this, client));
}

void db::server::donate_sst(const std::string& req,
                            const wsrep::gtid& gtid,
                            bool bypass)
{
    simulator_.sst(*this, req, gtid, bypass);
}


wsrep::high_priority_service* db::server::streaming_applier_service()
{
    throw wsrep::not_implemented_error();
}

