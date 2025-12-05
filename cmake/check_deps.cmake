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
if(NOT TARGET ZLIB::ZLIB)
  message(FATAL_ERROR "Target ZLIB::ZLIB not available.")
endif()

if(NOT TARGET BZip2::BZip2)
  message(FATAL_ERROR "Target BZip2::BZip2 not available.")
endif()

if(NOT TARGET absl::base)
  message(FATAL_ERROR "Target absl::base not available.")
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
  absl::flags_reflection
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

if(NOT TARGET protobuf::libprotobuf)
  message(FATAL_ERROR "Target protobuf::libprotobuf not available.")
endif()

if(NOT TARGET Eigen3::Eigen)
  message(FATAL_ERROR "Target Eigen3::Eigen not available.")
endif()

if(NOT TARGET re2::re2)
  message(FATAL_ERROR "Target re2::re2 not available.")
endif()
set(RE2_DEPS re2::re2)

if(USE_COINOR)
  if(NOT TARGET Coin::CbcSolver)
    message(FATAL_ERROR "Target Coin::CbcSolver not available.")
  endif()
  if(NOT TARGET Coin::ClpSolver)
    message(FATAL_ERROR "Target Coin::ClpSolver not available.")
  endif()
  set(COINOR_DEPS Coin::CbcSolver Coin::OsiCbc Coin::ClpSolver Coin::OsiClp)
endif()

if(USE_CPLEX)
  if(NOT TARGET CPLEX::CPLEX)
    message(FATAL_ERROR "Target CPLEX::CPLEX not available.")
  endif()
  set(CPLEX_DEPS CPLEX::CPLEX)
endif()

if(USE_GLPK)
  if(NOT TARGET GLPK::GLPK)
    message(FATAL_ERROR "Target GLPK::GLPK not available.")
  endif()
  set(GLPK_DEPS GLPK::GLPK)
endif()

if(USE_HIGHS)
  if(NOT TARGET highs::highs)
    message(FATAL_ERROR "Target highs::highs not available.")
  endif()
  set(HIGHS_DEPS highs::highs)
endif()

if(USE_PDLP AND BUILD_PDLP)
  set(PDLP_DEPS Eigen3::Eigen)
endif()

if(USE_SCIP)
  if(NOT TARGET SCIP::libscip)
    message(FATAL_ERROR "Target SCIP::libscip not available.")
  endif()
  set(SCIP_DEPS SCIP::libscip)
endif()

# CXX Test
if(BUILD_TESTING)
  if(NOT TARGET GTest::gtest_main)
    message(FATAL_ERROR "Target GTest::gtest_main not available.")
  endif()
  if(NOT TARGET benchmark::benchmark)
    message(FATAL_ERROR "Target benchmark::benchmark not available.")
  endif()
endif()

# Check language Dependencies
if(BUILD_PYTHON)
  if(NOT TARGET pybind11::pybind11_headers)
    message(FATAL_ERROR "Target pybind11::pybind11_headers not available.")
  endif()

  if(NOT TARGET pybind11_abseil::absl_casters)
    message(FATAL_ERROR "Target pybind11_abseil::absl_casters not available.")
  endif()

  if(NOT TARGET pybind11_native_proto_caster)
    message(FATAL_ERROR "Target pybind11_native_proto_caster not available.")
  endif()
endif()
