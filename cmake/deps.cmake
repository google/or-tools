# Copyright 2010-2022 Google LLC
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
if(NOT TARGET ZLIB::ZLIB)
  message(FATAL_ERROR "Target ZLIB::ZLIB not available.")
endif()

if(NOT BUILD_absl)
  find_package(absl REQUIRED)
endif()
set(ABSL_DEPS
  absl::base
  absl::core_headers
  absl::absl_check
  absl::absl_log
  absl::check
  absl::die_if_null
  absl::flags
  absl::flags_commandlineflag
  absl::flags_marshalling
  absl::flags_parse
  absl::flags_usage
  absl::log
  absl::log_flags
  absl::log_globals
  absl::log_initialize
  absl::log_internal_message
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
  absl::time
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

if(USE_GLPK)
  if(NOT BUILD_GLPK)
    find_package(GLPK REQUIRED)
  endif()
endif()

if(USE_HIGHS)
  if(NOT BUILD_HIGHS)
    find_package(HIGHS REQUIRED)
  endif()
endif()

if(USE_PDLP)
  if(NOT BUILD_PDLP)
    find_package(PDLP REQUIRED)
  else()
    if(NOT BUILD_Eigen3)
      find_package(Eigen3 REQUIRED)
    endif()
    if(NOT TARGET Eigen3::Eigen)
      message(FATAL_ERROR "Target Eigen3::Eigen not available.")
    endif()
    set(PDLP_DEPS Eigen3::Eigen)
  endif()
endif()

if(USE_SCIP)
  if(NOT BUILD_SCIP)
    find_package(SCIP REQUIRED)
  endif()
endif()

# Check optional Dependencies
if(USE_CPLEX)
  find_package(CPLEX REQUIRED)
endif()

# Check language Dependencies
if(BUILD_PYTHON)
  if(NOT BUILD_pybind11)
    find_package(pybind11 REQUIRED)
  endif()

  if(NOT BUILD_pybind11_protobuf)
    find_package(pybind11_protobuf REQUIRED)
  endif()
endif()
