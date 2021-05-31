# Install script for directory: /home/jianliang.yjl/project/server-10.4/sql/share

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

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Common" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/charsets" REGEX "/languages\\.html$" EXCLUDE)
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/danish")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/german")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/slovak")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/dutch")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/greek")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/norwegian")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/spanish")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/english")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/hungarian")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/norwegian-ny")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/swedish")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/italian")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/polish")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/ukrainian")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/japanese")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/portuguese")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/romanian")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/estonian")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/korean")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/russian")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/czech")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/french")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/serbian")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jianliang.yjl/project/server-10.4/sql/share/hindi")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Server" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE FILE FILES "/home/jianliang.yjl/project/server-10.4/sql/share/errmsg-utf8.txt")
endif()

