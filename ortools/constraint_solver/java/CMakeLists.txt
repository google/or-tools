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

set_property(SOURCE constraint_solver.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE constraint_solver.i PROPERTY SWIG_MODULE_NAME Globals)
set_property(SOURCE constraint_solver.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT=)
set_property(SOURCE constraint_solver.i PROPERTY COMPILE_OPTIONS
  -package ${JAVA_PACKAGE}.constraintsolver)
swig_add_library(jniconstraint_solver
  TYPE OBJECT
  LANGUAGE java
  OUTPUT_DIR ${JAVA_PROJECT_DIR}/${JAVA_SRC_PATH}/constraintsolver
  SOURCES constraint_solver.i)

target_include_directories(jniconstraint_solver PRIVATE ${JNI_INCLUDE_DIRS})
set_target_properties(jniconstraint_solver PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(jniconstraint_solver PRIVATE ortools::ortools)

if(BUILD_TESTING)
  file(GLOB JAVA_SRCS "*Test.java")
  foreach(FILE_NAME IN LISTS JAVA_SRCS)
    add_java_test(FILE_NAME ${FILE_NAME})
  endforeach()
endif()
