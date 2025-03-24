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
if(NOT BUILD_ZLIB)
 find_package(ZLIB REQUIRED)
endif()

if(NOT BUILD_absl)
  find_package(absl REQUIRED)
endif()

if(NOT BUILD_Protobuf)
  find_package(Protobuf REQUIRED)
endif()

if(NOT BUILD_Eigen3)
  find_package(Eigen3 REQUIRED)
endif()

if(NOT BUILD_re2 AND NOT TARGET re2::re2)
  find_package(re2 REQUIRED)
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
endif()

if(USE_GLPK AND NOT BUILD_GLPK)
  find_package(GLPK REQUIRED)
endif()

if(USE_HIGHS AND NOT BUILD_HIGHS)
  find_package(HIGHS REQUIRED)
endif()

if(USE_PDLP AND NOT BUILD_PDLP)
  find_package(PDLP REQUIRED)
endif()

if(USE_SCIP AND NOT BUILD_SCIP)
  find_package(SCIP REQUIRED)
endif()

# Check optional Dependencies
if(USE_CPLEX)
  find_package(CPLEX REQUIRED)
endif()

# CXX Test
if(BUILD_TESTING AND NOT BUILD_googletest)
  find_package(GTest REQUIRED)
endif()

if(BUILD_TESTING AND NOT BUILD_benchmark)
  find_package(benchmark REQUIRED)
endif()

if(BUILD_TESTING AND NOT BUILD_fuzztest)
  find_package(fuzztest REQUIRED)
endif()

# Check language Dependencies
if(BUILD_PYTHON)
  if(NOT BUILD_pybind11)
    find_package(pybind11 REQUIRED)
  endif()

  if(NOT BUILD_pybind11_abseil)
    find_package(pybind11_abseil REQUIRED)
  endif()

  if(NOT BUILD_pybind11_protobuf)
    find_package(pybind11_protobuf REQUIRED)
  endif()
endif()
