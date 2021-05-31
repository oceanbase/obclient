# Introduction

Project name: wsrep-lib - Integration library for WSREP API

The purpose of this project is to implement C++ wrapper
for wsrep API with additional convenience for transaction
processing.

This project will abstract away most of the transaction state
management required on DBMS side and will provide simple
C++ interface for key set population, data set population,
commit processing, write set applying etc.

# Build Instructions

In order to build the library, run

  cmake <cmake options> .
  make

## Build Requirements

* C++ compiler (g++ 5.4 or later recommended)
* CMake version 2.8 or later
* The following Boost libraries are required if the unit tests and
  the sample program is compiled
  * Unit Test Framework
  * Program Options
  * Filesystem
  * Thread

## CMake Options

* WSREP_LIB_WITH_UNIT_TESTS - Compile unit tests (default ON)
* WSREP_LIB_WITH_AUTO_TEST - Run unit tests automatically as a part
  of compilation (default OFF)
* WSREP_LIB_WITH_DBSIM - Compile sample program (default ON)
* WSREP_LIB_WITH_ASAN - Enable address sanitizer instrumentation (default OFF)
* WSREP_LIB_WITH_TSAN - Enable thread sanitizer instrumentation (default OFF)
* WSREP_LIB_WITH_DOCUMENTATION - Generate documentation, requires Doxygen
  (default OFF)
* WSREP_LIB_WITH_COVERAGE - Compile with coverage instrumentation (default OFF)
* WSREP_LIB_STRICT_BUILD_FLAGS - Compile with strict build flags, currently
  enables -Weffc++ (default OFF)
* WSREP_LIB_MAINTAINER_MODE - Make every compiler warning to be treated
  as error, enables -Werror compiler flag (default OFF)
