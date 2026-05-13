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

# This file is just an orchestration
cmake_minimum_required(VERSION 3.20)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Enable output of compile commands during generation.
option(CMAKE_EXPORT_COMPILE_COMMANDS "Export compile command" ON)

include(utils)
set_version(VERSION)

project(ortools VERSION ${VERSION} LANGUAGES CXX C)
set(PROJECT_NAMESPACE ortools)
message(STATUS "${PROJECT_NAME} version: ${PROJECT_VERSION}")
#message(STATUS "major: ${PROJECT_VERSION_MAJOR}")
#message(STATUS "minor: ${PROJECT_VERSION_MINOR}")
#message(STATUS "patch: ${PROJECT_VERSION_PATCH}")

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  # Ensure all targets—including third-party deps—are built with pthread support
  # so shared memory can be used when linking with -sUSE_PTHREADS=1.
  set(EMSCRIPTEN_USE_PTHREADS ON CACHE BOOL
      "Enable pthread support when targeting Emscripten")
  message(STATUS "Emscripten pthreads support (global): ${EMSCRIPTEN_USE_PTHREADS}")
  if(EMSCRIPTEN_USE_PTHREADS)
    add_compile_options(-pthread)
    add_link_options(-pthread -sUSE_PTHREADS=1)
  endif()
  set(EMSCRIPTEN_USE_SIMD ON CACHE BOOL
      "Enable SIMD when targeting Emscripten")
  message(STATUS "Emscripten SIMD support (global): ${EMSCRIPTEN_USE_SIMD}")
  if(EMSCRIPTEN_USE_SIMD)
    add_compile_options(-msimd128)
    add_link_options(-msimd128)
  endif()
endif()

if(MSVC)
  set(CMAKE_CXX_STANDARD 20)
else()
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set max os target version.
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)

# Default Build Type to be Release
get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(isMultiConfig)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Release;Debug" CACHE STRING
    "Choose the type of builds, options are: Debug Release RelWithDebInfo MinSizeRel. (default: Release;Debug)"
    FORCE)
  endif()
  message(STATUS "Configuration types: ${CMAKE_CONFIGURATION_TYPES}")
else()
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel. (default: Release)"
    FORCE)
  endif()
  message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif()

# Layout build dir like install dir
include(GNUInstallDirs)
if(UNIX)
  option(BUILD_SHARED_LIBS "Build shared libraries (.so or .dylib)." ON)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  # for multi-config build system (e.g. Xcode, Ninja Multi-Config)
  foreach(OutputConfig IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER ${OutputConfig} OUTPUTCONFIG)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_LIBDIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_LIBDIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_BINDIR})
  endforeach()
else()
  option(BUILD_SHARED_LIBS "Build shared libraries (.dll)." ON)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  # for multi-config builds (e.g. msvc)
  foreach(OutputConfig IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER ${OutputConfig} OUTPUTCONFIG)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_BINDIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_BINDIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OutputConfig}/${CMAKE_INSTALL_BINDIR})
  endforeach()
endif()

if(MSVC AND BUILD_SHARED_LIBS)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# Disable CTest targets
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten" AND NOT DEFINED BUILD_TESTING)
  set(BUILD_TESTING OFF CACHE BOOL "Enable testing" FORCE)
endif()
include(CTest)

# By default only build the C++ library which agregate all components.
option(BUILD_CXX "Build C++ library" ON)
message(STATUS "Build C++ library: ${BUILD_CXX}")

# If we don't build ortools we could build the GLOP standalone project
if(NOT BUILD_CXX)
  OPTION(BUILD_GLOP "Build GLOP standalone" ON)
  message(STATUS "Build standalone Glop: ${BUILD_GLOP}")
endif()

option(BUILD_PYTHON "Build Python Library" OFF)
message(STATUS "Build Python: ${BUILD_PYTHON}")

# If wrapper are built, we need to have the install rpath in BINARY_DIR to package
if(BUILD_PYTHON)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

include(CMakeDependentOption)

# Optional components (enabled by default)
## Flatzinc
CMAKE_DEPENDENT_OPTION(BUILD_FLATZINC "Build flatzinc" ON "BUILD_CXX" OFF)
message(STATUS "Build Flatzinc: ${BUILD_FLATZINC}")

## MathOpt
CMAKE_DEPENDENT_OPTION(BUILD_MATH_OPT "Build the MATH_OPT" ON "BUILD_CXX" OFF)
message(STATUS "Build MathOpt: ${BUILD_MATH_OPT}")

## Samples
option(BUILD_SAMPLES "Build samples" ON)
message(STATUS "Build samples: ${BUILD_SAMPLES}")
CMAKE_DEPENDENT_OPTION(BUILD_CXX_SAMPLES "Build cxx samples" ON "BUILD_SAMPLES;BUILD_CXX" OFF)
message(STATUS "Build C++ samples: ${BUILD_CXX_SAMPLES}")
CMAKE_DEPENDENT_OPTION(BUILD_PYTHON_SAMPLES "Build python samples" ON "BUILD_SAMPLES;BUILD_PYTHON" OFF)
message(STATUS "Build Python samples: ${BUILD_PYTHON_SAMPLES}")

## Examples
option(BUILD_EXAMPLES "Build examples" ON)
message(STATUS "Build examples: ${BUILD_EXAMPLES}")
CMAKE_DEPENDENT_OPTION(BUILD_CXX_EXAMPLES "Build cxx examples" ON "BUILD_EXAMPLES;BUILD_CXX" OFF)
message(STATUS "Build C++ examples: ${BUILD_CXX_EXAMPLES}")
CMAKE_DEPENDENT_OPTION(BUILD_PYTHON_EXAMPLES "Build python examples" ON "BUILD_EXAMPLES;BUILD_PYTHON" OFF)
message(STATUS "Build Python examples: ${BUILD_PYTHON_EXAMPLES}")

option(BUILD_DOC "Build documentation" OFF)
message(STATUS "Build documentation: ${BUILD_DOC}")
CMAKE_DEPENDENT_OPTION(INSTALL_DOC "Install doc" ON "BUILD_CXX AND BUILD_DOC" OFF)
message(STATUS "Install doc: ${INSTALL_DOC}")

# WASM JS bindings (optional, Emscripten only)
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  # Embed proto schemas directly into the WASM binary.
  set(EMSCRIPTEN_ALLOWED_ORIGINS "" CACHE STRING
      "Comma-separated list of allowed origins for the WASM runtime. Empty allows all origins.")
  string(REGEX REPLACE "^\"(.*)\"$" "\\1"
    EMSCRIPTEN_ALLOWED_ORIGINS_STRIPPED "${EMSCRIPTEN_ALLOWED_ORIGINS}")
  string(REPLACE "\"" "\\\"" EMSCRIPTEN_ALLOWED_ORIGINS_ESCAPED
    "${EMSCRIPTEN_ALLOWED_ORIGINS_STRIPPED}")
  message(STATUS "Emscripten allowed origins: ${EMSCRIPTEN_ALLOWED_ORIGINS_STRIPPED}")
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/javascript")
  set(PROTO_SCHEMA_HEADER "${PROJECT_BINARY_DIR}/javascript/generated_proto_schemas.h")
  add_custom_command(
    OUTPUT "${PROTO_SCHEMA_HEADER}"
    COMMAND "${CMAKE_COMMAND}"
            -DCP_MODEL_PROTO="${PROJECT_SOURCE_DIR}/ortools/sat/cp_model.proto"
            -DSAT_PARAMETERS_PROTO="${PROJECT_SOURCE_DIR}/ortools/sat/sat_parameters.proto"
            -DTEMPLATE_FILE="${PROJECT_SOURCE_DIR}/javascript/generated_proto_schemas.h.in"
            -DOUTPUT_FILE="${PROTO_SCHEMA_HEADER}"
            -P "${PROJECT_SOURCE_DIR}/scripts/embed_proto.cmake"
    DEPENDS
      "${PROJECT_SOURCE_DIR}/scripts/embed_proto.cmake"
      "${PROJECT_SOURCE_DIR}/javascript/generated_proto_schemas.h.in"
      "${PROJECT_SOURCE_DIR}/ortools/sat/cp_model.proto"
      "${PROJECT_SOURCE_DIR}/ortools/sat/sat_parameters.proto")

  set(JS_OUTPUT_DIR "${PROJECT_BINARY_DIR}/javascript/wasm")
  set(JS_EXAMPLES_DIR "${JS_OUTPUT_DIR}/examples")
  file(MAKE_DIRECTORY "${JS_OUTPUT_DIR}")
  file(MAKE_DIRECTORY "${JS_EXAMPLES_DIR}")

  set(CP_SAT_COMMON_SOURCES
    "${PROJECT_SOURCE_DIR}/javascript/cp_sat_api.cc"
    "${PROTO_SCHEMA_HEADER}")

  set(CP_SAT_COMMON_LINK_OPTIONS
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sEXPORT_NAME=cpSatModule"
    "-sENVIRONMENT=web,worker"
    "-sASSERTIONS=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sUSE_PTHREADS=1"
    "-sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency"
    "-sPTHREAD_POOL_SIZE_STRICT=2"
    "--bind"
    "-sEXPORTED_FUNCTIONS=['_malloc','_free','_solve_model','_free_buffer','_validate_model','_interrupt_solve','_get_cp_model_schema','_get_sat_parameters_schema']"
    "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8,allocateUTF8,HEAPU8"
    "-sNODEJS_CATCH_REJECTION=0"
    "-sNODEJS_CATCH_EXIT=0"
    "-pthread"
    "--shared-memory"
    "--no-entry"
    "-gsource-map"
    "--source-map-base=./")

  set(CP_SAT_JSPI_LINK_OPTIONS
    "-sJSPI=2"
    "-sJSPI_EXPORTS=['solve_model','validate_model']")

  set(CP_SAT_ASYNCIFY_LINK_OPTIONS
    "-sASYNCIFY=1")

  add_executable(cp_sat_runtime
    ${CP_SAT_COMMON_SOURCES})
  target_link_libraries(cp_sat_runtime PRIVATE ${PROJECT_NAMESPACE}::ortools)
  target_include_directories(cp_sat_runtime PRIVATE "${PROJECT_BINARY_DIR}/javascript")
  target_compile_definitions(cp_sat_runtime PRIVATE
    ORTOOLS_WASM_ALLOWED_ORIGINS="${EMSCRIPTEN_ALLOWED_ORIGINS_ESCAPED}")
  set_target_properties(cp_sat_runtime PROPERTIES
    OUTPUT_NAME "cp_sat_runtime"
    RUNTIME_OUTPUT_DIRECTORY "${JS_OUTPUT_DIR}")
  target_link_options(cp_sat_runtime PRIVATE
    ${CP_SAT_COMMON_LINK_OPTIONS}
    ${CP_SAT_JSPI_LINK_OPTIONS}
    "--emit-tsd"
    "${JS_OUTPUT_DIR}/cp_sat_runtime.d.ts")

  add_executable(cp_sat_runtime_asyncify
    ${CP_SAT_COMMON_SOURCES})
  target_link_libraries(cp_sat_runtime_asyncify PRIVATE ${PROJECT_NAMESPACE}::ortools)
  target_include_directories(cp_sat_runtime_asyncify PRIVATE "${PROJECT_BINARY_DIR}/javascript")
  target_compile_definitions(cp_sat_runtime_asyncify PRIVATE
    ORTOOLS_WASM_ALLOWED_ORIGINS="${EMSCRIPTEN_ALLOWED_ORIGINS_ESCAPED}")
  set_target_properties(cp_sat_runtime_asyncify PROPERTIES
    OUTPUT_NAME "cp_sat_runtime_asyncify"
    RUNTIME_OUTPUT_DIRECTORY "${JS_OUTPUT_DIR}")
  target_link_options(cp_sat_runtime_asyncify PRIVATE
    ${CP_SAT_COMMON_LINK_OPTIONS}
    ${CP_SAT_ASYNCIFY_LINK_OPTIONS})

  add_custom_target(cp_sat_runtime_site ALL
    DEPENDS cp_sat_runtime cp_sat_runtime_asyncify
    COMMENT "Building cp_sat_runtime runtimes")
endif()

# By default all dependencies are NOT built (i.e. BUILD_DEPS=OFF),
# BUT if building the Python wrapper then BUILD_DEPS=ON.
if(BUILD_PYTHON)
  option(BUILD_DEPS "Build all dependencies" ON)
else()
  option(BUILD_DEPS "Build all dependencies" OFF)
endif()
message(STATUS "Build all dependencies: ${BUILD_DEPS}")
# Install built dependencies if any,
option(INSTALL_BUILD_DEPS "Install build all dependencies" ON)

# IF BUILD_DEPS=ON THEN Force all BUILD_*=ON
CMAKE_DEPENDENT_OPTION(BUILD_ZLIB "Build the ZLIB dependency Library" ON
  "NOT BUILD_DEPS" ON)

CMAKE_DEPENDENT_OPTION(BUILD_BZip2 "Build the BZip2 dependency Library" ON
  "NOT BUILD_DEPS" ON)

set(_ORTOOLS_PATCH_DIR "${CMAKE_CURRENT_LIST_DIR}/patches")
if(BUILD_ZLIB AND NOT EXISTS "${_ORTOOLS_PATCH_DIR}/ZLIB-v1.3.1.patch")
  message(WARNING "Disabling BUILD_ZLIB (missing patches/ZLIB-v1.3.1.patch)")
  set(BUILD_ZLIB OFF CACHE BOOL "Build the ZLIB dependency Library" FORCE)
endif()
message(STATUS "Build ZLIB: ${BUILD_ZLIB}")

if(BUILD_BZip2 AND NOT EXISTS "${_ORTOOLS_PATCH_DIR}/bzip2.patch")
  message(WARNING "Disabling BUILD_BZip2 (missing patches/bzip2.patch)")
  set(BUILD_BZip2 OFF CACHE BOOL "Build the BZip2 dependency Library" FORCE)
endif()
message(STATUS "Build BZip2: ${BUILD_BZip2}")

CMAKE_DEPENDENT_OPTION(BUILD_absl "Build the abseil-cpp dependency Library" ON
  "NOT BUILD_DEPS" ON)
message(STATUS "Build abseil-cpp: ${BUILD_absl}")

CMAKE_DEPENDENT_OPTION(BUILD_Protobuf "Build the Protobuf dependency Library" ON
  "NOT BUILD_DEPS" ON)
message(STATUS "Build protobuf: ${BUILD_Protobuf}")

CMAKE_DEPENDENT_OPTION(BUILD_Eigen3 "Build the eigen3 dependency Library" ON
  "NOT BUILD_DEPS" ON)
message(STATUS "Build eigen3: ${BUILD_Eigen3}")

CMAKE_DEPENDENT_OPTION(BUILD_re2 "Build the re2 dependency Library" ON
  "NOT BUILD_DEPS" ON)
message(STATUS "Build re2: ${BUILD_re2}")

if(BUILD_TESTING)
  CMAKE_DEPENDENT_OPTION(BUILD_googletest "Build googletest" OFF
    "NOT BUILD_DEPS" ON)
  CMAKE_DEPENDENT_OPTION(BUILD_benchmark "Build benchmark" OFF
    "NOT BUILD_DEPS" ON)
  # Fuzztest do not support MSVC or toolchain
  if(MSVC OR CMAKE_CROSSCOMPILING)
    set(USE_fuzztest OFF)
  else()
    CMAKE_DEPENDENT_OPTION(USE_fuzztest "Enable fuzztest" ON "BUILD_CXX" OFF)
  endif()
  if(NOT USE_fuzztest)
    set(BUILD_fuzztest OFF)
  else()
    CMAKE_DEPENDENT_OPTION(BUILD_fuzztest "Build fuzztest" OFF
      "NOT BUILD_DEPS" ON)
  endif()
else()
  set(BUILD_googletest OFF)
  set(BUILD_benchmark OFF)
  set(USE_fuzztest OFF)
  set(BUILD_fuzztest OFF)
endif()
message(STATUS "Build googletest: ${BUILD_googletest}")
message(STATUS "Build benchmark: ${BUILD_benchmark}")
message(STATUS "Enable fuzztest: ${USE_fuzztest}")
message(STATUS "Build fuzztest: ${BUILD_fuzztest}")

# Optional third party solvers (enabled by default)
## BOP
# note OFF is currently not supported.
CMAKE_DEPENDENT_OPTION(USE_BOP "Use the BOP solver" ON "BUILD_CXX" OFF)
message(STATUS "BOP support: ${USE_BOP}")

## COIN-OR Solvers (Cbc, Clp)
CMAKE_DEPENDENT_OPTION(USE_COINOR "Use the COIN-OR solver" OFF "BUILD_CXX" OFF)
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
else()
  set(BUILD_CoinUtils OFF)
  set(BUILD_Osi OFF)
  set(BUILD_Clp OFF)
  set(BUILD_Cgl OFF)
  set(BUILD_Cbc OFF)
endif()

## GLOP
# note OFF is currently not supported.
CMAKE_DEPENDENT_OPTION(USE_GLOP "Use the GLOP solver" ON "BUILD_CXX" OFF)
message(STATUS "GLOP support: ${USE_GLOP}")

## GLPK
# Disable by default since it is GPLv3, user could enable it and release under GPLv3
# see: https://www.apache.org/licenses/GPL-compatibility.html
CMAKE_DEPENDENT_OPTION(USE_GLPK "Use the GLPK solver" OFF "BUILD_CXX" OFF)
message(STATUS "GLPK support: ${USE_GLPK}")
if(USE_GLPK)
  CMAKE_DEPENDENT_OPTION(BUILD_GLPK "Build the GLPK dependency Library" OFF
    "NOT BUILD_DEPS" ON)
else()
  set(BUILD_GLPK OFF)
endif()
message(STATUS "Build GLPK: ${BUILD_GLPK}")

## GUROBI
# Since it is dynamicaly loaded upon use, OFF is currently not supported.
CMAKE_DEPENDENT_OPTION(USE_GUROBI "Use the Gurobi solver" ON "BUILD_CXX" OFF)
message(STATUS "Gurobi support: ${USE_GUROBI}")

## HiGHS
# see: https://github.com/ERGO-Code/HiGHS
CMAKE_DEPENDENT_OPTION(USE_HIGHS "Use the HiGHS solver" OFF "BUILD_CXX" OFF)
message(STATUS "HiGHS support: ${USE_HIGHS}")
if(USE_HIGHS)
  CMAKE_DEPENDENT_OPTION(BUILD_HIGHS "Build the HiGHS dependency Library" OFF
    "NOT BUILD_DEPS" ON)
else()
  set(BUILD_HIGHS OFF)
endif()
message(STATUS "Build HiGHS: ${BUILD_HIGHS}")

## PDLP
CMAKE_DEPENDENT_OPTION(USE_PDLP "Use the PDLP solver" ON "BUILD_CXX" OFF)
message(STATUS "PDLP support: ${USE_PDLP}")
if(USE_PDLP)
  CMAKE_DEPENDENT_OPTION(BUILD_PDLP "Build the PDLP dependency Library" ON
    "NOT BUILD_DEPS" ON)
else()
  set(BUILD_PDLP OFF)
endif()
message(STATUS "Build PDLP: ${BUILD_PDLP}")

## SCIP
# see: https://github.com/scipopt/scip
CMAKE_DEPENDENT_OPTION(USE_SCIP "Use the Scip solver" OFF "BUILD_CXX" OFF)
message(STATUS "SCIP support: ${USE_SCIP}")
if(USE_SCIP)
  CMAKE_DEPENDENT_OPTION(BUILD_Boost "Build the Boost dependency Library" OFF
    "NOT BUILD_DEPS" ON)
  CMAKE_DEPENDENT_OPTION(BUILD_soplex "Build the Soplex dependency Library" OFF
    "NOT BUILD_DEPS" ON)
  CMAKE_DEPENDENT_OPTION(BUILD_SCIP "Build the SCIP dependency Library" OFF
    "NOT BUILD_DEPS" ON)
else()
  set(BUILD_Boost OFF)
  set(BUILD_soplex OFF)
  set(BUILD_SCIP OFF)
endif()
message(STATUS "Build Boost (Soplex requirement): ${BUILD_Boost}")
message(STATUS "Build Soplex (SCIP requirement): ${BUILD_soplex}")
message(STATUS "Build SCIP: ${BUILD_SCIP}")

# Optional third party solvers (disabled by default)
## CPLEX
option(USE_CPLEX "Use the CPLEX solver" OFF)
message(STATUS "CPLEX support: ${USE_CPLEX}")

## XPRESS
# Since it is dynamicaly loaded upon use, OFF is currently not supported.
CMAKE_DEPENDENT_OPTION(USE_XPRESS "Use the Xpress solver" ON "BUILD_CXX" OFF)
message(STATUS "Xpress support: ${USE_XPRESS}")

# Language specific options
if(BUILD_CXX)
  CMAKE_DEPENDENT_OPTION(BUILD_CXX_DOC "Build the C++ doc" OFF "NOT BUILD_DOC" ON)
  message(STATUS "C++: Build doc: ${BUILD_CXX_DOC}")
endif()

if(BUILD_PYTHON)
  CMAKE_DEPENDENT_OPTION(BUILD_pybind11 "Build the pybind11 dependency Library" OFF
    "NOT BUILD_DEPS" ON)
  message(STATUS "Python: Build pybind11: ${BUILD_pybind11}")

  CMAKE_DEPENDENT_OPTION(BUILD_pybind11_abseil "Build the pybind11_abseil dependency Library" OFF
    "NOT BUILD_DEPS" ON)
  message(STATUS "Python: Build pybind11_abseil: ${BUILD_pybind11_abseil}")

  CMAKE_DEPENDENT_OPTION(BUILD_pybind11_protobuf "Build the pybind11_protobuf dependency Library" OFF
    "NOT BUILD_DEPS" ON)
  message(STATUS "Python: Build pybind11_protobuf: ${BUILD_pybind11_protobuf}")

  option(GENERATE_PYTHON_STUB "Generate Python stub file (.pyi)" ON)
  message(STATUS "Python: Generate stub file: ${GENERATE_PYTHON_STUB}")

CMAKE_DEPENDENT_OPTION(BUILD_PYTHON_DOC "Build the Python doc" OFF "NOT BUILD_DOC" ON)
  message(STATUS "Python: Build doc: ${BUILD_PYTHON_DOC}")

  CMAKE_DEPENDENT_OPTION(BUILD_VENV "Create Python venv in BINARY_DIR/python/venv" OFF
    "NOT BUILD_TESTING" ON)
  message(STATUS "Python: Create venv: ${BUILD_VENV}")

  CMAKE_DEPENDENT_OPTION(VENV_USE_SYSTEM_SITE_PACKAGES "Python venv can use system site packages" OFF
    "BUILD_VENV" OFF)
  message(STATUS "Python: Allow venv to use system site packages: ${VENV_USE_SYSTEM_SITE_PACKAGES}")

  option(FETCH_PYTHON_DEPS "Install python required modules if not available" ${BUILD_DEPS})
  message(STATUS "Python: Fetch dependencies: ${FETCH_PYTHON_DEPS}")
endif()

# Find system deps
include(system_deps)

# Build Needed dependencies
add_subdirectory(cmake/dependencies dependencies)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_BINARY_DIR}/dependencies/install)

include(host)
# verify deps
include(check_deps)

include(cpp)
include(flatzinc)
include(glop)

include(python)

# Since samples mix all languages we must parse them once we have included all
# <language>.cmake files
foreach(SAMPLES IN ITEMS
 glop
 ${MATH_OPT_DIR}
 ${PDLP_DIR}
 sat)
  add_subdirectory(ortools/${SAMPLES}/samples)
endforeach()
