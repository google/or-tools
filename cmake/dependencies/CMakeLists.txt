# Copyright 2010-2024 Google LLC
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
set(BUILD_SHARED_LIBS OFF)
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
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/ZLIB-v1.3.1.patch")
  FetchContent_MakeAvailable(ZLIB)
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
  set(ABSL_USE_SYSTEM_INCLUDES ON)
  set(ABSL_PROPAGATE_CXX_STD ON)
  set(ABSL_BUILD_TESTING OFF)
  FetchContent_Declare(
    absl
    GIT_REPOSITORY "https://github.com/abseil/abseil-cpp.git"
    GIT_TAG "20240722.0"
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/abseil-cpp-20240722.0.patch"
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
  set(protobuf_BUILD_SHARED_LIBS OFF)
  set(protobuf_BUILD_EXPORT OFF)
  set(protobuf_MSVC_STATIC_RUNTIME OFF)
  #set(protobuf_BUILD_LIBUPB ON)
  FetchContent_Declare(
    Protobuf
    GIT_REPOSITORY "https://github.com/protocolbuffers/protobuf.git"
    GIT_TAG "v26.1"
    GIT_SHALLOW TRUE
    GIT_SUBMODULES ""
    PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/protobuf-v26.1.patch"
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
endif()

if(BUILD_PYTHON AND BUILD_pybind11)
  message(CHECK_START "Fetching pybind11")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  set(PYBIND11_FINDPYTHON ON)
  set(PYBIND11_INSTALL ON)
  set(PYBIND11_TEST OFF)
  FetchContent_Declare(
    pybind11
    GIT_REPOSITORY "https://github.com/pybind/pybind11.git"
    GIT_TAG "v2.13.1"
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/pybind11.patch"
  )
  FetchContent_MakeAvailable(pybind11)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

if(BUILD_PYTHON AND BUILD_pybind11_abseil)
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

if(BUILD_PYTHON AND BUILD_pybind11_protobuf)
  message(CHECK_START "Fetching pybind11_protobuf")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  FetchContent_Declare(
    pybind11_protobuf
    GIT_REPOSITORY "https://github.com/pybind/pybind11_protobuf.git"
    GIT_TAG "84653a591aea5df482dc2bde42c19efafbd53a57" # 2024/06/28
    PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/pybind11_protobuf.patch"
    #GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(pybind11_protobuf)
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
    GIT_TAG        "v1.8.0"
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/highs-v1.8.0.patch"
  )
  FetchContent_MakeAvailable(highs)
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
    GIT_REPOSITORY "https://github.com/scipopt/scip.git"
    GIT_TAG        "v900"
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/../../patches/scip-v900.patch"
  )
  FetchContent_MakeAvailable(scip)
  set(LPI_GLOP_SRC ${scip_SOURCE_DIR}/src/lpi/lpi_glop.cpp PARENT_SCOPE)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
endif()

# ##############################################################################
# Coinutils
# ##############################################################################
# Coin-OR does not support C++17/C++20 (use of 'register' storage class specifier)
set(CMAKE_CXX_STANDARD 11)

if(BUILD_CoinUtils)
  message(CHECK_START "Fetching CoinUtils")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
  FetchContent_Declare(
    CoinUtils
    GIT_REPOSITORY "https://github.com/Mizux/CoinUtils.git"
    GIT_TAG "stable/2.11"
    #GIT_TAG "cmake/2.11.6"
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
    GIT_TAG "stable/0.108"
    #GIT_TAG "cmake/0.108.7"
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
    GIT_TAG "stable/1.17"
    #GIT_TAG "cmake/1.17.7"
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
    GIT_TAG "stable/0.60"
    #GIT_TAG "cmake/0.60.5"
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
    GIT_TAG "stable/2.10"
    #GIT_TAG "cmake/2.10.7"
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/cbc-2.10.patch")
  FetchContent_MakeAvailable(Cbc)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")
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
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply --ignore-whitespace
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/googletest-v1.15.2.patch"
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
    GIT_TAG v1.8.4
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
