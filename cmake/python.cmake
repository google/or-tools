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

if(NOT BUILD_PYTHON)
  return()
endif()

# see: https://packaging.python.org/en/latest/specifications/version-specifiers/
if(RELEASE)
  set(PYTHON_RELEASE "")
else()
  set(PYTHON_RELEASE "rc1")
endif()

# Use latest Python3 module (3.18)
cmake_minimum_required(VERSION 3.18)

if(NOT TARGET ${PROJECT_NAMESPACE}::ortools)
  message(FATAL_ERROR "Python: missing ortools TARGET")
endif()

# Find Python 3
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)

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
include(generated/protos)
set(PROTO_PYS)
set(PROTO_MYPYS)
foreach(PROTO_FILE IN LISTS ORTOOLS_PYTHON_PROTOS)
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
    DEPENDS ${PROJECT_SOURCE_DIR}/${PROTO_FILE} ${PROTOC_PRG}
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

set(PYTHON_PROJECT ${PROJECT_NAME})
message(STATUS "Python project: ${PYTHON_PROJECT}")
set(PYTHON_PROJECT_DIR ${PROJECT_BINARY_DIR}/python/${PYTHON_PROJECT})
message(STATUS "Python project build path: ${PYTHON_PROJECT_DIR}")

###################
##  Python Venv  ##
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
else()
  set(VENV_DIR ${PROJECT_BINARY_DIR}/python)
  set(VENV_Python3_EXECUTABLE ${Python3_EXECUTABLE})
endif()

########################
##  Helper Functions  ##
########################
# add_python_test()
# CMake function to generate and build python test.
# Parameters:
#  FILE_NAME: the python filename
#  COMPONENT_NAME: name of the ortools/ subdir where the test is located
#  DISABLED: mark the registered CTest target as DISABLED
#  note: automatically determined if located in ortools/<component>/python/
function(add_python_test)
  set(options DISABLED)
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

  if(BUILD_PYTHON_TESTING)
    add_test(
      NAME python_${COMPONENT_NAME}_${TEST_NAME}
      COMMAND ${VENV_Python3_EXECUTABLE} -m pytest ${TEST_FILE_NAME}
      WORKING_DIRECTORY ${VENV_DIR})
    if(TEST_DISABLED)
      set_tests_properties(python_${COMPONENT_NAME}_${TEST_NAME}
        PROPERTIES DISABLED TRUE)
    endif()
  endif()
  message(STATUS "Configuring test ${TEST_FILE_NAME} ...DONE")
endfunction()

# add_python_sample()
# CMake function to generate and build python sample.
# Parameters:
#  FILE_NAME: the Python filename
#  COMPONENT_NAME: name of the ortools/ subdir where the sample is located
#  DISABLED: mark the registered CTest target as DISABLED
function(add_python_sample)
  set(options DISABLED)
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
    # sample is located in ortools/<component_name>/samples/
    get_filename_component(SAMPLE_DIR ${SAMPLE_FILE_NAME} DIRECTORY)
    get_filename_component(COMPONENT_DIR ${SAMPLE_DIR} DIRECTORY)
    get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)
  else()
    set(COMPONENT_NAME ${SAMPLE_COMPONENT_NAME})
  endif()

  if(BUILD_PYTHON_TESTING)
    add_test(
      NAME python_${COMPONENT_NAME}_${SAMPLE_NAME}
      COMMAND ${VENV_Python3_EXECUTABLE} ${SAMPLE_FILE_NAME}
      WORKING_DIRECTORY ${VENV_DIR})
    if(SAMPLE_DISABLED)
      set_tests_properties(python_${COMPONENT_NAME}_${SAMPLE_NAME}
        PROPERTIES DISABLED TRUE)
    endif()
  endif()
  message(STATUS "Configuring sample ${SAMPLE_FILE_NAME} ...DONE")
endfunction()

# add_python_example()
# CMake function to generate and build python example.
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
    # example is located in examples/<component_name>/
    get_filename_component(EXAMPLE_DIR ${EXAMPLE_FILE_NAME} DIRECTORY)
    get_filename_component(COMPONENT_NAME ${EXAMPLE_DIR} NAME)
  else()
    set(COMPONENT_NAME ${EXAMPLE_COMPONENT_NAME})
  endif()

  if(BUILD_PYTHON_TESTING)
    add_test(
      NAME python_${COMPONENT_NAME}_${EXAMPLE_NAME}
      COMMAND ${VENV_Python3_EXECUTABLE} ${EXAMPLE_FILE_NAME}
      WORKING_DIRECTORY ${VENV_DIR})
  endif()
  message(STATUS "Configuring example ${EXAMPLE_FILE_NAME} ...DONE")
endfunction()

# add_python_binary()
# CMake function to generate a shell wrapper to execute a Python program.
function(add_python_binary)
  set(options "")
  set(oneValueArgs NAME;FILE)
  set(multiValueArgs "")
  cmake_parse_arguments(PY_BINARY
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
  message(STATUS "Configuring python binary ${PY_BINARY_NAME} ...")
  if(NOT PY_BINARY_NAME)
    message(FATAL_ERROR "no NAME provided for python binary")
  endif()
  if(NOT PY_BINARY_FILE)
    message(FATAL_ERROR "no FILE provided for python binary")
  endif()
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${PY_BINARY_NAME}" "#!/usr/bin/env sh\n${VENV_Python3_EXECUTABLE} ${PY_BINARY_FILE} \"$@\"\n")
  file(CHMOD "${CMAKE_CURRENT_BINARY_DIR}/${PY_BINARY_NAME}"
      FILE_PERMISSIONS
          OWNER_READ OWNER_EXECUTE
          GROUP_READ GROUP_EXECUTE
          WORLD_READ WORLD_EXECUTE
  )
  add_executable("${PY_BINARY_NAME}" IMPORTED)
  set_target_properties("${PY_BINARY_NAME}" PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/${PY_BINARY_NAME}")
  message(STATUS "Configuring python binary ${PY_BINARY_NAME} ...DONE")
endfunction()

# ortools_python_copy()
# Copies pure-Python module sources (and checked-in .pyi stubs) into the staging
# package tree and records the destination package directory.
function(ortools_python_copy)
  set(options "")
  set(oneValueArgs PACKAGE_DIR)
  set(multiValueArgs SOURCES)
  cmake_parse_arguments(COPY
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
  if(NOT COPY_PACKAGE_DIR)
    message(FATAL_ERROR "no PACKAGE_DIR provided")
  endif()
  if(NOT COPY_SOURCES)
    message(FATAL_ERROR "no SOURCES provided")
  endif()
  file(COPY ${COPY_SOURCES}
    DESTINATION "${PYTHON_PROJECT_DIR}/${COPY_PACKAGE_DIR}")
  set_property(GLOBAL APPEND PROPERTY ORTOOLS_PYTHON_PURE_DIRS
    "${COPY_PACKAGE_DIR}")
endfunction()

# ortools_python_extension()
# Builds a pybind11 C++ Python extension module and registers it for wheel
# staging, setup.py package_data, and optional stubgen execution.
function(ortools_python_extension)
  set(options RPATH_ABSEIL TESTONLY NO_STUBGEN)
  set(oneValueArgs NAME MODULE_NAME PACKAGE_DIR)
  set(multiValueArgs SOURCES LINK_LIBRARIES INCLUDE_DIRS)
  cmake_parse_arguments(EXT
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
  if(NOT EXT_NAME)
    message(FATAL_ERROR "no NAME provided")
  endif()
  if(NOT EXT_MODULE_NAME)
    message(FATAL_ERROR "no MODULE_NAME provided")
  endif()
  if(NOT EXT_PACKAGE_DIR)
    message(FATAL_ERROR "no PACKAGE_DIR provided")
  endif()
  if(NOT EXT_SOURCES)
    message(FATAL_ERROR "no SOURCES provided")
  endif()

  pybind11_add_module(${EXT_NAME} MODULE ${EXT_SOURCES})
  target_compile_features(${EXT_NAME} PRIVATE cxx_std_20)
  set_target_properties(${EXT_NAME} PROPERTIES
    LIBRARY_OUTPUT_NAME "${EXT_MODULE_NAME}"
    ORTOOLS_PACKAGE_DIR "${EXT_PACKAGE_DIR}"
    ORTOOLS_MODULE_NAME "${EXT_MODULE_NAME}"
    ORTOOLS_TESTONLY "${EXT_TESTONLY}"
    ORTOOLS_NO_STUBGEN "${EXT_NO_STUBGEN}"
    ORTOOLS_RPATH_ABSEIL "${EXT_RPATH_ABSEIL}")

  file(RELATIVE_PATH _rel_py_root
    "${PYTHON_PROJECT_DIR}/${EXT_PACKAGE_DIR}"
    "${PROJECT_BINARY_DIR}/python")
  # note: macOS is APPLE and also UNIX !
  if(APPLE)
    set(_rpath "@loader_path;@loader_path/${_rel_py_root}/${PYTHON_PROJECT}/.libs")
    if(EXT_RPATH_ABSEIL)
      string(APPEND _rpath ";@loader_path/${_rel_py_root}/pybind11_abseil")
    endif()
    set_target_properties(${EXT_NAME} PROPERTIES
      SUFFIX ".so"
      INSTALL_RPATH "${_rpath}")
  elseif(UNIX)
    set(_rpath "$ORIGIN:$ORIGIN/${_rel_py_root}/${PYTHON_PROJECT}/.libs")
    if(EXT_RPATH_ABSEIL)
      string(APPEND _rpath ":$ORIGIN/${_rel_py_root}/pybind11_abseil")
    endif()
    set_target_properties(${EXT_NAME} PROPERTIES
      INSTALL_RPATH "${_rpath}")
  endif()

  target_include_directories(${EXT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_BINARY_DIR}
    ${EXT_INCLUDE_DIRS})
  target_link_libraries(${EXT_NAME} PRIVATE ${EXT_LINK_LIBRARIES})
  add_library(${PROJECT_NAMESPACE}::${EXT_NAME} ALIAS ${EXT_NAME})
  set_property(GLOBAL APPEND PROPERTY ORTOOLS_PYBIND_TARGETS "${EXT_NAME}")
endfunction()

# Recursively collects non-imported SHARED_LIBRARY targets defined under DIR.
function(_ortools_collect_shared_targets OUT_VAR DIR)
  set(_collected "")
  if(DIR AND IS_DIRECTORY "${DIR}")
    get_property(_dir_targets DIRECTORY "${DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_t IN LISTS _dir_targets)
      if(TARGET "${_t}")
        get_target_property(_t_type "${_t}" TYPE)
        if(_t_type STREQUAL "SHARED_LIBRARY")
          list(APPEND _collected "${_t}")
        endif()
      endif()
    endforeach()
    get_property(_subdirs DIRECTORY "${DIR}" PROPERTY SUBDIRECTORIES)
    foreach(_sub IN LISTS _subdirs)
      _ortools_collect_shared_targets(_sub_targets "${_sub}")
      list(APPEND _collected ${_sub_targets})
    endforeach()
  endif()
  set(${OUT_VAR} "${_collected}" PARENT_SCOPE)
endfunction()

#######################
##  PYTHON WRAPPERS  ##
#######################
set_property(GLOBAL PROPERTY ORTOOLS_PYTHON_PURE_DIRS "")
set_property(GLOBAL PROPERTY ORTOOLS_PYTHON_GENERATED_SRCS "")
set_property(GLOBAL PROPERTY ORTOOLS_PYBIND_TARGETS "")
include(generated/python)

get_property(_py_pure_dirs GLOBAL PROPERTY ORTOOLS_PYTHON_PURE_DIRS)
get_property(_py_generated_srcs GLOBAL PROPERTY ORTOOLS_PYTHON_GENERATED_SRCS)
get_property(_pybind_targets GLOBAL PROPERTY ORTOOLS_PYBIND_TARGETS)

set(_need_pybind11_abseil_status FALSE)
foreach(_ext IN LISTS _pybind_targets)
  get_target_property(_rpath_abseil ${_ext} ORTOOLS_RPATH_ABSEIL)
  if(_rpath_abseil)
    set(_need_pybind11_abseil_status TRUE)
  endif()
endforeach()

#######################
## Python Packaging  ##
#######################
configure_file(
  ${PROJECT_SOURCE_DIR}/ortools/python/__init__.py.in
  ${PROJECT_BINARY_DIR}/python/__init__.py.in
  @ONLY)
file(GENERATE
  OUTPUT ${PYTHON_PROJECT_DIR}/__init__.py
  INPUT ${PROJECT_BINARY_DIR}/python/__init__.py.in)

if(_need_pybind11_abseil_status AND TARGET status_py_extension_stub)
  file(GENERATE
    OUTPUT ${PYTHON_PROJECT_DIR}/../pybind11_abseil/__init__.py
    CONTENT "")
endif()

set(_seed_dirs "")
foreach(PROTO_FILE IN LISTS ORTOOLS_PYTHON_PROTOS)
  get_filename_component(_pdir "${PROTO_FILE}" DIRECTORY)
  string(REGEX REPLACE "^ortools/" "" _psub "${_pdir}")
  if(_psub AND NOT _psub STREQUAL "ortools")
    list(APPEND _seed_dirs "${_psub}")
  endif()
endforeach()
list(APPEND _seed_dirs ${_py_pure_dirs})
foreach(_ext IN LISTS _pybind_targets)
  get_target_property(_pkg_dir ${_ext} ORTOOLS_PACKAGE_DIR)
  list(APPEND _seed_dirs "${_pkg_dir}")
endforeach()
if(BUILD_CORE)
  list(APPEND _seed_dirs ${ORTOOLS_PYTHON_TYPED_DIRS})
endif()

set(_all_init_dirs "")
foreach(_d IN LISTS _seed_dirs)
  set(_cur "${_d}")
  while(_cur AND NOT _cur STREQUAL ".")
    list(APPEND _all_init_dirs "${_cur}")
    get_filename_component(_cur "${_cur}" DIRECTORY)
  endwhile()
endforeach()
if(_all_init_dirs)
  list(REMOVE_DUPLICATES _all_init_dirs)
  list(SORT _all_init_dirs)
endif()

foreach(_dir IN LISTS _all_init_dirs)
  file(GENERATE OUTPUT "${PYTHON_PROJECT_DIR}/${_dir}/__init__.py" CONTENT "")
endforeach()

# Add py.typed markers to typed packages.
foreach(_tdir IN LISTS ORTOOLS_PYTHON_TYPED_DIRS)
  if(_tdir IN_LIST _all_init_dirs)
    file(MAKE_DIRECTORY "${PYTHON_PROJECT_DIR}/${_tdir}")
    file(TOUCH "${PYTHON_PROJECT_DIR}/${_tdir}/py.typed")
  endif()
endforeach()

# Build package_data entries dynamically for setup.py.in.
set(_pkg_data_lines "        '${PYTHON_PROJECT}': ['.libs/*'],")
if(_need_pybind11_abseil_status AND TARGET status_py_extension_stub)
  list(APPEND _pkg_data_lines
    "        'pybind11_abseil': ['$<TARGET_FILE_NAME:status_py_extension_stub>', '*.pyi'],")
endif()
foreach(_dir IN LISTS _all_init_dirs)
  string(REPLACE "/" "." _dots "${_dir}")
  set(_items "")
  foreach(_ext IN LISTS _pybind_targets)
    get_target_property(_ext_dir ${_ext} ORTOOLS_PACKAGE_DIR)
    if(_ext_dir STREQUAL _dir)
      list(APPEND _items "'$<TARGET_FILE_NAME:${_ext}>'")
    endif()
  endforeach()
  list(APPEND _items "'*.pyi'")
  if(_dir IN_LIST ORTOOLS_PYTHON_TYPED_DIRS)
    list(APPEND _items "'py.typed'")
  endif()
  list(JOIN _items ", " _items_str)
  list(APPEND _pkg_data_lines
    "        '${PYTHON_PROJECT}.${_dots}': [${_items_str}],")
endforeach()
list(JOIN _pkg_data_lines "\n" ORTOOLS_PYTHON_PACKAGE_DATA)

# setup.py.in contains cmake variable e.g. @PYTHON_PROJECT@ and
# generator expression e.g. $<TARGET_FILE_NAME:pyFoo>
configure_file(
  ${PROJECT_SOURCE_DIR}/ortools/python/setup.py.in
  ${PROJECT_BINARY_DIR}/python/setup.py.in
  @ONLY)
file(GENERATE
  OUTPUT ${PROJECT_BINARY_DIR}/python/setup.py
  INPUT ${PROJECT_BINARY_DIR}/python/setup.py.in)

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

set(need_unix_bzip2_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_BZip2}>>")
set(need_windows_bzip2_lib "$<AND:${is_windows},$<BOOL:${BUILD_BZip2}>>")

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

set(need_unix_scip_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_SCIP}>>")
set(need_windows_scip_lib "$<AND:${is_windows},$<BOOL:${BUILD_SCIP}>>")

set(is_ortools_shared "$<STREQUAL:$<TARGET_PROPERTY:ortools,TYPE>,SHARED_LIBRARY>")
set(need_unix_ortools_lib "$<AND:${is_not_windows},${is_ortools_shared}>")
set(need_windows_ortools_lib "$<AND:${is_windows},${is_ortools_shared}>")

# Dynamically discover Abseil shared libraries from ORTOOLS_ABSL_BINARY_DIR.
set(_absl_shared_targets "")
if(BUILD_absl AND ORTOOLS_ABSL_BINARY_DIR)
  _ortools_collect_shared_targets(_absl_shared_targets "${ORTOOLS_ABSL_BINARY_DIR}")
endif()
set(_absl_copy_command "")
if(_absl_shared_targets)
  set(_absl_lib_files "")
  foreach(_absl_tgt IN LISTS _absl_shared_targets)
    list(APPEND _absl_lib_files
      "$<${need_unix_absl_lib}:$<TARGET_SONAME_FILE:${_absl_tgt}>>"
      "$<${need_windows_absl_lib}:$<TARGET_FILE:${_absl_tgt}>>")
  endforeach()
  set(_absl_copy_command
    COMMAND ${CMAKE_COMMAND} -E copy ${_absl_lib_files} ${PYTHON_PROJECT}/.libs)
endif()

# Dynamically collect active OR-Tools shared component libraries.
set(_ortools_lib_files "")
set(_ortools_lib_deps "")
foreach(_comp IN ITEMS
    ortools_proto
    ortools_core
    ortools_packing
    ortools_scheduling
    ortools_routing
    ortools_math_opt
    ortools)
  if(TARGET ${PROJECT_NAMESPACE}::${_comp})
    list(APPEND _ortools_lib_deps ${PROJECT_NAMESPACE}::${_comp})
    list(APPEND _ortools_lib_files
      "$<${need_unix_ortools_lib}:$<TARGET_SONAME_FILE:${PROJECT_NAMESPACE}::${_comp}>>"
      "$<${need_windows_ortools_lib}:$<TARGET_FILE:${PROJECT_NAMESPACE}::${_comp}>>")
  endif()
endforeach()

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
    $<IF:$<BOOL:${BUILD_BZip2}>,copy,true>
    $<${need_unix_bzip2_lib}:$<TARGET_SONAME_FILE:BZip2::BZip2>>
    $<${need_windows_bzip2_lib}:$<TARGET_FILE:BZip2::BZip2>>
    ${PYTHON_PROJECT}/.libs

  ${_absl_copy_command}

  COMMAND ${CMAKE_COMMAND} -E
    $<IF:$<BOOL:${BUILD_re2}>,copy,true>
    $<${need_unix_re2_lib}:$<TARGET_SONAME_FILE:re2::re2>>
    $<${need_windows_re2_lib}:$<TARGET_FILE:re2::re2>>
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
    $<${need_unix_highs_lib}:$<TARGET_SONAME_FILE:highs::highs>>
    $<${need_windows_highs_lib}:$<TARGET_FILE:highs::highs>>
    ${PYTHON_PROJECT}/.libs

  COMMAND ${CMAKE_COMMAND} -E
    $<IF:$<BOOL:${BUILD_SCIP}>,copy,true>
    $<${need_unix_scip_lib}:$<TARGET_SONAME_FILE:SCIP::libscip>>
    $<${need_windows_scip_lib}:$<TARGET_FILE:SCIP::libscip>>
    ${PYTHON_PROJECT}/.libs

  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${is_ortools_shared},copy,true>
    ${_ortools_lib_files}
    ${PYTHON_PROJECT}/.libs

  COMMAND ${CMAKE_COMMAND} -E touch ${PROJECT_BINARY_DIR}/python/ortools_timestamp
  MAIN_DEPENDENCY
    ortools/python/setup.py.in
  DEPENDS
    Py${PROJECT_NAME}_proto
    ${_ortools_lib_deps}
    ${_py_generated_srcs}
  WORKING_DIRECTORY python
  COMMAND_EXPAND_LISTS)

set(_pybind_copy_commands "")
set(_pybind_depends "")
foreach(_ext IN LISTS _pybind_targets)
  get_target_property(_pkg_dir ${_ext} ORTOOLS_PACKAGE_DIR)
  list(APPEND _pybind_copy_commands
    COMMAND ${CMAKE_COMMAND} -E copy
      "$<TARGET_FILE:${_ext}>" "${PYTHON_PROJECT}/${_pkg_dir}")
  list(APPEND _pybind_depends ${_ext})
endforeach()
if(_need_pybind11_abseil_status AND TARGET status_py_extension_stub)
  list(APPEND _pybind_copy_commands
    COMMAND ${CMAKE_COMMAND} -E copy
      "$<TARGET_FILE:status_py_extension_stub>"
      "${PYTHON_PROJECT}/../pybind11_abseil")
  list(APPEND _pybind_depends status_py_extension_stub)
endif()

add_custom_command(
  OUTPUT python/pybind11_timestamp
  COMMAND ${CMAKE_COMMAND} -E remove -f pybind11_timestamp
  ${_pybind_copy_commands}
  COMMAND ${CMAKE_COMMAND} -E touch ${PROJECT_BINARY_DIR}/python/pybind11_timestamp
  MAIN_DEPENDENCY
    ortools/python/setup.py.in
  DEPENDS
    ${_pybind_depends}
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

  set(_stubgen_commands "")
  foreach(_ext IN LISTS _pybind_targets)
    get_target_property(_pkg_dir ${_ext} ORTOOLS_PACKAGE_DIR)
    get_target_property(_mod_name ${_ext} ORTOOLS_MODULE_NAME)
    get_target_property(_is_testonly ${_ext} ORTOOLS_TESTONLY)
    get_target_property(_no_stubgen ${_ext} ORTOOLS_NO_STUBGEN)
    if(NOT _is_testonly AND NOT _no_stubgen)
      string(REPLACE "/" "." _pkg_dots "${_pkg_dir}")
      list(APPEND _stubgen_commands
        COMMAND ${stubgen_EXECUTABLE}
          -p "${PYTHON_PROJECT}.${_pkg_dots}.${_mod_name}" --output .)
    endif()
  endforeach()
  if(_need_pybind11_abseil_status AND TARGET status_py_extension_stub)
    list(APPEND _stubgen_commands
      COMMAND ${stubgen_EXECUTABLE} -p pybind11_abseil.status --output .
      COMMAND ${stubgen_EXECUTABLE} -p pybind11_abseil.absl_casters --output .)
  endif()

  add_custom_command(
    OUTPUT python/stub_timestamp
    COMMAND ${CMAKE_COMMAND} -E remove -f stub_timestamp
    ${_stubgen_commands}
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
search_python_module(
  NAME typing_extensions
  PACKAGE typing-extensions
  NO_VERSION)

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
      --find-links=${CMAKE_CURRENT_BINARY_DIR}/python/dist ${PYTHON_PROJECT}==${PROJECT_VERSION}${PYTHON_RELEASE}
    # install modules only required to run examples
    COMMAND ${VENV_Python3_EXECUTABLE} -m pip install
      pandas matplotlib pytest scipy svgwrite
    BYPRODUCTS ${VENV_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Create venv and install ${PYTHON_PROJECT}"
    VERBATIM)
endif()

if(BUILD_PYTHON_TESTING)
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
      https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/v2.3.4/doxygen-awesome.css
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
        ${PROJECT_SOURCE_DIR}/ortools/doxygen/header.html
        ${PROJECT_SOURCE_DIR}/ortools/doxygen/DoxygenLayout.xml
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
