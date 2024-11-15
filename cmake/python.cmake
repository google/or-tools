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

if(NOT BUILD_PYTHON)
  return()
endif()

# Use latest UseSWIG module (3.14) and Python3 module (3.18)
cmake_minimum_required(VERSION 3.18)

if(NOT TARGET ${PROJECT_NAMESPACE}::ortools)
  message(FATAL_ERROR "Python: missing ortools TARGET")
endif()

# Will need swig
set(CMAKE_SWIG_FLAGS)
find_package(SWIG REQUIRED)
include(UseSWIG)

if(${SWIG_VERSION} VERSION_GREATER_EQUAL 4)
  list(APPEND CMAKE_SWIG_FLAGS "-doxygen")
endif()

if(UNIX AND NOT APPLE AND NOT (CMAKE_SYSTEM_NAME STREQUAL "OpenBSD"))
  if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    list(APPEND CMAKE_SWIG_FLAGS "-DSWIGWORDSIZE64")
  else()
    list(APPEND CMAKE_SWIG_FLAGS "-DSWIGWORDSIZE32")
  endif()
endif()

# Find Python 3
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
list(APPEND CMAKE_SWIG_FLAGS "-py3" "-DPY3")

# Find if the python module is available,
# otherwise install it (PACKAGE_NAME) to the Python3 user install directory.
# If CMake option FETCH_PYTHON_DEPS is OFF then issue a fatal error instead.
# e.g
# search_python_module(
#   NAME
#     mypy_protobuf
#   PACKAGE
#     mypy-protobuf
#   NO_VERSION
# )
function(search_python_module)
  set(options NO_VERSION)
  set(oneValueArgs NAME PACKAGE)
  set(multiValueArgs "")
  cmake_parse_arguments(MODULE
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
  message(STATUS "Searching python module: \"${MODULE_NAME}\"")
  if(${MODULE_NO_VERSION})
    execute_process(
      COMMAND ${Python3_EXECUTABLE} -c "import ${MODULE_NAME}"
      RESULT_VARIABLE _RESULT
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(MODULE_VERSION "unknown")
  else()
    execute_process(
      COMMAND ${Python3_EXECUTABLE} -c "import ${MODULE_NAME}; print(${MODULE_NAME}.__version__)"
      RESULT_VARIABLE _RESULT
      OUTPUT_VARIABLE MODULE_VERSION
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  endif()
  if(${_RESULT} STREQUAL "0")
    message(STATUS "Found python module: \"${MODULE_NAME}\" (found version \"${MODULE_VERSION}\")")
  else()
    if(FETCH_PYTHON_DEPS)
      message(WARNING "Can't find python module: \"${MODULE_NAME}\", install it using pip...")
      execute_process(
        COMMAND ${Python3_EXECUTABLE} -m pip install --user ${MODULE_PACKAGE}
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
      )
    else()
      message(FATAL_ERROR "Can't find python module: \"${MODULE_NAME}\", please install it using your system package manager.")
    endif()
  endif()
endfunction()

# Find if a python builtin module is available.
# e.g
# search_python_internal_module(
#   NAME
#     mypy_protobuf
# )
function(search_python_internal_module)
  set(options "")
  set(oneValueArgs NAME)
  set(multiValueArgs "")
  cmake_parse_arguments(MODULE
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
  message(STATUS "Searching python module: \"${MODULE_NAME}\"")
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import ${MODULE_NAME}"
    RESULT_VARIABLE _RESULT
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  if(${_RESULT} STREQUAL "0")
    message(STATUS "Found python internal module: \"${MODULE_NAME}\"")
  else()
    message(FATAL_ERROR "Can't find python internal module \"${MODULE_NAME}\", please install it using your system package manager.")
  endif()
endfunction()

##################
##  PROTO FILE  ##
##################
# Generate Protobuf py sources with mypy support
search_python_module(
  NAME mypy_protobuf
  PACKAGE mypy-protobuf
  NO_VERSION)
set(PROTO_PYS)
set(PROTO_MYPYS)
set(OR_TOOLS_PROTO_PY_FILES)
file(GLOB_RECURSE OR_TOOLS_PROTO_PY_FILES RELATIVE ${PROJECT_SOURCE_DIR}
  "ortools/algorithms/*.proto"
  "ortools/bop/*.proto"
  "ortools/constraint_solver/*.proto"
  "ortools/glop/*.proto"
  "ortools/graph/*.proto"
  "ortools/linear_solver/*.proto"
  "ortools/packing/*.proto"
  "ortools/sat/*.proto"
  "ortools/scheduling/*.proto"
  "ortools/util/*.proto"
  )
list(REMOVE_ITEM OR_TOOLS_PROTO_PY_FILES "ortools/constraint_solver/demon_profiler.proto")

if(BUILD_MATH_OPT)
  file(GLOB_RECURSE MATH_OPT_PROTO_PY_FILES RELATIVE ${PROJECT_SOURCE_DIR}
    "ortools/math_opt/*.proto"
    "ortools/math_opt/solver/*.proto")
  list(APPEND OR_TOOLS_PROTO_PY_FILES ${MATH_OPT_PROTO_PY_FILES})

  file(GLOB_RECURSE SERVICE_PROTO_PY_FILES RELATIVE ${PROJECT_SOURCE_DIR}
    "ortools/service/v1/*.proto")
  list(APPEND OR_TOOLS_PROTO_PY_FILES ${SERVICE_PROTO_PY_FILES})
endif()

if(USE_PDLP OR BUILD_MATH_OPT)
  file(GLOB_RECURSE PDLP_PROTO_PY_FILES RELATIVE ${PROJECT_SOURCE_DIR} "ortools/pdlp/*.proto")
  list(APPEND OR_TOOLS_PROTO_PY_FILES ${PDLP_PROTO_PY_FILES})
endif()
if(USE_SCIP OR BUILD_MATH_OPT)
  file(GLOB_RECURSE GSCIP_PROTO_PY_FILES RELATIVE ${PROJECT_SOURCE_DIR} "ortools/gscip/*.proto")
  list(APPEND OR_TOOLS_PROTO_PY_FILES ${GSCIP_PROTO_PY_FILES})
endif()

foreach(PROTO_FILE IN LISTS OR_TOOLS_PROTO_PY_FILES)
  #message(STATUS "protoc proto(py): ${PROTO_FILE}")
  get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(PROTO_PY ${PROJECT_BINARY_DIR}/python/${PROTO_DIR}/${PROTO_NAME}_pb2.py)
  set(PROTO_MYPY ${PROJECT_BINARY_DIR}/python/${PROTO_DIR}/${PROTO_NAME}_pb2.pyi)
  #message(STATUS "protoc(py) py: ${PROTO_PY}")
  #message(STATUS "protoc(py) mypy: ${PROTO_MYPY}")
  add_custom_command(
    OUTPUT ${PROTO_PY} ${PROTO_MYPY}
    COMMAND ${PROTOC_PRG}
    "--proto_path=${PROJECT_SOURCE_DIR}"
    ${PROTO_DIRS}
    "--python_out=${PROJECT_BINARY_DIR}/python"
    "--mypy_out=${PROJECT_BINARY_DIR}/python"
    ${PROTO_FILE}
    DEPENDS ${PROTO_FILE} ${PROTOC_PRG}
    COMMENT "Generate Python 3 protocol buffer for ${PROTO_FILE}"
    VERBATIM)
  list(APPEND PROTO_PYS ${PROTO_PY})
  list(APPEND PROTO_MYPYS ${PROTO_MYPY})
endforeach()
add_custom_target(Py${PROJECT_NAME}_proto
  DEPENDS
    ${PROTO_PYS}
    ${PROTO_MYPYS}
    ${PROJECT_NAMESPACE}::ortools)

###################
##  Python Test  ##
###################
if(BUILD_VENV)
  search_python_module(NAME virtualenv PACKAGE virtualenv)
  # venv not working on github runners
  # search_python_internal_module(NAME venv)
  # Testing using a vitual environment
  set(VENV_EXECUTABLE ${Python3_EXECUTABLE} -m virtualenv)
  #set(VENV_EXECUTABLE ${Python3_EXECUTABLE} -m venv)
  set(VENV_DIR ${CMAKE_CURRENT_BINARY_DIR}/python/venv)
  if(WIN32)
    set(VENV_Python3_EXECUTABLE ${VENV_DIR}/Scripts/python.exe)
  else()
    set(VENV_Python3_EXECUTABLE ${VENV_DIR}/bin/python)
  endif()
endif()

# add_python_test()
# CMake function to generate and build python test.
# Parameters:
#  FILE_NAME: the python filename
#  COMPONENT_NAME: name of the ortools/ subdir where the test is located
#  note: automatically determined if located in ortools/<component>/python/
# e.g.:
# add_python_test(
#   FILE_NAME
#     ${PROJECT_SOURCE_DIR}/ortools/foo/python/bar_test.py
#   COMPONENT_NAME
#     foo
# )
function(add_python_test)
  set(options "")
  set(oneValueArgs FILE_NAME COMPONENT_NAME)
  set(multiValueArgs "")
  cmake_parse_arguments(TEST
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
  if(NOT TEST_FILE_NAME)
    message(FATAL_ERROR "no FILE_NAME provided")
  endif()
  get_filename_component(TEST_NAME ${TEST_FILE_NAME} NAME_WE)

  message(STATUS "Configuring test ${TEST_FILE_NAME} ...")

  if(NOT TEST_COMPONENT_NAME)
    # test is located in ortools/<component_name>/python/
    get_filename_component(WRAPPER_DIR ${TEST_FILE_NAME} DIRECTORY)
    get_filename_component(COMPONENT_DIR ${WRAPPER_DIR} DIRECTORY)
    get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)
  else()
    set(COMPONENT_NAME ${TEST_COMPONENT_NAME})
  endif()

  if(BUILD_TESTING)
    add_test(
      NAME python_${COMPONENT_NAME}_${TEST_NAME}
      COMMAND ${VENV_Python3_EXECUTABLE} -m pytest ${TEST_FILE_NAME}
      WORKING_DIRECTORY ${VENV_DIR})
  endif()
  message(STATUS "Configuring test ${TEST_FILE_NAME} ...DONE")
endfunction()

#######################
##  PYTHON WRAPPERS  ##
#######################
list(APPEND CMAKE_SWIG_FLAGS "-I${PROJECT_SOURCE_DIR}")

set(PYTHON_PROJECT ${PROJECT_NAME})
message(STATUS "Python project: ${PYTHON_PROJECT}")
set(PYTHON_PROJECT_DIR ${PROJECT_BINARY_DIR}/python/${PYTHON_PROJECT})
message(STATUS "Python project build path: ${PYTHON_PROJECT_DIR}")

# SWIG/Pybind11 wrap all libraries
foreach(SUBPROJECT IN ITEMS
 init
 algorithms
 graph
 linear_solver
 ${PDLP_DIR}
 constraint_solver
 sat
 scheduling
 util)
  add_subdirectory(ortools/${SUBPROJECT}/python)
endforeach()

if(BUILD_MATH_OPT)
  add_subdirectory(ortools/math_opt/core/python)
  add_subdirectory(ortools/math_opt/python)
endif()

#######################
## Python Packaging  ##
#######################
#file(MAKE_DIRECTORY python/${PYTHON_PROJECT})
configure_file(
  ${PROJECT_SOURCE_DIR}/ortools/python/__init__.py.in
  ${PROJECT_BINARY_DIR}/python/__init__.py.in
  @ONLY)
file(GENERATE
  OUTPUT ${PYTHON_PROJECT_DIR}/__init__.py
  INPUT ${PROJECT_BINARY_DIR}/python/__init__.py.in)

file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/algorithms/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/algorithms/python/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/bop/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/constraint_solver/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/glop/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/graph/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/graph/python/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/gscip/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/init/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/init/python/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/linear_solver/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/linear_solver/python/__init__.py CONTENT "")
if(BUILD_MATH_OPT)
  file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/../pybind11_abseil/__init__.py CONTENT "")
  file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/math_opt/__init__.py CONTENT "")
  file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/math_opt/core/__init__.py CONTENT "")
  file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/math_opt/core/python/__init__.py CONTENT "")
  file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/math_opt/python/__init__.py CONTENT "")
  file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/math_opt/python/ipc/__init__.py CONTENT "")
  file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/math_opt/python/testing/__init__.py CONTENT "")
  file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/math_opt/solvers/__init__.py CONTENT "")

  file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/service/__init__.py CONTENT "")
  file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/service/v1/__init__.py CONTENT "")
  file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/service/v1/mathopt/__init__.py CONTENT "")
endif()
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/packing/__init__.py CONTENT "")
if(USE_PDLP OR BUILD_MATH_OPT)
  file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/pdlp/__init__.py CONTENT "")
  file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/pdlp/python/__init__.py CONTENT "")
endif()
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/sat/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/sat/python/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/sat/colab/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/scheduling/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/scheduling/python/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/util/__init__.py CONTENT "")
file(GENERATE OUTPUT ${PYTHON_PROJECT_DIR}/util/python/__init__.py CONTENT "")

file(COPY
  ortools/linear_solver/python/linear_solver_natural_api.py
  DESTINATION ${PYTHON_PROJECT_DIR}/linear_solver/python)
file(COPY
  ortools/linear_solver/python/model_builder.py
  ortools/linear_solver/python/model_builder_numbers.py
  DESTINATION ${PYTHON_PROJECT_DIR}/linear_solver/python)
if(BUILD_MATH_OPT)
  file(COPY
    ortools/math_opt/python/callback.py
    ortools/math_opt/python/compute_infeasible_subsystem_result.py
    ortools/math_opt/python/errors.py
    ortools/math_opt/python/expressions.py
    ortools/math_opt/python/hash_model_storage.py
    ortools/math_opt/python/init_arguments.py
    ortools/math_opt/python/mathopt.py
    ortools/math_opt/python/message_callback.py
    ortools/math_opt/python/model.py
    ortools/math_opt/python/model_parameters.py
    ortools/math_opt/python/model_storage.py
    ortools/math_opt/python/normalize.py
    ortools/math_opt/python/parameters.py
    ortools/math_opt/python/result.py
    ortools/math_opt/python/solution.py
    ortools/math_opt/python/solve.py
    ortools/math_opt/python/solver_resources.py
    ortools/math_opt/python/sparse_containers.py
    ortools/math_opt/python/statistics.py
    DESTINATION ${PYTHON_PROJECT_DIR}/math_opt/python)
  file(COPY
    ortools/math_opt/python/ipc/proto_converter.py
    ortools/math_opt/python/ipc/remote_http_solve.py
    DESTINATION ${PYTHON_PROJECT_DIR}/math_opt/python/ipc)
  file(COPY
    ortools/math_opt/python/testing/compare_proto.py
    ortools/math_opt/python/testing/proto_matcher.py
    DESTINATION ${PYTHON_PROJECT_DIR}/math_opt/python/testing)
endif()
file(COPY
  ortools/sat/python/cp_model.py
  ortools/sat/python/cp_model_helper.py
  DESTINATION ${PYTHON_PROJECT_DIR}/sat/python)
file(COPY
  ortools/sat/colab/flags.py
  ortools/sat/colab/visualization.py
  DESTINATION ${PYTHON_PROJECT_DIR}/sat/colab)

# Adds py.typed to make typed packages.
file(TOUCH ${PYTHON_PROJECT_DIR}/linear_solver/python/py.typed)
file(TOUCH ${PYTHON_PROJECT_DIR}/sat/py.typed)
file(TOUCH ${PYTHON_PROJECT_DIR}/sat/python/py.typed)

# setup.py.in contains cmake variable e.g. @PYTHON_PROJECT@ and
# generator expression e.g. $<TARGET_FILE_NAME:pyFoo>
configure_file(
  ${PROJECT_SOURCE_DIR}/ortools/python/setup.py.in
  ${PROJECT_BINARY_DIR}/python/setup.py.in
  @ONLY)
file(GENERATE
  OUTPUT ${PROJECT_BINARY_DIR}/python/setup.py
  INPUT ${PROJECT_BINARY_DIR}/python/setup.py.in)

#add_custom_command(
#  OUTPUT python/setup.py
#  DEPENDS ${PROJECT_BINARY_DIR}/python/setup.py
#  COMMAND ${CMAKE_COMMAND} -E copy setup.py setup.py
#  WORKING_DIRECTORY python)

configure_file(
  ${PROJECT_SOURCE_DIR}/ortools/python/README.pypi.txt
  ${PROJECT_BINARY_DIR}/python/README.txt
  COPYONLY)

configure_file(
  ${PROJECT_SOURCE_DIR}/LICENSE
  ${PROJECT_BINARY_DIR}/python/LICENSE
  COPYONLY)

set(is_windows "$<PLATFORM_ID:Windows>")
set(is_not_windows "$<NOT:$<PLATFORM_ID:Windows>>")

set(need_unix_zlib_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_ZLIB}>>")
set(need_windows_zlib_lib "$<AND:${is_windows},$<BOOL:${BUILD_ZLIB}>>")

set(need_unix_absl_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_absl}>>")
set(need_windows_absl_lib "$<AND:${is_windows},$<BOOL:${BUILD_absl}>>")

set(need_unix_re2_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_re2}>>")
set(need_windows_re2_lib "$<AND:${is_windows},$<BOOL:${BUILD_re2}>>")

set(need_unix_protobuf_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_Protobuf}>>")
set(need_windows_protobuf_lib "$<AND:${is_windows},$<BOOL:${BUILD_Protobuf}>>")

set(need_unix_coinutils_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_CoinUtils}>>")
set(need_unix_osi_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_Osi}>>")
set(need_unix_clp_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_Clp}>>")
set(need_unix_cgl_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_Cgl}>>")
set(need_unix_cbc_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_Cbc}>>")

set(need_unix_highs_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_HIGHS}>>")
set(need_windows_highs_lib "$<AND:${is_windows},$<BOOL:${BUILD_HIGHS}>>")

set(is_ortools_shared "$<STREQUAL:$<TARGET_PROPERTY:ortools,TYPE>,SHARED_LIBRARY>")

add_custom_command(
  OUTPUT python/ortools_timestamp
  COMMAND ${CMAKE_COMMAND} -E remove -f ortools_timestamp
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PYTHON_PROJECT}/.libs
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:$<BOOL:${BUILD_ZLIB}>,copy,true>
    $<${need_unix_zlib_lib}:$<TARGET_SONAME_FILE:ZLIB::ZLIB>>
    $<${need_windows_zlib_lib}:$<TARGET_FILE:ZLIB::ZLIB>>
    ${PYTHON_PROJECT}/.libs
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:$<BOOL:${BUILD_absl}>,copy,true>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::base>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::bad_any_cast_impl>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::bad_optional_access>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::bad_variant_access>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::city>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::civil_time>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::cord>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::cord_internal>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::cordz_functions>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::cordz_handle>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::cordz_info>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::crc32c>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::crc_cord_state>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::crc_cpu_detect>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::crc_internal>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::debugging_internal>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::decode_rust_punycode>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::demangle_internal>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::demangle_rust>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::die_if_null>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::examine_stack>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::exponential_biased>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_commandlineflag>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_commandlineflag_internal>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_config>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_internal>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_marshalling>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_parse>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_private_handle_accessor>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_program_name>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_reflection>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_usage>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_usage_internal>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::graphcycles_internal>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::hash>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::hashtablez_sampler>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::int128>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::kernel_timeout_internal>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::leak_check>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::log_entry>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::log_flags>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::log_globals>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::log_initialize>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_check_op>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_conditions>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_fnmatch>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_format>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_globals>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_log_sink_set>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_message>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_nullguard>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_proto>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::log_severity>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::log_sink>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::low_level_hash>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::malloc_internal>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::random_distributions>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::random_internal_platform>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::random_internal_pool_urbg>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::random_internal_randen>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::random_internal_randen_hwaes>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::random_internal_randen_hwaes_impl>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::random_internal_randen_slow>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::random_internal_seed_material>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::random_seed_gen_exception>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::random_seed_sequences>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::raw_hash_set>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::raw_logging_internal>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::spinlock_wait>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::stacktrace>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::status>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::statusor>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::str_format_internal>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::strerror>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::string_view>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::strings>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::strings_internal>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::symbolize>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::synchronization>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::throw_delegate>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::time>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::time_zone>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::utf8_for_code_point>>
    $<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:absl::vlog_config_internal>>
    $<${need_windows_absl_lib}:$<TARGET_FILE:absl::abseil_dll>>
    ${PYTHON_PROJECT}/.libs

  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_unix_re2_lib},copy,true>
    $<${need_unix_re2_lib}:$<TARGET_SONAME_FILE:re2::re2>>
    ${PYTHON_PROJECT}/.libs

  COMMAND ${CMAKE_COMMAND} -E
    $<IF:$<BOOL:${BUILD_Protobuf}>,copy,true>
    $<${need_unix_protobuf_lib}:$<TARGET_SONAME_FILE:protobuf::libprotobuf>>
    $<${need_unix_protobuf_lib}:$<TARGET_SONAME_FILE:utf8_validity>>
    $<${need_windows_protobuf_lib}:$<TARGET_FILE:protobuf::libprotobuf>>
    $<${need_windows_protobuf_lib}:$<TARGET_FILE:utf8_validity>>
    ${PYTHON_PROJECT}/.libs

  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_unix_coinutils_lib},copy,true>
    $<${need_unix_coinutils_lib}:$<TARGET_SONAME_FILE:Coin::CoinUtils>>
    ${PYTHON_PROJECT}/.libs
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_unix_osi_lib},copy,true>
    $<${need_unix_osi_lib}:$<TARGET_SONAME_FILE:Coin::Osi>>
    ${PYTHON_PROJECT}/.libs
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_unix_clp_lib},copy,true>
    $<${need_unix_clp_lib}:$<TARGET_SONAME_FILE:Coin::Clp>>
    $<${need_unix_clp_lib}:$<TARGET_SONAME_FILE:Coin::OsiClp>>
    $<${need_unix_clp_lib}:$<TARGET_SONAME_FILE:Coin::ClpSolver>>
    ${PYTHON_PROJECT}/.libs
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_unix_cgl_lib},copy,true>
    $<${need_unix_cgl_lib}:$<TARGET_SONAME_FILE:Coin::Cgl>>
    ${PYTHON_PROJECT}/.libs
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_unix_cbc_lib},copy,true>
    $<${need_unix_cbc_lib}:$<TARGET_SONAME_FILE:Coin::Cbc>>
    $<${need_unix_cbc_lib}:$<TARGET_SONAME_FILE:Coin::OsiCbc>>
    $<${need_unix_cbc_lib}:$<TARGET_SONAME_FILE:Coin::CbcSolver>>
    ${PYTHON_PROJECT}/.libs

  COMMAND ${CMAKE_COMMAND} -E
    $<IF:$<BOOL:${BUILD_HIGHS}>,copy,true>
    $<${need_unix_highs_lib}:$<TARGET_SONAME_FILE:highs>>
    $<${need_windows_highs_lib}:$<TARGET_FILE:highs>>
    ${PYTHON_PROJECT}/.libs

  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${is_ortools_shared},copy,true>
    $<${is_ortools_shared}:$<TARGET_SONAME_FILE:ortools>>
    ${PYTHON_PROJECT}/.libs
  COMMAND ${CMAKE_COMMAND} -E touch ${PROJECT_BINARY_DIR}/python/ortools_timestamp
  MAIN_DEPENDENCY
    ortools/python/setup.py.in
  DEPENDS
    Py${PROJECT_NAME}_proto
    ${PROJECT_NAMESPACE}::ortools
  WORKING_DIRECTORY python
  COMMAND_EXPAND_LISTS)

add_custom_command(
  OUTPUT python/pybind11_timestamp
  COMMAND ${CMAKE_COMMAND} -E remove -f pybind11_timestamp
  COMMAND ${CMAKE_COMMAND} -E copy
   $<TARGET_FILE:init_pybind11> ${PYTHON_PROJECT}/init/python
  COMMAND ${CMAKE_COMMAND} -E copy
   $<TARGET_FILE:knapsack_solver_pybind11> ${PYTHON_PROJECT}/algorithms/python
  COMMAND ${CMAKE_COMMAND} -E copy
   $<TARGET_FILE:set_cover_pybind11> ${PYTHON_PROJECT}/algorithms/python
  COMMAND ${CMAKE_COMMAND} -E copy
   $<TARGET_FILE:linear_sum_assignment_pybind11> ${PYTHON_PROJECT}/graph/python
  COMMAND ${CMAKE_COMMAND} -E copy
   $<TARGET_FILE:max_flow_pybind11> ${PYTHON_PROJECT}/graph/python
  COMMAND ${CMAKE_COMMAND} -E copy
   $<TARGET_FILE:min_cost_flow_pybind11> ${PYTHON_PROJECT}/graph/python
  COMMAND ${CMAKE_COMMAND} -E copy
   $<TARGET_FILE:pywrapcp> ${PYTHON_PROJECT}/constraint_solver
  COMMAND ${CMAKE_COMMAND} -E copy
   $<TARGET_FILE:pywraplp> ${PYTHON_PROJECT}/linear_solver
  COMMAND ${CMAKE_COMMAND} -E copy
   $<TARGET_FILE:model_builder_helper_pybind11> ${PYTHON_PROJECT}/linear_solver/python
  COMMAND ${CMAKE_COMMAND} -E
   $<IF:$<BOOL:${BUILD_MATH_OPT}>,copy,true>
   $<TARGET_FILE:math_opt_pybind11> ${PYTHON_PROJECT}/math_opt/core/python
  COMMAND ${CMAKE_COMMAND} -E
   $<IF:$<BOOL:${BUILD_MATH_OPT}>,copy,true>
   $<TARGET_FILE:status_py_extension_stub> ${PYTHON_PROJECT}/../pybind11_abseil
  COMMAND ${CMAKE_COMMAND} -E
   $<IF:$<TARGET_EXISTS:pdlp_pybind11>,copy,true>
   $<$<TARGET_EXISTS:pdlp_pybind11>:$<TARGET_FILE:pdlp_pybind11>> ${PYTHON_PROJECT}/pdlp/python
  COMMAND ${CMAKE_COMMAND} -E copy
   $<TARGET_FILE:swig_helper_pybind11> ${PYTHON_PROJECT}/sat/python
  COMMAND ${CMAKE_COMMAND} -E copy
   $<TARGET_FILE:rcpsp_pybind11> ${PYTHON_PROJECT}/scheduling/python
  COMMAND ${CMAKE_COMMAND} -E copy
   $<TARGET_FILE:sorted_interval_list_pybind11> ${PYTHON_PROJECT}/util/python
  COMMAND ${CMAKE_COMMAND} -E touch ${PROJECT_BINARY_DIR}/python/pybind11_timestamp
  MAIN_DEPENDENCY
    ortools/python/setup.py.in
  DEPENDS
    init_pybind11
    knapsack_solver_pybind11
    set_cover_pybind11
    linear_sum_assignment_pybind11
    max_flow_pybind11
    min_cost_flow_pybind11
    pywrapcp
    pywraplp
    model_builder_helper_pybind11
    math_opt_pybind11
    $<TARGET_NAME_IF_EXISTS:pdlp_pybind11>
    swig_helper_pybind11
    rcpsp_pybind11
    sorted_interval_list_pybind11
  WORKING_DIRECTORY python
  COMMAND_EXPAND_LISTS)


# Generate Stub
if(GENERATE_PYTHON_STUB)
# Look for required python modules
search_python_module(
  NAME mypy
  PACKAGE mypy
  NO_VERSION)

find_program(
  stubgen_EXECUTABLE
  NAMES stubgen stubgen.exe
  REQUIRED
)
message(STATUS "Python: stubgen: ${stubgen_EXECUTABLE}")

add_custom_command(
  OUTPUT python/stub_timestamp
  COMMAND ${CMAKE_COMMAND} -E remove -f stub_timestamp
  COMMAND ${stubgen_EXECUTABLE} -p ortools.init.python.init --output .
  COMMAND ${stubgen_EXECUTABLE} -p ortools.algorithms.python.knapsack_solver --output .
  COMMAND ${stubgen_EXECUTABLE} -p ortools.algorithms.python.set_cover --output .
  COMMAND ${stubgen_EXECUTABLE} -p ortools.graph.python.linear_sum_assignment --output .
  COMMAND ${stubgen_EXECUTABLE} -p ortools.graph.python.max_flow --output .
  COMMAND ${stubgen_EXECUTABLE} -p ortools.graph.python.min_cost_flow --output .
  COMMAND ${stubgen_EXECUTABLE} -p ortools.constraint_solver.pywrapcp --output .
  COMMAND ${stubgen_EXECUTABLE} -p ortools.linear_solver.pywraplp --output .
  COMMAND ${stubgen_EXECUTABLE} -p ortools.linear_solver.python.model_builder_helper --output .
  COMMAND ${stubgen_EXECUTABLE} -p pybind11_abseil.status --output .
  COMMAND ${stubgen_EXECUTABLE} -p ortools.math_opt.core.python.solver --output .
  COMMAND ${stubgen_EXECUTABLE} -p ortools.pdlp.python.pdlp --output .
  COMMAND ${stubgen_EXECUTABLE} -p ortools.sat.python.swig_helper --output .
  COMMAND ${stubgen_EXECUTABLE} -p ortools.scheduling.python.rcpsp --output .
  COMMAND ${stubgen_EXECUTABLE} -p ortools.util.python.sorted_interval_list --output .
  COMMAND ${CMAKE_COMMAND} -E touch ${PROJECT_BINARY_DIR}/python/stub_timestamp
  MAIN_DEPENDENCY
    ortools/python/setup.py.in
  DEPENDS
    python/ortools_timestamp
    python/pybind11_timestamp
  WORKING_DIRECTORY python
  COMMAND_EXPAND_LISTS)
endif()

# Look for required python modules
search_python_module(
  NAME setuptools
  PACKAGE setuptools)
search_python_module(
  NAME wheel
  PACKAGE wheel)

add_custom_command(
  OUTPUT python/dist_timestamp
  COMMAND ${CMAKE_COMMAND} -E remove_directory dist
  #COMMAND ${Python3_EXECUTABLE} setup.py bdist_egg bdist_wheel
  COMMAND ${Python3_EXECUTABLE} setup.py bdist_wheel
  COMMAND ${CMAKE_COMMAND} -E touch ${PROJECT_BINARY_DIR}/python/dist_timestamp
  MAIN_DEPENDENCY
    ortools/python/setup.py.in
  DEPENDS
    python/setup.py
    python/ortools_timestamp
    python/pybind11_timestamp
    $<$<BOOL:${GENERATE_PYTHON_STUB}>:python/stub_timestamp>
  BYPRODUCTS
    python/${PYTHON_PROJECT}
    python/${PYTHON_PROJECT}.egg-info
    python/build
    python/dist
  WORKING_DIRECTORY python
  COMMAND_EXPAND_LISTS)

# Main Target
add_custom_target(python_package ALL
  DEPENDS
    python/dist_timestamp
  WORKING_DIRECTORY python)

# Install rules
configure_file(
  ${PROJECT_SOURCE_DIR}/cmake/python-install.cmake.in
  ${PROJECT_BINARY_DIR}/python/python-install.cmake
  @ONLY)
install(SCRIPT ${PROJECT_BINARY_DIR}/python/python-install.cmake)

if(BUILD_VENV)
  # make a virtualenv to install our python package in it
  add_custom_command(TARGET python_package POST_BUILD
    # Clean previous install otherwise pip install may do nothing
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${VENV_DIR}
    COMMAND ${VENV_EXECUTABLE} -p ${Python3_EXECUTABLE}
    $<IF:$<BOOL:${VENV_USE_SYSTEM_SITE_PACKAGES}>,--system-site-packages,-q>
      ${VENV_DIR}
    #COMMAND ${VENV_EXECUTABLE} ${VENV_DIR}
    # Must NOT call it in a folder containing the setup.py otherwise pip call it
    # (i.e. "python setup.py bdist") while we want to consume the wheel package
    COMMAND ${VENV_Python3_EXECUTABLE} -m pip install
      --find-links=${CMAKE_CURRENT_BINARY_DIR}/python/dist ${PYTHON_PROJECT}==${PROJECT_VERSION}
    # install modules only required to run examples
    COMMAND ${VENV_Python3_EXECUTABLE} -m pip install
      pandas matplotlib pytest scipy svgwrite
    BYPRODUCTS ${VENV_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Create venv and install ${PYTHON_PROJECT}"
    VERBATIM)
endif()

if(BUILD_TESTING)
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/init/python/version_test.py.in
    ${PROJECT_BINARY_DIR}/python/version_test.py
    @ONLY)

  # run the tests within the virtualenv
  add_test(NAME python_init_version_test
    COMMAND ${VENV_Python3_EXECUTABLE} ${PROJECT_BINARY_DIR}/python/version_test.py)
endif()

###############
## Doc rules ##
###############
if(BUILD_PYTHON_DOC)
  # add a target to generate API documentation with Doxygen
  find_package(Doxygen)
  if(DOXYGEN_FOUND)
    configure_file(${PROJECT_SOURCE_DIR}/ortools/python/Doxyfile.in ${PROJECT_BINARY_DIR}/python/Doxyfile @ONLY)
    file(DOWNLOAD
      https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/v2.1.0/doxygen-awesome.css
      ${PROJECT_BINARY_DIR}/python/doxygen-awesome.css
      SHOW_PROGRESS
    )
    add_custom_target(${PROJECT_NAME}_python_doc ALL
      #COMMAND ${CMAKE_COMMAND} -E rm -rf ${PROJECT_BINARY_DIR}/docs/python
      COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/docs/python
      COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/python/Doxyfile
      DEPENDS
        python_package
        ${PROJECT_BINARY_DIR}/python/Doxyfile
        ${PROJECT_BINARY_DIR}/python/doxygen-awesome.css
        ${PROJECT_SOURCE_DIR}/ortools/python/stylesheet.css
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      COMMENT "Generating Python API documentation with Doxygen"
      VERBATIM)
  else()
    message(WARNING "cmd `doxygen` not found, Python doc generation is disable!")
  endif()

  # pdoc doc
  find_program(PDOC_PRG NAMES pdoc)
  if (PDOC_PRG)
    # add a target to generate API documentation with pdoc
    add_custom_target(${PROJECT_NAME}_pdoc_doc ALL
      #COMMAND ${CMAKE_COMMAND} -E rm -rf ${PROJECT_BINARY_DIR}/docs/pdoc
      COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/docs/pdoc
      COMMAND ${PDOC_PRG}
      --logo https://developers.google.com/optimization/images/orLogo.png
      --no-search -d google
      --footer-text "OR-Tools v${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
      -o ${PROJECT_BINARY_DIR}/docs/pdoc
      ${PROJECT_BINARY_DIR}/python/ortools
      DEPENDS python_package
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      COMMENT "Generating Python API documentation with pdoc"
      VERBATIM)
  else()
    message(WARNING "cmd `pdoc` not found, Python doc generation is disable!")
  endif()
endif()

#####################
##  Python Sample  ##
#####################
# add_python_sample()
# CMake function to generate and build python sample.
# Parameters:
#  FILE_NAME: the Python filename
#  COMPONENT_NAME: name of the ortools/ subdir where the test is located
#  note: automatically determined if located in ortools/<component>/samples/
# e.g.:
# add_python_sample(
#   FILE_NAME
#     ${PROJECT_SOURCE_DIR}/ortools/foo/sample/bar.py
#   COMPONENT_NAME
#     foo
# )
function(add_python_sample)
  set(options "")
  set(oneValueArgs FILE_NAME COMPONENT_NAME)
  set(multiValueArgs "")
  cmake_parse_arguments(SAMPLE
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
  if(NOT SAMPLE_FILE_NAME)
    message(FATAL_ERROR "no FILE_NAME provided")
  endif()
  get_filename_component(SAMPLE_NAME ${SAMPLE_FILE_NAME} NAME_WE)

  message(STATUS "Configuring sample ${SAMPLE_FILE_NAME} ...")

  if(NOT SAMPLE_COMPONENT_NAME)
    # sample is located in ortools/<component_name>/sample/
    get_filename_component(SAMPLE_DIR ${SAMPLE_FILE_NAME} DIRECTORY)
    get_filename_component(COMPONENT_DIR ${SAMPLE_DIR} DIRECTORY)
    get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)
  else()
    set(COMPONENT_NAME ${SAMPLE_COMPONENT_NAME})
  endif()

  if(BUILD_TESTING)
    add_test(
      NAME python_${COMPONENT_NAME}_${SAMPLE_NAME}
      COMMAND ${VENV_Python3_EXECUTABLE} ${SAMPLE_FILE_NAME}
      WORKING_DIRECTORY ${VENV_DIR})
  endif()
  message(STATUS "Configuring sample ${SAMPLE_FILE_NAME} ...DONE")
endfunction()

######################
##  Python Example  ##
######################
# add_python_example()
# CMake function to generate and build python example.
# Parameters:
#  FILE_NAME: the Python filename
#  COMPONENT_NAME: name of the ortools/ subdir where the test is located
#  note: automatically determined if located in ortools/<component>/samples/
# e.g.:
# add_python_example(
#   FILE_NAME
#     ${PROJECT_SOURCE_DIR}/examples/foo/bar.py
#   COMPONENT_NAME
#     foo
# )
function(add_python_example)
  set(options "")
  set(oneValueArgs FILE_NAME COMPONENT_NAME)
  set(multiValueArgs "")
  cmake_parse_arguments(EXAMPLE
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
if(NOT EXAMPLE_FILE_NAME)
    message(FATAL_ERROR "no FILE_NAME provided")
  endif()
  get_filename_component(EXAMPLE_NAME ${EXAMPLE_FILE_NAME} NAME_WE)

  message(STATUS "Configuring example ${EXAMPLE_FILE_NAME} ...")

  if(NOT EXAMPLE_COMPONENT_NAME)
    # example is located in example/<component_name>/
    get_filename_component(EXAMPLE_DIR ${EXAMPLE_FILE_NAME} DIRECTORY)
    get_filename_component(COMPONENT_NAME ${EXAMPLE_DIR} NAME)
  else()
    set(COMPONENT_NAME ${EXAMPLE_COMPONENT_NAME})
  endif()

  if(BUILD_TESTING)
    add_test(
      NAME python_${COMPONENT_NAME}_${EXAMPLE_NAME}
      COMMAND ${VENV_Python3_EXECUTABLE} ${EXAMPLE_FILE_NAME}
      WORKING_DIRECTORY ${VENV_DIR})
  endif()
  message(STATUS "Configuring example ${EXAMPLE_FILE_NAME} ...DONE")
endfunction()
