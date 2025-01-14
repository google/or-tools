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

pybind11_add_module(pdlp_pybind11 MODULE pdlp.cc)
# note: macOS is APPLE and also UNIX !
set_target_properties(pdlp_pybind11 PROPERTIES
  LIBRARY_OUTPUT_NAME "pdlp")
if(APPLE)
  set_target_properties(pdlp_pybind11 PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs")
elseif(UNIX)
  set_target_properties(pdlp_pybind11 PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs")
endif()
target_link_libraries(pdlp_pybind11 PRIVATE
  ${PROJECT_NAMESPACE}::ortools
  pybind11_native_proto_caster
  protobuf::libprotobuf)
target_include_directories(pdlp_pybind11 PRIVATE ${protobuf_SOURCE_DIR})
add_library(${PROJECT_NAMESPACE}::pdlp_pybind11 ALIAS pdlp_pybind11)

if(BUILD_TESTING)
  file(GLOB PYTHON_SRCS "*_test.py")
  foreach(FILE_NAME IN LISTS PYTHON_SRCS)
    add_python_test(FILE_NAME ${FILE_NAME})
  endforeach()
endif()
