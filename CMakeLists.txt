# This file is just an orchestration
cmake_minimum_required(VERSION 3.5.2)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Apple: Don't modify install_name when touching RPATH.
if(POLICY CMP0068)
  cmake_policy(SET CMP0068 NEW)
endif()

if(POLICY CMP0078)
  cmake_policy(SET CMP0078 OLD)
endif()

if(POLICY CMP0086)
  cmake_policy(SET CMP0086 NEW)
endif()

include(utils)
set_version(VERSION)

project(ortools VERSION ${VERSION} LANGUAGES CXX)
message(STATUS "version: ${PROJECT_VERSION}")
message(STATUS "major: ${PROJECT_VERSION_MAJOR}")
message(STATUS "minor: ${PROJECT_VERSION_MINOR}")
message(STATUS "patch: ${PROJECT_VERSION_PATCH}")

# Default Build Type to be Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel. (default: Release)"
    FORCE)
endif()

if(UNIX)
  option(BUILD_SHARED_LIBS "Build shared libraries(.so)." ON)
elseif(MSVC)
  # Windows only support static build.
  set(BUILD_SHARED_LIBS OFF)
endif()

# By default only build the C++ library.
option(BUILD_CXX "Build C++ library" ON)
message(STATUS "Build C++ library: ${BUILD_CXX}")

option(BUILD_PYTHON "Build Python Library" OFF)
message(STATUS "Build Python: ${BUILD_PYTHON}")
option(BUILD_JAVA "Build Java Library" OFF)
message(STATUS "Build Java: ${BUILD_JAVA}")
option(BUILD_DOTNET "Build .NET Library" OFF)
message(STATUS "Build .Net: ${BUILD_DOTNET}")

#option(BUILD_DOC "Build doxygen" OFF)
#message(STATUS "Build doxygen: ${BUILD_DOC}")

# By default all dependencies are NOT built (i.e. BUILD_DEPS=OFF),
# BUT if building any wrappers (Python, Java or .Net) then BUILD_DEPS=ON.
if(BUILD_PYTHON OR BUILD_JAVA OR BUILD_DOTNET)
  option(BUILD_DEPS "Build all dependencies" ON)
else()
  option(BUILD_DEPS "Build all dependencies" OFF)
endif()
message(STATUS "Build all dependencies: ${BUILD_DEPS}")
# Install built dependencies if any,
option(INSTALL_BUILD_DEPS "Install build all dependencies" ON)

# IF BUILD_DEPS=ON THEN Force all BUILD_*=ON
include(CMakeDependentOption)
CMAKE_DEPENDENT_OPTION(BUILD_ZLIB "Build the ZLIB dependency Library" OFF
  "NOT BUILD_DEPS" ON)
message(STATUS "Build ZLIB: ${BUILD_ZLIB}")

CMAKE_DEPENDENT_OPTION(BUILD_absl "Build the abseil-cpp dependency Library" OFF
  "NOT BUILD_DEPS" ON)
message(STATUS "Build abseil-cpp: ${BUILD_absl}")

CMAKE_DEPENDENT_OPTION(BUILD_gflags "Build the gflags dependency Library" OFF
  "NOT BUILD_DEPS" ON)
message(STATUS "Build gflags: ${BUILD_gflags}")

CMAKE_DEPENDENT_OPTION(BUILD_glog "Build the glog dependency Library" OFF
  "NOT BUILD_DEPS" ON)
message(STATUS "Build glog: ${BUILD_glog}")

CMAKE_DEPENDENT_OPTION(BUILD_Protobuf "Build the Protobuf dependency Library" OFF
  "NOT BUILD_DEPS" ON)
message(STATUS "Build protobuf: ${BUILD_Protobuf}")

# Optional third party solvers (enabled by default)
option(USE_COINOR "Use the COIN-OR solver" ON)
message(STATUS "COIN-OR support: ${USE_COINOR}")

if(USE_COINOR)
  CMAKE_DEPENDENT_OPTION(BUILD_CoinUtils "Build the CoinUtils dependency Library" OFF
    "NOT BUILD_DEPS" ON)
  message(STATUS "Build CoinUtils: ${BUILD_CoinUtils}")

  CMAKE_DEPENDENT_OPTION(BUILD_Osi "Build the Osi dependency Library" OFF
    "NOT BUILD_DEPS" ON)
  message(STATUS "Build Osi: ${BUILD_Osi}")

  CMAKE_DEPENDENT_OPTION(BUILD_Clp "Build the Clp dependency Library" OFF
    "NOT BUILD_DEPS" ON)
  message(STATUS "Build Clp: ${BUILD_Clp}")

  CMAKE_DEPENDENT_OPTION(BUILD_Cgl "Build the Cgl dependency Library" OFF
    "NOT BUILD_DEPS" ON)
  message(STATUS "Build Cgl: ${BUILD_Cgl}")

  CMAKE_DEPENDENT_OPTION(BUILD_Cbc "Build the Cbc dependency Library" OFF
    "NOT BUILD_DEPS" ON)
  message(STATUS "Build Cbc: ${BUILD_Cbc}")
endif()

# Optional third party solvers (disabled by default)
option(USE_CPLEX "Use the CPLEX solver" OFF)
message(STATUS "CPLEX support: ${USE_CPLEX}")

option(USE_SCIP "Use the SCIP solver" OFF)
message(STATUS "SCIP support: ${USE_SCIP}")

option(USE_XPRESS "Use the XPRESS solver" OFF)
message(STATUS "XPRESS support: ${USE_XPRESS}")

# Build Needed dependencies
add_subdirectory(cmake/dependencies dependencies)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/dependencies/install)

include(CTest)

# Basic type
include(CMakePushCheckState)
cmake_push_check_state(RESET)
set(CMAKE_EXTRA_INCLUDE_FILES "cstdint")
include(CheckTypeSize)
check_type_size("long" SIZEOF_LONG LANGUAGE CXX)
message(STATUS "Found long size: ${SIZEOF_LONG}")
check_type_size("long long" SIZEOF_LONG_LONG LANGUAGE CXX)
message(STATUS "Found long long size: ${SIZEOF_LONG_LONG}")
check_type_size("int64_t" SIZEOF_INT64_T LANGUAGE CXX)
message(STATUS "Found int64_t size: ${SIZEOF_INT64_T}")

check_type_size("unsigned long" SIZEOF_ULONG LANGUAGE CXX)
message(STATUS "Found unsigned long size: ${SIZEOF_ULONG}")
check_type_size("unsigned long long" SIZEOF_ULONG_LONG LANGUAGE CXX)
message(STATUS "Found unsigned long long size: ${SIZEOF_ULONG_LONG}")
check_type_size("uint64_t" SIZEOF_UINT64_T LANGUAGE CXX)
message(STATUS "Found uint64_t size: ${SIZEOF_UINT64_T}")

check_type_size("int *" SIZEOF_INT_P LANGUAGE CXX)
message(STATUS "Found int * size: ${SIZEOF_INT_P}")
cmake_pop_check_state()

include(cpp)

include(python)
include(java)
include(dotnet)
