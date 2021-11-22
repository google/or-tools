# Check dependencies
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREAD_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# Tell find_package() to try “Config” mode before “Module” mode if no mode was specified.
# This should avoid find_package() to first find our FindXXX.cmake modules if
# distro package already provide a CMake config file...
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

# libprotobuf force us to depends on ZLIB::ZLIB target
if(NOT BUILD_ZLIB)
 find_package(ZLIB REQUIRED)
endif()
if(NOT TARGET ZLIB::ZLIB)
  message(FATAL_ERROR "Target ZLIB::ZLIB not available.")
endif()

if(NOT BUILD_absl)
  find_package(absl REQUIRED)
endif()
set(ABSL_DEPS
  absl::base
  absl::flags
  absl::flags_commandlineflag
  absl::flags_parse
  absl::flags_usage
  absl::cord
  absl::random_random
  absl::raw_hash_set
  absl::hash
  absl::leak_check
  absl::memory
  absl::meta
  absl::stacktrace
  absl::status
  absl::statusor
  absl::str_format
  absl::strings
  absl::synchronization
  absl::any
  )

if(NOT BUILD_Protobuf)
  find_package(Protobuf REQUIRED)
endif()
if(NOT TARGET protobuf::libprotobuf)
  message(FATAL_ERROR "Target protobuf::libprotobuf not available.")
endif()

if(BUILD_LP_PARSER)
  if(NOT BUILD_re2)
    find_package(re2 REQUIRED)
  endif()
  if(NOT TARGET re2::re2)
    message(FATAL_ERROR "Target re2::re2 not available.")
  endif()
  set(RE2_DEPS re2::re2)
endif()

if(USE_SCIP)
  if(NOT BUILD_SCIP)
    find_package(SCIP REQUIRED)
  endif()
endif()

if(USE_GLPK)
  if(NOT BUILD_GLPK)
    find_package(GLPK REQUIRED)
  endif()
endif()

if(USE_COINOR)
  if(NOT BUILD_CoinUtils)
    find_package(CoinUtils REQUIRED)
  endif()

  if(NOT BUILD_Osi)
    find_package(Osi REQUIRED)
  endif()

  if(NOT BUILD_Clp)
    find_package(Clp REQUIRED)
  endif()

  if(NOT BUILD_Cgl)
    find_package(Cgl REQUIRED)
  endif()

  if(NOT BUILD_Cbc)
    find_package(Cbc REQUIRED)
  endif()

  set(COINOR_DEPS Coin::CbcSolver Coin::OsiCbc Coin::ClpSolver Coin::OsiClp)
endif()

# Check optional Dependencies
if(USE_CPLEX)
  find_package(CPLEX REQUIRED)
endif()

if(USE_XPRESS)
  find_package(XPRESS REQUIRED)
endif()
