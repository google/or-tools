# ##############################################################################
# SWIG (WIN32)
# ##############################################################################
if(WIN32
   AND (BUILD_PYTHON
        OR BUILD_JAVA
        OR BUILD_DOTNET))
  message(CHECK_START "Fetching SWIG")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/SWIG.CMakeLists.txt.in
                 ${CMAKE_CURRENT_BINARY_DIR}/SWIG/CMakeLists.txt @ONLY)

  execute_process(
    COMMAND ${CMAKE_COMMAND} -H. -Bproject_build -G "${CMAKE_GENERATOR}"
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/SWIG)
  if(result)
    message(FATAL_ERROR "CMake step for SWIG failed: ${result}")
  endif()

  execute_process(
    COMMAND ${CMAKE_COMMAND} --build project_build --config ${CMAKE_BUILD_TYPE}
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/SWIG)
  if(result)
    message(FATAL_ERROR "Build step for SWIG failed: ${result}")
  endif()

  set(SWIG_EXECUTABLE
      ${CMAKE_CURRENT_BINARY_DIR}/SWIG/source/swig.exe
      CACHE INTERNAL "swig.exe location" FORCE)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_TESTING OFF)

# ##############################################################################
# ZLIB
# ##############################################################################
if(BUILD_ZLIB)
  message(CHECK_START "Fetching ZLIB")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  FetchContent_Declare(
    zlib
    GIT_REPOSITORY "https://github.com/madler/ZLIB.git"
    GIT_TAG "v1.2.11"
    PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/ZLIB.patch")
  FetchContent_MakeAvailable(zlib)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# ABSEIL-CPP
# ##############################################################################
if(BUILD_absl)
  message(CHECK_START "Fetching Abseil-cpp")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  set(ABSL_ENABLE_INSTALL ON)
  FetchContent_Declare(
    absl
    GIT_REPOSITORY "https://github.com/abseil/abseil-cpp.git"
    GIT_TAG "20211102.0"
    PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/abseil-cpp-20211102.0.patch")
  FetchContent_MakeAvailable(absl)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# Protobuf
# ##############################################################################
if(BUILD_Protobuf)
  message(CHECK_START "Fetching Protobuf")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  set(protobuf_BUILD_TESTS OFF)
  set(protobuf_BUILD_EXPORT OFF)
  set(protobuf_MSVC_STATIC_RUNTIME OFF)
  # FetchContent_Declare(SOURCE_SUBDIR) was introduced in 3.18
  if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.18")
    FetchContent_Declare(
      protobuf
      GIT_REPOSITORY "https://github.com/protocolbuffers/protobuf.git"
      GIT_TAG "v3.19.1"
      GIT_SUBMODULES ""
      PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/protobuf-v3.19.1.patch"
      SOURCE_SUBDIR cmake)
    FetchContent_MakeAvailable(protobuf)
  else()
    fetch_git_dependency(
      NAME Protobuf
      REPOSITORY "https://github.com/protocolbuffers/protobuf.git"
      TAG "v3.19.1"
      PATCH_COMMAND "git apply --ignore-whitespace \"${CMAKE_CURRENT_LIST_DIR}/../../patches/protobuf-v3.19.1.patch\""
      SOURCE_SUBDIR cmake
      )
  endif()
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# RE2
# ##############################################################################
if(BUILD_re2)
  message(CHECK_START "Fetching re2")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  set(ABSL_ENABLE_INSTALL ON)
  FetchContent_Declare(
    re2
    GIT_REPOSITORY "https://github.com/google/re2.git"
    GIT_TAG "2021-11-01"
    PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/re2-2021-11-01.patch"
  )
  FetchContent_MakeAvailable(re2)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# SCIP
# ##############################################################################
if(BUILD_SCIP)
  message(CHECK_START "Fetching SCIP")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  set(SHARED OFF)
  set(READLINE OFF)
  set(GMP OFF)
  set(PAPILO OFF)
  set(ZIMPL OFF)
  set(IPOPT OFF)
  set(AMPL OFF)

  set(TPI "tny" CACHE STRING "Scip param")
  set(EXPRINT "none" CACHE STRING "Scip param")
  set(LPS "none" CACHE STRING "Scip param")
  set(SYM "none" CACHE STRING "Scip param")
  FetchContent_Declare(
    scip
    GIT_REPOSITORY https://github.com/scipopt/scip.git
    GIT_TAG        master
  )
  FetchContent_MakeAvailable(scip)
  set(LPI_GLOP_SRC ${scip_SOURCE_DIR}/src/lpi/lpi_glop.cpp PARENT_SCOPE)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# GLPK
# ##############################################################################
if(BUILD_GLPK)
  message(CHECK_START "Fetching GLPK")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  set(BUILD_EXAMPLES OFF)
  set(WITH_GMP OFF)
  set(WITH_ODBC OFF)
  set(WITH_MYSQL OFF)

  FetchContent_Declare(
    glpk
    GIT_REPOSITORY https://github.com/Mizux/GLPK.git
    GIT_TAG        5.0
  )
  FetchContent_MakeAvailable(glpk)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# Coinutils
# ##############################################################################
# Coin-OR does not support C++17 (use of 'register' storage class specifier)
set(CMAKE_CXX_STANDARD 11)

if(BUILD_CoinUtils)
  message(CHECK_START "Fetching CoinUtils")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  FetchContent_Declare(
    CoinUtils
    GIT_REPOSITORY "https://github.com/Mizux/CoinUtils.git"
    GIT_TAG "stable/2.11"
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/coinutils-2.11.patch")
  FetchContent_MakeAvailable(CoinUtils)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# Osi
# ##############################################################################
if(BUILD_Osi)
  message(CHECK_START "Fetching Osi")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  FetchContent_Declare(
    Osi
    GIT_REPOSITORY "https://github.com/Mizux/Osi.git"
    GIT_TAG "stable/0.108"
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/osi-0.108.patch")
  FetchContent_MakeAvailable(Osi)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# Clp
# ##############################################################################
if(BUILD_Clp)
  message(CHECK_START "Fetching Clp")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  FetchContent_Declare(
    Clp
    GIT_REPOSITORY "https://github.com/Mizux/Clp.git"
    GIT_TAG "stable/1.17.4"
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/clp-1.17.4.patch")
  FetchContent_MakeAvailable(Clp)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# Cgl
# ##############################################################################
if(BUILD_Cgl)
  message(CHECK_START "Fetching Cgl")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  FetchContent_Declare(
    Cgl
    GIT_REPOSITORY "https://github.com/Mizux/Cgl.git"
    GIT_TAG "stable/0.60"
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/cgl-0.60.patch")
  FetchContent_MakeAvailable(Cgl)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# Cbc
# ##############################################################################
if(BUILD_Cbc)
  message(CHECK_START "Fetching Cbc")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  FetchContent_Declare(
    Cbc
    GIT_REPOSITORY "https://github.com/Mizux/Cbc.git"
    GIT_TAG "stable/2.10"
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/cbc-2.10.patch")
  FetchContent_MakeAvailable(Cbc)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

set(CMAKE_CXX_STANDARD 17)
