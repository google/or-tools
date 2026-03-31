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

set_property(SOURCE sat.swig PROPERTY CPLUSPLUS ON)
set_property(SOURCE sat.swig PROPERTY SWIG_MODULE_NAME main)
set_property(SOURCE sat.swig PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS})
set_property(SOURCE sat.swig PROPERTY COMPILE_OPTIONS
  -package ${JAVA_PACKAGE}.sat)

# SWIG generator
add_executable(gen_cp_model_proto_swig gen_cp_model_proto_swig.cc)
target_link_libraries(gen_cp_model_proto_swig PRIVATE
  ortools::ortools
  ${PROJECT_NAMESPACE}::util_java_wrappers
  absl::flags_parse
  absl::flags_usage
  absl::die_if_null
  absl::log_initialize
  absl::str_format)

include(GNUInstallDirs)
if(APPLE)
  set_target_properties(gen_cp_model_proto_swig PROPERTIES INSTALL_RPATH
    "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
elseif(UNIX)
  cmake_path(RELATIVE_PATH CMAKE_INSTALL_FULL_LIBDIR
             BASE_DIRECTORY ${CMAKE_INSTALL_FULL_BINDIR}
             OUTPUT_VARIABLE libdir_relative_path)
  set_target_properties(gen_cp_model_proto_swig PROPERTIES
                        INSTALL_RPATH "$ORIGIN/${libdir_relative_path}")
endif()

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/java_cp_model_proto_timestamp
  COMMAND gen_cp_model_proto_swig > "${CMAKE_CURRENT_BINARY_DIR}/cp_model_proto.swig"
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/java_cp_model_proto_timestamp
  DEPENDS gen_cp_model_proto_swig
  BYPRODUCTS
    ${CMAKE_CURRENT_BINARY_DIR}/cp_model_proto.swig
  COMMENT "Generate java/cp_model_proto.swig (java_cp_model_proto_timestamp)"
  VERBATIM)

add_custom_target(java_cp_model_proto
  DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/java_cp_model_proto_timestamp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# cp_model_proto
set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/cp_model_proto.swig" PROPERTY CPLUSPLUS ON)
set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/cp_model_proto.swig" PROPERTY SWIG_MODULE_NAME cp_model_proto)
set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/cp_model_proto.swig" PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS})
set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/cp_model_proto.swig" PROPERTY COMPILE_OPTIONS
  -package ${JAVA_PACKAGE}.sat)

set(SWIG_MODULE_jni_cp_model_proto_EXTRA_DEPS java_cp_model_proto)
swig_add_library(jni_cp_model_proto
  TYPE OBJECT
  LANGUAGE java
  OUTPUT_DIR ${JAVA_PROJECT_DIR}/${JAVA_SRC_PATH}/sat
  SOURCES "${CMAKE_CURRENT_BINARY_DIR}/cp_model_proto.swig")

target_include_directories(jni_cp_model_proto PRIVATE
  ${JNI_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR})
set_target_properties(jni_cp_model_proto PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(jni_cp_model_proto PRIVATE ortools::ortools)

# sat
set(SWIG_MODULE_jnisat_EXTRA_DEPS java_cp_model_proto)
swig_add_library(jnisat
  TYPE OBJECT
  LANGUAGE java
  OUTPUT_DIR ${JAVA_PROJECT_DIR}/${JAVA_SRC_PATH}/sat
  SOURCES sat.swig)

target_include_directories(jnisat PRIVATE
  ${JNI_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR})
set_target_properties(jnisat PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(jnisat PRIVATE ortools::ortools)

if(BUILD_TESTING)
  file(GLOB JAVA_SRCS "*Test.java")
  foreach(FILE_NAME IN LISTS JAVA_SRCS)
    add_java_test(FILE_NAME ${FILE_NAME})
  endforeach()
endif()
