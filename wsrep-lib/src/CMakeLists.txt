#
# Copyright (C) 2018 Codership Oy <info@codership.com>
#

add_library(wsrep-lib
  client_state.cpp
  exception.cpp
  gtid.cpp
  id.cpp
  xid.cpp
  key.cpp
  logger.cpp
  provider.cpp
  seqno.cpp
  view.cpp
  server_state.cpp
  thread.cpp
  thread_service_v1.cpp
  transaction.cpp
  uuid.cpp
  wsrep_provider_v26.cpp)
target_link_libraries(wsrep-lib wsrep_api_v26 pthread ${WSREP_LIB_LIBDL})
