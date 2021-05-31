# Install script for directory: /home/jianliang.yjl/project/server-10.4/man

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

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES
    "/home/jianliang.yjl/project/server-10.4/man/innochecksum.1"
    "/home/jianliang.yjl/project/server-10.4/man/my_print_defaults.1"
    "/home/jianliang.yjl/project/server-10.4/man/myisam_ftdump.1"
    "/home/jianliang.yjl/project/server-10.4/man/myisamchk.1"
    "/home/jianliang.yjl/project/server-10.4/man/aria_chk.1"
    "/home/jianliang.yjl/project/server-10.4/man/aria_dump_log.1"
    "/home/jianliang.yjl/project/server-10.4/man/aria_ftdump.1"
    "/home/jianliang.yjl/project/server-10.4/man/aria_pack.1"
    "/home/jianliang.yjl/project/server-10.4/man/aria_read_log.1"
    "/home/jianliang.yjl/project/server-10.4/man/myisamlog.1"
    "/home/jianliang.yjl/project/server-10.4/man/myisampack.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql.server.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql_convert_table_format.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql_fix_extensions.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql_install_db.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql_secure_installation.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql_setpermission.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql_tzinfo_to_sql.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql_upgrade.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysqld_multi.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysqld_safe.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysqldumpslow.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysqlhotcopy.1"
    "/home/jianliang.yjl/project/server-10.4/man/perror.1"
    "/home/jianliang.yjl/project/server-10.4/man/replace.1"
    "/home/jianliang.yjl/project/server-10.4/man/resolve_stack_dump.1"
    "/home/jianliang.yjl/project/server-10.4/man/resolveip.1"
    "/home/jianliang.yjl/project/server-10.4/man/mariadb-service-convert.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysqld_safe_helper.1"
    "/home/jianliang.yjl/project/server-10.4/man/wsrep_sst_common.1"
    "/home/jianliang.yjl/project/server-10.4/man/wsrep_sst_mysqldump.1"
    "/home/jianliang.yjl/project/server-10.4/man/wsrep_sst_rsync.1"
    "/home/jianliang.yjl/project/server-10.4/man/galera_recovery.1"
    "/home/jianliang.yjl/project/server-10.4/man/galera_new_cluster.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql_ldb.1"
    "/home/jianliang.yjl/project/server-10.4/man/wsrep_sst_mariabackup.1"
    "/home/jianliang.yjl/project/server-10.4/man/mbstream.1"
    "/home/jianliang.yjl/project/server-10.4/man/mariabackup.1"
    "/home/jianliang.yjl/project/server-10.4/man/wsrep_sst_rsync_wan.1"
    )
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man8" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mysqld.8")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesClient" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES
    "/home/jianliang.yjl/project/server-10.4/man/msql2mysql.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql_find_rows.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql_waitpid.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysqlaccess.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysqladmin.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysqlbinlog.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysqlcheck.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysqldump.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysqlimport.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysqlshow.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysqlslap.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql_plugin.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql_embedded.1"
    )
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesDevelopment" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mysql_config.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesTest" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES
    "/home/jianliang.yjl/project/server-10.4/man/mysql-stress-test.pl.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql-test-run.pl.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql_client_test.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysqltest.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysqltest_embedded.1"
    "/home/jianliang.yjl/project/server-10.4/man/mysql_client_test_embedded.1"
    "/home/jianliang.yjl/project/server-10.4/man/my_safe_process.1"
    )
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesClient" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesClient" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-access.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesClient" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-admin.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-backup.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesClient" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-binlog.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesClient" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-check.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesTest" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-client-test-embedded.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesTest" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-client-test.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-convert-table-format.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesClient" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-dump.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-dumpslow.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesClient" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-embedded.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesClient" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-find-rows.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-fix-extensions.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-hotcopy.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesClient" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-import.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-install-db.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-ldb.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesClient" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-plugin.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-secure-installation.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-setpermission.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesClient" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-show.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesClient" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-slap.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesTest" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-test.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesTest" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-test-embedded.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-tzinfo-to-sql.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-upgrade.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesClient" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadb-waitpid.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man8" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadbd.8")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadbd-multi.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadbd-safe.1")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "ManPagesServer" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/man/mariadbd-safe-helper.1")
endif()

