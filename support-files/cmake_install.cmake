# Install script for directory: /home/jianliang.yjl/project/server-10.4/support-files

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

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "IniFiles" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/support-files" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/support-files/wsrep.cnf")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server_Scripts" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/support-files" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/support-files/mysqld_multi.server")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server_Scripts" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/support-files" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/support-files/mysql-log-rotate")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server_Scripts" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/support-files" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/support-files/binary-configure")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server_Scripts" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/support-files" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/support-files/wsrep_notify")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "SupportFiles" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/support-files" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/support-files/magic")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "SupportFiles" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/support-files" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/support-files/policy")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/support-files/mariadb.pc")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/aclocal" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/support-files/mysql.m4")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "SupportFiles" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/support-files" TYPE PROGRAM FILES "/home/jianliang.yjl/project/server-10.4/support-files/mysql.server")
endif()

