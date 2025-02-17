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

add_subdirectory(indicator)
add_subdirectory(quadratic)
add_subdirectory(second_order_cone)
add_subdirectory(sos)
add_subdirectory(util)

set(NAME ${PROJECT_NAME}_math_opt_constraints)
add_library(${NAME} OBJECT)
target_sources(${NAME} PUBLIC
  $<TARGET_OBJECTS:${NAME}_indicator>
  $<TARGET_OBJECTS:${NAME}_quadratic>
  $<TARGET_OBJECTS:${NAME}_second_order_cone>
  $<TARGET_OBJECTS:${NAME}_sos>
  $<TARGET_OBJECTS:${NAME}_util>
)
install(TARGETS ${NAME} EXPORT ${PROJECT_NAME}Targets)
