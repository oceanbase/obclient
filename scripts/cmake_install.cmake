# Install script for directory: /home/jianliang.yjl/project/server-10.4/scripts

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/app/mariadb")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "RelWithDebInfo")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE FILE FILES
    "/home/jianliang.yjl/project/server-10.4/scripts/mysql_system_tables.sql"
    "/home/jianliang.yjl/project/server-10.4/scripts/mysql_system_tables_data.sql"
    "/home/jianliang.yjl/project/server-10.4/scripts/mysql_performance_tables.sql"
    "/home/jianliang.yjl/project/server-10.4/scripts/mysql_test_db.sql"
    "/home/jianliang.yjl/project/server-10.4/scripts/fill_help_tables.sql"
    "/home/jianliang.yjl/project/server-10.4/scripts/mysql_test_data_timezone.sql"
    "/home/jianliang.yjl/project/server-10.4/scripts/mysql_to_mariadb.sql"
    "/home/jianliang.yjl/project/server-10.4/scripts/maria_add_gis_sp.sql"
    "/home/jianliang.yjl/project/server-10.4/scripts/maria_add_gis_sp_bootstrap.sql"
    )
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/scripts" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/mysql_install_db")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/scripts" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/scripts/mariadb-install-db")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/scripts/wsrep_sst_rsync_wan")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/scripts/wsrep_sst_common")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Client" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/msql2mysql")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/mysql_config")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/mysql_setpermission")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/scripts/mariadb-setpermission")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/mysql_secure_installation")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/scripts/mariadb-secure-installation")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Client" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/mysqlaccess")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Client" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/scripts/mariadb-access")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/mysql_convert_table_format")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/scripts/mariadb-convert-table-format")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Client" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/mysql_find_rows")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Client" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/scripts/mariadb-find-rows")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Mytop" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/mytop")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/mysqlhotcopy")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/scripts/mariadb-hotcopy")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/mysql_fix_extensions")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/scripts/mariadb-fix-extensions")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/mysqld_multi")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/scripts/mariadbd-multi")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/mysqld_safe")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/scripts/mariadbd-safe")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/mysqldumpslow")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/scripts/mariadb-dumpslow")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/wsrep_sst_mysqldump")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/wsrep_sst_rsync")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/scripts/wsrep_sst_mariabackup")
endif()

