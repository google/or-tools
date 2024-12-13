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

pybind11_add_module(linear_sum_assignment_pybind11 MODULE linear_sum_assignment.cc)
# note: macOS is APPLE and also UNIX !
set_target_properties(linear_sum_assignment_pybind11 PROPERTIES
  LIBRARY_OUTPUT_NAME "linear_sum_assignment")
if(APPLE)
  set_target_properties(linear_sum_assignment_pybind11 PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs"
    )
elseif(UNIX)
  set_target_properties(linear_sum_assignment_pybind11 PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs"
    )
endif()
target_link_libraries(linear_sum_assignment_pybind11 PRIVATE ${PROJECT_NAMESPACE}::ortools)
add_library(${PROJECT_NAMESPACE}::linear_sum_assignment_pybind11 ALIAS linear_sum_assignment_pybind11)

pybind11_add_module(max_flow_pybind11 MODULE max_flow.cc)
# note: macOS is APPLE and also UNIX !
set_target_properties(max_flow_pybind11 PROPERTIES
  LIBRARY_OUTPUT_NAME "max_flow")
if(APPLE)
  set_target_properties(max_flow_pybind11 PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs"
    )
elseif(UNIX)
  set_target_properties(max_flow_pybind11 PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs"
    )
endif()
target_link_libraries(max_flow_pybind11 PRIVATE ${PROJECT_NAMESPACE}::ortools)
add_library(${PROJECT_NAMESPACE}::max_flow_pybind11 ALIAS max_flow_pybind11)

pybind11_add_module(min_cost_flow_pybind11 MODULE min_cost_flow.cc)
# note: macOS is APPLE and also UNIX !
set_target_properties(min_cost_flow_pybind11 PROPERTIES
  LIBRARY_OUTPUT_NAME "min_cost_flow")
if(APPLE)
  set_target_properties(min_cost_flow_pybind11 PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs"
    )
elseif(UNIX)
  set_target_properties(min_cost_flow_pybind11 PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs"
    )
endif()
target_link_libraries(min_cost_flow_pybind11 PRIVATE ${PROJECT_NAMESPACE}::ortools)
add_library(${PROJECT_NAMESPACE}::min_cost_flow_pybind11 ALIAS min_cost_flow_pybind11)

if(BUILD_TESTING)
  file(GLOB PYTHON_SRCS "*_test.py")
  foreach(FILE_NAME IN LISTS PYTHON_SRCS)
    add_python_test(${FILE_NAME})
  endforeach()
endif()
