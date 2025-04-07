# Copyright 2010-2025 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
    COMMAND ${CMAKE_COMMAND} --build project_build --config Release
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
set(BUILD_SHARED_LIBS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_TESTING OFF)
set(CMAKE_Fortran_COMPILER OFF)

# ##############################################################################
# ZLIB
# ##############################################################################
if(BUILD_ZLIB)
  message(CHECK_START "Fetching ZLIB")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  set(ZLIB_BUILD_EXAMPLES OFF)
  FetchContent_Declare(
    ZLIB
    GIT_REPOSITORY "https://github.com/madler/ZLIB.git"
    GIT_TAG "v1.3.1"
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/ZLIB-v1.3.1.patch")
  FetchContent_MakeAvailable(ZLIB)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# BZip2
# ##############################################################################
if(BUILD_BZip2)
  message(CHECK_START "Fetching BZip2")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  set(ENABLE_LIB_ONLY ON)
  set(ENABLE_TESTS OFF)
  FetchContent_Declare(
    BZip2
    GIT_REPOSITORY "https://gitlab.com/bzip2/bzip2.git"
    GIT_TAG "master"
    # GIT_TAG "bzip2-1.0.8" # CMake support not available
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/bzip2.patch"
  )
  FetchContent_MakeAvailable(BZip2)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# ABSEIL-CPP
# ##############################################################################
if(BUILD_absl)
  message(CHECK_START "Fetching Abseil-cpp")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  set(ABSL_USE_SYSTEM_INCLUDES ON)
  # We want Abseil to declare what C++ standard it was compiled with.
  set(ABSL_PROPAGATE_CXX_STD ON)
  # We want Abseil to keep the INSTALL rules enabled, even though it is a
  # subproject. Otherwise the install rules in this project break.
  set(ABSL_ENABLE_INSTALL ON)
  set(ABSL_BUILD_TESTING OFF)
  FetchContent_Declare(
    absl
    GIT_REPOSITORY "https://github.com/abseil/abseil-cpp.git"
    GIT_TAG "20250127.1"
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/abseil-cpp-20250127.1.patch"
    OVERRIDE_FIND_PACKAGE
  )
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
  set(protobuf_BUILD_SHARED_LIBS ON)
  set(protobuf_BUILD_EXPORT OFF)
  set(protobuf_MSVC_STATIC_RUNTIME OFF)
  #set(protobuf_BUILD_LIBUPB ON)
  FetchContent_Declare(
    Protobuf
    GIT_REPOSITORY "https://github.com/protocolbuffers/protobuf.git"
    GIT_TAG "v30.2"
    GIT_SHALLOW TRUE
    GIT_SUBMODULES ""
    PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/protobuf-v30.2.patch"
  )
  FetchContent_MakeAvailable(Protobuf)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# RE2
# ##############################################################################
if(BUILD_re2)
  message(CHECK_START "Fetching re2")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  set(RE2_BUILD_TESTING OFF)
  FetchContent_Declare(
    re2
    GIT_REPOSITORY "https://github.com/google/re2.git"
    GIT_TAG "2024-04-01"
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/re2-2024-04-01.patch"
  )
  FetchContent_MakeAvailable(re2)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# EIGEN3
# ##############################################################################
if(BUILD_Eigen3)
  message(CHECK_START "Fetching Eigen3")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  set(EIGEN_MPL2_ONLY ON)
  set(EIGEN_BUILD_PKGCONFIG OFF)
  set(EIGEN_BUILD_DOC OFF)
  set(EIGEN_BUILD_TESTING OFF)
  FetchContent_Declare(
    eigen3
    GIT_REPOSITORY "https://gitlab.com/libeigen/eigen.git"
    GIT_TAG "3.4.0"
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/eigen3-3.4.0.patch"
  )
  FetchContent_MakeAvailable(eigen3)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# pybind11
# ##############################################################################
if(BUILD_PYTHON)
  # Find Python 3
  find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)

  if(BUILD_pybind11)
    message(CHECK_START "Fetching pybind11")
    list(APPEND CMAKE_MESSAGE_INDENT "  ")
    set(PYBIND11_FINDPYTHON ON)
    set(PYBIND11_INSTALL ON)
    set(PYBIND11_TEST OFF)
    FetchContent_Declare(
      pybind11
      GIT_REPOSITORY "https://github.com/pybind/pybind11.git"
      GIT_TAG "v2.13.6"
      GIT_SHALLOW TRUE
      PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/pybind11.patch"
    )
    FetchContent_MakeAvailable(pybind11)
    list(POP_BACK CMAKE_MESSAGE_INDENT)
    message(CHECK_PASS "fetched")
  endif()

  if(BUILD_pybind11_abseil)
    message(CHECK_START "Fetching pybind11_abseil")
    list(APPEND CMAKE_MESSAGE_INDENT "  ")
    FetchContent_Declare(
      pybind11_abseil
      GIT_REPOSITORY "https://github.com/pybind/pybind11_abseil.git" # 2024/01/11
      GIT_TAG "v202402.0"
      GIT_SHALLOW TRUE
      PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/pybind11_abseil.patch"
    )
    FetchContent_MakeAvailable(pybind11_abseil)
    list(POP_BACK CMAKE_MESSAGE_INDENT)
    message(CHECK_PASS "fetched")
  endif()

  if(BUILD_pybind11_protobuf)
    message(CHECK_START "Fetching pybind11_protobuf")
    list(APPEND CMAKE_MESSAGE_INDENT "  ")
    FetchContent_Declare(
      pybind11_protobuf
      GIT_REPOSITORY "https://github.com/pybind/pybind11_protobuf.git"
      GIT_TAG "f02a2b7653bc50eb5119d125842a3870db95d251" # 2025/02/10
      #PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/pybind11_protobuf.patch"
      #GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(pybind11_protobuf)
    list(POP_BACK CMAKE_MESSAGE_INDENT)
    message(CHECK_PASS "fetched")
  endif()
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
    GIT_REPOSITORY "https://github.com/Mizux/GLPK.git"
    GIT_TAG        "5.0"
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(glpk)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# HiGHS
# ##############################################################################
if(BUILD_HIGHS)
  message(CHECK_START "Fetching HiGHS")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  set(CI OFF) # disable CI tests
  set(BUILD_EXAMPLES OFF)
  FetchContent_Declare(
    highs
    GIT_REPOSITORY "https://github.com/ERGO-Code/HiGHS.git"
    GIT_TAG        "v1.10.0"
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/highs-v1.10.0.patch"
  )
  FetchContent_MakeAvailable(highs)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# BOOST
# ##############################################################################
if(BUILD_Boost)
  message(CHECK_START "Fetching Boost")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  FetchContent_Declare(
    Boost
    URL
    https://github.com/boostorg/boost/releases/download/boost-1.87.0/boost-1.87.0-cmake.tar.gz
    USES_TERMINAL_DOWNLOAD TRUE
    DOWNLOAD_NO_EXTRACT FALSE
    PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/boost-1.87.0.patch"
  )
  set(BOOST_INCLUDE_LIBRARIES multiprecision serialization)
  option(BOOST_SKIP_INSTALL_RULES NO)
  set(BOOST_ENABLE_CMAKE ON)
  set(BUILD_SHARED_LIBS OFF)
  FetchContent_MakeAvailable(Boost)
  set(BUILD_SHARED_LIBS ON)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# SOPLEX
# ##############################################################################
if(BUILD_soplex)
  message(CHECK_START "Fetching SOPLEX")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  FetchContent_Declare(
    soplex
    GIT_REPOSITORY "https://github.com/scipopt/soplex.git"
    GIT_TAG        "release-713"
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/soplex-v7.1.3.patch"
  )
  set(SHARED ON CACHE BOOL "Soplex param" FORCE)
  set(ZLIB ON CACHE BOOL "Soplex param" FORCE)
  set(GMP OFF CACHE BOOL "Soplex param" FORCE)
  set(BOOST ON CACHE BOOL "Soplex param" FORCE)
  set(QUADMATH  OFF CACHE BOOL "Soplex param" FORCE)
  set(MPFR OFF CACHE BOOL "Soplex param" FORCE)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON CACHE BOOL "Soplex param" FORCE)
  set(MT OFF CACHE BOOL "Soplex param" FORCE)
  set(PAPILO OFF CACHE BOOL "Soplex param" FORCE)
  #set(Boost_INCLUDE_DIR "../boost-src/libs/config/include" CACHE PATH "Soplex param" FORCE)
  FetchContent_MakeAvailable(soplex)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# SCIP
# ##############################################################################
if(BUILD_SCIP)
  message(CHECK_START "Fetching SCIP")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  FetchContent_Declare(
    scip
    GIT_REPOSITORY "https://github.com/scipopt/scip.git"
    GIT_TAG        "v921"
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/scip-v921.patch"
  )
  set(SHARED ON CACHE BOOL "Scip param" FORCE)
  set(ZLIB ON CACHE BOOL "Scip param" FORCE)
  set(READLINE OFF CACHE BOOL "Scip param" FORCE)
  set(GMP OFF CACHE BOOL "Scip param" FORCE)
  set(PAPILO OFF CACHE BOOL "Scip param" FORCE)
  set(ZIMPL OFF CACHE BOOL "Scip param" FORCE)
  set(AMPL OFF CACHE BOOL "Scip param" FORCE)
  set(IPOPT OFF CACHE BOOL "Scip param" FORCE)
  set(TPI "tny" CACHE STRING "Scip param" FORCE)
  set(EXPRINT "none" CACHE STRING "Scip param" FORCE)
  set(LPS "spx" CACHE STRING "Scip param" FORCE)
  set(SYM "none" CACHE STRING "Scip param" FORCE)
  FetchContent_MakeAvailable(scip)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# Coinutils
# ##############################################################################
# Coin-OR does not support C++17/C++20 (use of 'register' storage class specifier)
set(CMAKE_CXX_STANDARD 11)
if(WIN32)
  set(BUILD_SHARED_LIBS OFF)
endif()

if(BUILD_CoinUtils)
  message(CHECK_START "Fetching CoinUtils")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  FetchContent_Declare(
    CoinUtils
    GIT_REPOSITORY "https://github.com/Mizux/CoinUtils.git"
    GIT_TAG "cmake/2.11.12"
    GIT_SHALLOW TRUE
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
    GIT_TAG "cmake/0.108.11"
    GIT_SHALLOW TRUE
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
    GIT_TAG "cmake/1.17.10"
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/clp-1.17.patch")
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
    GIT_TAG "cmake/0.60.9"
    GIT_SHALLOW TRUE
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
    GIT_TAG "cmake/2.10.12"
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/cbc-2.10.patch")
  FetchContent_MakeAvailable(Cbc)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

if(WIN32)
  set(BUILD_SHARED_LIBS ON)
endif()
if(MSVC)
  set(CMAKE_CXX_STANDARD 20)
else()
  set(CMAKE_CXX_STANDARD 17)
endif()

###############
##  TESTING  ##
###############
if(BUILD_googletest)
  message(CHECK_START "Fetching googletest")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.16.0
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/googletest-v1.16.0.patch"
    #PATCH_COMMAND git apply --ignore-whitespace ""
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF)
  set(GTEST_HAS_ABSL ON)
  FetchContent_MakeAvailable(googletest)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

if(BUILD_benchmark)
  message(CHECK_START "Fetching benchmark")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.1
    GIT_SHALLOW TRUE
    #PATCH_COMMAND git apply --ignore-whitespace ""
  )
  set(BENCHMARK_ENABLE_TESTING OFF)
  set(BENCHMARK_ENABLE_WERROR OFF)
  set(BENCHMARK_ENABLE_INSTALL OFF)
  FetchContent_MakeAvailable(benchmark)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

if(BUILD_fuzztest)
  message(CHECK_START "Fetching fuzztest")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  FetchContent_Declare(
    fuzztest
    GIT_REPOSITORY https://github.com/google/fuzztest.git
    GIT_TAG 2025-02-14
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/fuzztest-2025-02-14.patch"
  )
  FetchContent_MakeAvailable(fuzztest)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()
