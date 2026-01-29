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

pybind11_add_module(piecewise_linear_function_pybind11 MODULE piecewise_linear_function.cc)
set_target_properties(piecewise_linear_function_pybind11 PROPERTIES
  LIBRARY_OUTPUT_NAME "piecewise_linear_function")
# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(piecewise_linear_function_pybind11 PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs")
elseif(UNIX)
  set_target_properties(piecewise_linear_function_pybind11 PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs")
endif()
target_link_libraries(piecewise_linear_function_pybind11 PRIVATE ${PROJECT_NAMESPACE}::ortools)
add_library(${PROJECT_NAMESPACE}::piecewise_linear_function_pybind11 ALIAS piecewise_linear_function_pybind11)

pybind11_add_module(sorted_interval_list_pybind11 MODULE sorted_interval_list.cc)
set_target_properties(sorted_interval_list_pybind11 PROPERTIES
  LIBRARY_OUTPUT_NAME "sorted_interval_list")
# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(sorted_interval_list_pybind11 PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs")
elseif(UNIX)
  set_target_properties(sorted_interval_list_pybind11 PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs")
endif()
target_link_libraries(sorted_interval_list_pybind11 PRIVATE ${PROJECT_NAMESPACE}::ortools)
add_library(${PROJECT_NAMESPACE}::sorted_interval_list_pybind11 ALIAS sorted_interval_list_pybind11)

add_library(py_solve_interrupter OBJECT
  "py_solve_interrupter.h"
  "py_solve_interrupter.cc")
set_target_properties(py_solve_interrupter PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(py_solve_interrupter PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)
target_link_libraries(py_solve_interrupter PRIVATE absl::base)

pybind11_add_module(solve_interrupter_pybind11 MODULE pybind_solve_interrupter.cc)
target_sources(solve_interrupter_pybind11 PRIVATE
  $<TARGET_OBJECTS:py_solve_interrupter>)
set_target_properties(solve_interrupter_pybind11 PROPERTIES
  LIBRARY_OUTPUT_NAME "pybind_solve_interrupter")
# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(solve_interrupter_pybind11 PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs")
elseif(UNIX)
  set_target_properties(solve_interrupter_pybind11 PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs")
endif()
target_link_libraries(solve_interrupter_pybind11 PRIVATE ${PROJECT_NAMESPACE}::ortools)
add_library(${PROJECT_NAMESPACE}::solve_interrupter_pybind11 ALIAS solve_interrupter_pybind11)


if(BUILD_TESTING)
  add_library(py_solve_interrupter_testing OBJECT
    "py_solve_interrupter_testing.cc")
  set_target_properties(py_solve_interrupter_testing PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_include_directories(py_solve_interrupter_testing PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)
  target_link_libraries(py_solve_interrupter_testing PRIVATE absl::base)

  pybind11_add_module(solve_interrupter_testing_pybind11 MODULE
    pybind_solve_interrupter_testing.cc)
  target_sources(solve_interrupter_testing_pybind11 PRIVATE
    $<TARGET_OBJECTS:py_solve_interrupter_testing>
  )
  set_target_properties(solve_interrupter_testing_pybind11 PROPERTIES
    LIBRARY_OUTPUT_NAME "pybind_solve_interrupter_testing")
  # note: macOS is APPLE and also UNIX !
  if(APPLE)
    set_target_properties(solve_interrupter_testing_pybind11 PROPERTIES
      SUFFIX ".so"
      INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs")
  elseif(UNIX)
    set_target_properties(solve_interrupter_testing_pybind11 PROPERTIES
      INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs")
  endif()
  target_link_libraries(solve_interrupter_testing_pybind11 PRIVATE ${PROJECT_NAMESPACE}::ortools)

  file(GLOB PYTHON_SRCS "*_test.py")
  foreach(FILE_NAME IN LISTS PYTHON_SRCS)
    add_python_test(FILE_NAME ${FILE_NAME})
  endforeach()
endif()
