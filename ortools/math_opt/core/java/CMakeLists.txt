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

# solver
set_property(SOURCE solver.swig PROPERTY CPLUSPLUS ON)
set_property(SOURCE solver.swig PROPERTY SWIG_MODULE_NAME JniSolver)
set_property(SOURCE solver.swig PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS})
set_property(SOURCE solver.swig PROPERTY COMPILE_OPTIONS
  -package ${JAVA_PACKAGE}.mathopt)

#set(SWIG_MODULE_jnimathopt_EXTRA_DEPS java_model_proto)
swig_add_library(jnimathopt
  TYPE OBJECT
  LANGUAGE java
  OUTPUT_DIR ${JAVA_PROJECT_DIR}/${JAVA_SRC_PATH}/mathopt
  SOURCES solver.swig)
target_include_directories(jnimathopt PRIVATE
  ${JNI_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR})
set_target_properties(jnimathopt PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(jnimathopt PRIVATE ortools::ortools)

# JNI Helper
add_library(mathopt_java_jni_helper OBJECT solver.h solver.cc)
set_target_properties(mathopt_java_jni_helper PROPERTIES
  POSITION_INDEPENDENT_CODE ON)
target_include_directories(mathopt_java_jni_helper PUBLIC
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR})
target_link_libraries(mathopt_java_jni_helper PRIVATE
  JNI::JNI
  JNI::JVM
  ortools::math_opt_proto
  absl::statusor
  protobuf::libprotobuf
)
add_library(${PROJECT_NAMESPACE}::mathopt_java_jni_helper ALIAS mathopt_java_jni_helper)

