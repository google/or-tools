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

set_property(SOURCE linear_solver.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE linear_solver.i PROPERTY SWIG_MODULE_NAME pywraplp)
set_property(SOURCE linear_solver.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT=)
swig_add_library(pywraplp
  TYPE MODULE
  LANGUAGE python
  OUTPUT_DIR ${PYTHON_PROJECT_DIR}/linear_solver
  SOURCES linear_solver.i)

target_include_directories(pywraplp PRIVATE ${Python3_INCLUDE_DIRS})
set_property(TARGET pywraplp PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
target_compile_definitions(pywraplp PUBLIC "PY3")

# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(pywraplp PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../${PROJECT_NAME}/.libs")
  target_link_options(pywraplp PRIVATE "LINKER:-undefined,dynamic_lookup")
elseif(UNIX)
  set_target_properties(pywraplp PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../${PROJECT_NAME}/.libs")
endif()
target_link_libraries(pywraplp PRIVATE ortools::ortools)

# Variable PYTHON_LIBRARIES can contains keyword `optimized`
# which won't be interpreted inside a generator expression.
# i.e. we can't use: $<$<PLATFORM_ID:Windows>:${PYTHON_LIBRARIES}>
# see: https://cmake.org/cmake/help/git-stage/command/target_link_libraries.html#command:target_link_libraries
if(MSVC)
  target_link_libraries(pywraplp PRIVATE ${Python3_LIBRARIES})
endif()

pybind11_add_module(model_builder_helper_pybind11 MODULE model_builder_helper.cc)
set_target_properties(model_builder_helper_pybind11 PROPERTIES
  LIBRARY_OUTPUT_NAME "model_builder_helper")

target_include_directories(model_builder_helper_pybind11 PRIVATE
  ${protobuf_SOURCE_DIR})

# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(model_builder_helper_pybind11 PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs")
elseif(UNIX)
  set_target_properties(model_builder_helper_pybind11 PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs")
endif()

target_link_libraries(model_builder_helper_pybind11 PRIVATE
  ${PROJECT_NAMESPACE}::ortools
  pybind11_native_proto_caster
  Eigen3::Eigen)
add_library(${PROJECT_NAMESPACE}::model_builder_helper_pybind11 ALIAS model_builder_helper_pybind11)

if(BUILD_TESTING)
  file(GLOB PYTHON_SRCS "*_test.py")
  foreach(FILE_NAME IN LISTS PYTHON_SRCS)
    add_python_test(FILE_NAME ${FILE_NAME})
  endforeach()
endif()
