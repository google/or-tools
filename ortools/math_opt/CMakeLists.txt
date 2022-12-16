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

if(NOT BUILD_MATH_OPT)
  return()
endif()

add_subdirectory(core)
add_subdirectory(constraints)
add_subdirectory(cpp)
add_subdirectory(elemental)
add_subdirectory(io)
add_subdirectory(labs)
add_subdirectory(solver_tests)
add_subdirectory(solvers)
add_subdirectory(storage)
add_subdirectory(validators)

set(NAME ${PROJECT_NAME}_math_opt)
add_library(${NAME} OBJECT)
target_sources(${NAME} PUBLIC
  $<TARGET_OBJECTS:${NAME}_core>
  $<TARGET_OBJECTS:${NAME}_core_c_api>
  $<TARGET_OBJECTS:${NAME}_cpp>
  $<TARGET_OBJECTS:${NAME}_elemental>
  $<TARGET_OBJECTS:${NAME}_io>
  $<TARGET_OBJECTS:${NAME}_labs>
  $<TARGET_OBJECTS:${NAME}_solvers>
  $<TARGET_OBJECTS:${NAME}_storage>
  $<TARGET_OBJECTS:${NAME}_validators>
)
target_link_libraries(${NAME} INTERFACE
  ${NAME}_constraints
)
install(TARGETS ${PROJECT_NAME}_math_opt EXPORT ${PROJECT_NAME}Targets)
