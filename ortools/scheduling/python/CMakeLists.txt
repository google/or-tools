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

set_property(SOURCE rcpsp.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE rcpsp.i PROPERTY SWIG_MODULE_NAME pywraprcpsp)
set_property(SOURCE rcpsp.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT=)
swig_add_library(pywraprcpsp
  TYPE MODULE
  LANGUAGE python
  OUTPUT_DIR ${PYTHON_PROJECT_DIR}/scheduling
  SOURCES rcpsp.i)

target_include_directories(pywraprcpsp PRIVATE ${Python3_INCLUDE_DIRS})
set_property(TARGET pywraprcpsp PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
target_compile_definitions(pywraprcpsp PUBLIC "PY3")

# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(pywraprcpsp PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../${PROJECT_NAME}/.libs")
  set_property(TARGET pywraprcpsp APPEND PROPERTY
    LINK_FLAGS "-flat_namespace -undefined suppress")
elseif(UNIX)
  set_target_properties(pywraprcpsp PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../${PROJECT_NAME}/.libs")
endif()
target_link_libraries(pywraprcpsp PRIVATE ortools::ortools)

# Variable PYTHON_LIBRARIES can contains keyword `optimized`
# which won't be interpreted inside a generator expression.
# i.e. we can't use: $<$<PLATFORM_ID:Windows>:${PYTHON_LIBRARIES}>
# see: https://cmake.org/cmake/help/git-stage/command/target_link_libraries.html#command:target_link_libraries
if(MSVC)
  target_link_libraries(pywraprcpsp PRIVATE ${Python3_LIBRARIES})
endif()

if(BUILD_TESTING)
  file(GLOB PYTHON_SRCS "*_test.py")
  foreach(FILE_NAME IN LISTS PYTHON_SRCS)
    add_python_test(${FILE_NAME})
  endforeach()
endif()
