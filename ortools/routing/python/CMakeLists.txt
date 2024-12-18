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

# routing
pybind11_add_module(routing_pybind11 MODULE model.cc)
set_target_properties(routing_pybind11 PROPERTIES
  LIBRARY_OUTPUT_NAME "model")

# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(routing_pybind11 PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs")
elseif(UNIX)
  set_target_properties(routing_pybind11 PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs")
endif()

target_link_libraries(routing_pybind11 PRIVATE
  ${PROJECT_NAMESPACE}::ortools
  pybind11_native_proto_caster
)
add_library(${PROJECT_NAMESPACE}::routing_pybind11 ALIAS routing_pybind11)

# legacy pywraprouting
set_property(SOURCE routing.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE routing.i PROPERTY SWIG_MODULE_NAME pywraprouting)
set_property(SOURCE routing.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT=)
set_property(SOURCE routing.i PROPERTY COMPILE_OPTIONS -nofastunpack)
swig_add_library(pywraprouting
  TYPE MODULE
  LANGUAGE python
  OUTPUT_DIR ${PYTHON_PROJECT_DIR}/routing
  SOURCES routing.i)

target_include_directories(pywraprouting PRIVATE ${Python3_INCLUDE_DIRS})
set_property(TARGET pywraprouting PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
target_compile_definitions(pywraprouting PUBLIC "PY3")

# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(pywraprouting PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../${PROJECT_NAME}/.libs")
  target_link_options(pywraprouting PRIVATE "LINKER:-undefined,dynamic_lookup")
elseif(UNIX)
  set_target_properties(pywraprouting PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../${PROJECT_NAME}/.libs")
endif()
target_link_libraries(pywraprouting PRIVATE ortools::ortools)

# Variable PYTHON_LIBRARIES can contains keyword `optimized`
# which won't be interpreted inside a generator expression.
# i.e. we can't use: $<$<PLATFORM_ID:Windows>:${PYTHON_LIBRARIES}>
# see: https://cmake.org/cmake/help/git-stage/command/target_link_libraries.html#command:target_link_libraries
if(MSVC)
  target_link_libraries(pywraprouting PRIVATE ${Python3_LIBRARIES})
endif()

# Test
if(BUILD_TESTING)
  file(GLOB PYTHON_SRCS "*_test.py")
  foreach(FILE_NAME IN LISTS PYTHON_SRCS)
    add_python_test(FILE_NAME ${FILE_NAME})
  endforeach()
endif()
