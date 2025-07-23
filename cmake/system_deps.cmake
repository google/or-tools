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

# Check dependencies
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREAD_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# Tell find_package() to try “Config” mode before “Module” mode if no mode was specified.
# This should avoid find_package() to first find our FindXXX.cmake modules if
# distro package already provide a CMake config file...
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

# libprotobuf force us to depends on ZLIB::ZLIB target
if(NOT BUILD_ZLIB AND NOT TARGET ZLIB::ZLIB)
 find_package(ZLIB REQUIRED)
endif()

if(NOT BUILD_BZip2 AND NOT TARGET BZip2::BZip2)
 find_package(BZip2 REQUIRED)
endif()

if(NOT BUILD_absl AND NOT TARGET absl::base)
  find_package(absl REQUIRED)
endif()

if(NOT BUILD_Protobuf AND NOT TARGET protobuf::libprotobuf)
  find_package(Protobuf REQUIRED)
endif()

if(NOT BUILD_Eigen3 AND NOT TARGET Eigen3::Eigen)
  find_package(Eigen3 REQUIRED)
endif()

if(NOT BUILD_re2 AND NOT TARGET re2::re2)
  find_package(re2 REQUIRED)
endif()

# Third Party Solvers
if(USE_COINOR)
  if(NOT BUILD_CoinUtils AND NOT TARGET Coin::CoinUtils)
    find_package(CoinUtils REQUIRED)
  endif()

  if(NOT BUILD_Osi AND NOT TARGET Coin::Osi)
    find_package(Osi REQUIRED)
  endif()

  if(NOT BUILD_Clp AND NOT TARGET Coin::ClpSolver)
    find_package(Clp REQUIRED)
  endif()

  if(NOT BUILD_Cgl AND NOT TARGET Coin::Cgl)
    find_package(Cgl REQUIRED)
  endif()

  if(NOT BUILD_Cbc AND NOT TARGET Coin::CbcSolver)
    find_package(Cbc REQUIRED)
  endif()
endif()

if(USE_CPLEX)
  if(NOT TARGET CPLEX::CPLEX)
    find_package(CPLEX REQUIRED)
  endif()
endif()

if(USE_GLPK)
  if(NOT BUILD_GLPK AND NOT TARGET GLPK::GLPK)
    find_package(GLPK REQUIRED)
  endif()
endif()

if(USE_HIGHS)
  if(NOT BUILD_HIGHS AND NOT TARGET highs::highs)
    find_package(HIGHS REQUIRED)
  endif()
endif()

if(USE_PDLP)
  if(NOT BUILD_PDLP)
    find_package(PDLP REQUIRED)
  endif()
endif()

if(USE_SCIP)
  if(NOT BUILD_SCIP AND NOT TARGET SCIP::libscip)
    find_package(SCIP REQUIRED)
    if(TARGET libscip AND NOT TARGET SCIP::libscip)
      message(WARNING "SCIP::libscip not provided")
      add_library(SCIP::libscip ALIAS libscip)
    endif()
  endif()
endif()

# CXX Test
if(BUILD_TESTING)
  if(NOT BUILD_googletest AND NOT TARGET GTest::gtest_main)
    find_package(GTest REQUIRED)
  endif()

  if(NOT BUILD_benchmark AND NOT TARGET benchmark::benchmark)
    find_package(benchmark REQUIRED)
  endif()

  if(USE_fuzztest)
    if(NOT BUILD_fuzztest AND NOT TARGET fuzztest::fuzztest)
      find_package(fuzztest REQUIRED)
    endif()
  endif()
endif()

# Check language Dependencies
if(BUILD_PYTHON)
  if(NOT BUILD_pybind11 AND NOT TARGET pybind11::pybind11_headers)
    find_package(pybind11 REQUIRED)
  endif()

  if(NOT BUILD_pybind11_abseil AND NOT TARGET pybind11_abseil::absl_casters)
    find_package(pybind11_abseil REQUIRED)
  endif()

  if(NOT BUILD_pybind11_protobuf AND NOT TARGET pybind11_native_proto_caster)
    find_package(pybind11_protobuf REQUIRED)
  endif()
endif()
