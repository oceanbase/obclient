#
# Copyright (C) 2018 Codership Oy <info@codership.com>
#

add_executable(dbsim
  db_client.cpp
  db_client_service.cpp
  db_high_priority_service.cpp
  db_params.cpp
  db_server.cpp
  db_server_service.cpp
  db_server_state.cpp
  db_simulator.cpp
  db_storage_engine.cpp
  db_threads.cpp
  dbsim.cpp
)

target_link_libraries(dbsim wsrep-lib ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY})
set_property(TARGET dbsim PROPERTY CXX_STANDARD 14)
