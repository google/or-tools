# This file is just an orchestration
cmake_minimum_required(VERSION 3.15)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(utils)
set_version(VERSION)

project(ortools VERSION ${VERSION} LANGUAGES CXX C)
set(PROJECT_NAMESPACE ortools)
message(STATUS "${PROJECT_NAME} version: ${PROJECT_VERSION}")
#message(STATUS "major: ${PROJECT_VERSION_MAJOR}")
#message(STATUS "minor: ${PROJECT_VERSION_MINOR}")
#message(STATUS "patch: ${PROJECT_VERSION_PATCH}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set max os target version.
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)

# Default Build Type to be Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel. (default: Release)"
    FORCE)
endif()

# Layout build dir like install dir
include(GNUInstallDirs)
if(UNIX)
  option(BUILD_SHARED_LIBS "Build shared libraries (.so or .dyld)." ON)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  # for multi-config build system (e.g. xcode)
  foreach(OUTPUTCONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_LIBDIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_LIBDIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_BINDIR})
  endforeach()
else()
  # Currently Only support static build for windows
  option(BUILD_SHARED_LIBS "Build shared libraries (.dll)." OFF)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  # for multi-config builds (e.g. msvc)
  foreach(OUTPUTCONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_BINDIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_BINDIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/${CMAKE_INSTALL_BINDIR})
  endforeach()
endif()

if(BUILD_SHARED_LIBS AND MSVC)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
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

# If wrapper are built, we need to have the install rpath in BINARY_DIR to package
if(BUILD_PYTHON OR BUILD_JAVA OR BUILD_DOTNET)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

include(CMakeDependentOption)

CMAKE_DEPENDENT_OPTION(BUILD_FLATZINC "Build flatzinc" ON "BUILD_CXX" OFF)
message(STATUS "Build Flatzinc: ${BUILD_FLATZINC}")

CMAKE_DEPENDENT_OPTION(BUILD_GLOP "Build GLOP standalone" ON "NOT BUILD_CXX" OFF)
message(STATUS "Build standalone Glop: ${BUILD_GLOP}")

option(BUILD_LP_PARSER "Build lp_parser" OFF)
message(STATUS "Build LP Parser: ${BUILD_LP_PARSER}")

option(BUILD_SAMPLES "Build samples" ON)
message(STATUS "Build samples: ${BUILD_SAMPLES}")
CMAKE_DEPENDENT_OPTION(BUILD_CXX_SAMPLES "Build cxx samples" ON "BUILD_SAMPLES;BUILD_CXX" OFF)
message(STATUS "Build C++ samples: ${BUILD_CXX_SAMPLES}")
CMAKE_DEPENDENT_OPTION(BUILD_PYTHON_SAMPLES "Build python samples" ON "BUILD_SAMPLES;BUILD_PYTHON" OFF)
message(STATUS "Build Python samples: ${BUILD_PYTHON_SAMPLES}")
CMAKE_DEPENDENT_OPTION(BUILD_JAVA_SAMPLES "Build java samples" ON "BUILD_SAMPLES;BUILD_JAVA" OFF)
message(STATUS "Build Java samples: ${BUILD_JAVA_SAMPLES}")
CMAKE_DEPENDENT_OPTION(BUILD_DOTNET_SAMPLES "Build dotnet samples" ON "BUILD_SAMPLES;BUILD_DOTNET" OFF)
message(STATUS "Build .Net samples: ${BUILD_DOTNET_SAMPLES}")


option(BUILD_EXAMPLES "Build examples" ON)
message(STATUS "Build examples: ${BUILD_EXAMPLES}")
CMAKE_DEPENDENT_OPTION(BUILD_CXX_EXAMPLES "Build cxx examples" ON "BUILD_EXAMPLES;BUILD_CXX" OFF)
message(STATUS "Build C++ examples: ${BUILD_CXX_EXAMPLES}")
CMAKE_DEPENDENT_OPTION(BUILD_PYTHON_EXAMPLES "Build python examples" ON "BUILD_EXAMPLES;BUILD_PYTHON" OFF)
message(STATUS "Build Python examples: ${BUILD_PYTHON_EXAMPLES}")
CMAKE_DEPENDENT_OPTION(BUILD_JAVA_EXAMPLES "Build java examples" ON "BUILD_EXAMPLES;BUILD_JAVA" OFF)
message(STATUS "Build Java examples: ${BUILD_JAVA_EXAMPLES}")
CMAKE_DEPENDENT_OPTION(BUILD_DOTNET_EXAMPLES "Build dotnet examples" ON "BUILD_EXAMPLES;BUILD_DOTNET" OFF)
message(STATUS "Build .Net examples: ${BUILD_DOTNET_EXAMPLES}")

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
CMAKE_DEPENDENT_OPTION(BUILD_ZLIB "Build the ZLIB dependency Library" OFF
  "NOT BUILD_DEPS" ON)
message(STATUS "Build ZLIB: ${BUILD_ZLIB}")

CMAKE_DEPENDENT_OPTION(BUILD_absl "Build the abseil-cpp dependency Library" OFF
  "NOT BUILD_DEPS" ON)
message(STATUS "Build abseil-cpp: ${BUILD_absl}")

CMAKE_DEPENDENT_OPTION(BUILD_Protobuf "Build the Protobuf dependency Library" OFF
  "NOT BUILD_DEPS" ON)
message(STATUS "Build protobuf: ${BUILD_Protobuf}")

if(BUILD_LP_PARSER)
  CMAKE_DEPENDENT_OPTION(BUILD_re2 "Build the re2 dependency Library" OFF
    "NOT BUILD_DEPS" ON)
  message(STATUS "Build re2: ${BUILD_re2}")
endif()

# Optional third party solvers (enabled by default)
CMAKE_DEPENDENT_OPTION(USE_SCIP "Use the Scip solver" ON "BUILD_CXX" OFF)
message(STATUS "SCIP support: ${USE_SCIP}")
if(USE_SCIP)
  CMAKE_DEPENDENT_OPTION(BUILD_SCIP "Build the SCIP dependency Library" OFF
    "NOT BUILD_DEPS" ON)
  message(STATUS "Build SCIP: ${BUILD_SCIP}")
endif()

# Disable by default since it is GLPv3, user could enable it and release under GLPv3
# see: https://www.apache.org/licenses/GPL-compatibility.html
CMAKE_DEPENDENT_OPTION(USE_GLPK "Use the GLPK solver" OFF "BUILD_CXX" OFF)
message(STATUS "GLPK support: ${USE_GLPK}")
if(USE_GLPK)
  CMAKE_DEPENDENT_OPTION(BUILD_GLPK "Build the GLPK dependency Library" OFF
    "NOT BUILD_DEPS" ON)
  message(STATUS "Build GLPK: ${BUILD_GLPK}")
endif()

CMAKE_DEPENDENT_OPTION(USE_COINOR "Use the COIN-OR solver" ON "BUILD_CXX" OFF)
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

option(USE_XPRESS "Use the XPRESS solver" OFF)
message(STATUS "XPRESS support: ${USE_XPRESS}")

if(BUILD_JAVA)
  option(BUILD_FAT_JAR "Create single .jar with all dependencies (including native binaries)" OFF)
  message(STATUS "Java build single fat .jar: ${BUILD_FAT_JAR}")

  option(SKIP_GPG "Disable GPG sign" ON)
  message(STATUS "Java disable gpg:sign: ${SKIP_GPG}")

  option(UNIVERSAL_JAVA_PACKAGE "Build a Java multi OS Package" OFF)
  message(STATUS "Java multiple os package: ${UNIVERSAL_JAVA_PACKAGE}")

  set(GPG_ARGS "<arg>--pinentry-mode</arg><arg>loopback</arg>" CACHE STRING "Extra options for GPG")
  message(STATUS "Java GPG options: ${GPG_ARGS}")
endif()

if(BUILD_PYTHON)
  option(FETCH_PYTHON_DEPS "Install python required modules if not available" ${BUILD_DEPS})
  message(STATUS "Python fetch dependencies: ${FETCH_PYTHON_DEPS}")
endif()

# Build Needed dependencies
add_subdirectory(cmake/dependencies dependencies)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_BINARY_DIR}/dependencies/install)

# Disable CTest targets
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
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

include(host)
include(deps)
include(cpp)
include(flatzinc)
include(glop)

include(python)
include(java)
include(dotnet)

# Since samples mix all languages we must parse them once we have included all
# <language>.cmake files
foreach(SAMPLES IN ITEMS algorithms graph linear_solver constraint_solver sat)
  add_subdirectory(ortools/${SAMPLES}/samples)
endforeach()

# Same for examples
foreach(EXAMPLES IN ITEMS contrib cpp python java dotnet)
  add_subdirectory(examples/${EXAMPLES})
endforeach()

# Add tests in examples/tests
add_subdirectory(examples/tests)
