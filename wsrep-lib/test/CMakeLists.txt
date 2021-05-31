#
# Copyright (C) 2018 Codership Oy <info@codership.com>
#

add_executable(wsrep-lib_test
  mock_client_state.cpp
  mock_high_priority_service.cpp
  mock_storage_service.cpp
  test_utils.cpp
  buffer_test.cpp
  gtid_test.cpp
  id_test.cpp
  nbo_test.cpp
  server_context_test.cpp
  toi_test.cpp
  transaction_test.cpp
  transaction_test_2pc.cpp
  transaction_test_xa.cpp
  view_test.cpp
  xid_test.cpp
  wsrep-lib_test.cpp
  )

target_link_libraries(wsrep-lib_test wsrep-lib)

add_test(NAME    wsrep-lib_test
         COMMAND wsrep-lib_test)

if (WSREP_LIB_WITH_AUTO_TEST)
  set(UNIT_TEST wsrep-lib_test)
  add_custom_command(
    TARGET ${UNIT_TEST}
    COMMENT "Run tests"
    POST_BUILD
    COMMAND ${UNIT_TEST}
    )
endif()
