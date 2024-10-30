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

if(NOT BUILD_JAVA)
  return()
endif()

if(NOT TARGET ${PROJECT_NAMESPACE}::ortools)
  message(FATAL_ERROR "Java: missing ${PROJECT_NAMESPACE}::ortools TARGET")
endif()

# Will need swig
set(CMAKE_SWIG_FLAGS)
find_package(SWIG REQUIRED)
include(UseSWIG)

if(${SWIG_VERSION} VERSION_GREATER_EQUAL 4)
  list(APPEND CMAKE_SWIG_FLAGS "-doxygen")
endif()

if(UNIX AND NOT APPLE)
  list(APPEND CMAKE_SWIG_FLAGS "-DSWIGWORDSIZE64")
endif()

# Find Java and JNI
find_package(Java 1.8 COMPONENTS Development REQUIRED)
find_package(JNI REQUIRED)

# Find maven
# On windows mvn spawn a process while mvn.cmd is a blocking command
if(UNIX)
  find_program(MAVEN_EXECUTABLE mvn)
else()
  find_program(MAVEN_EXECUTABLE mvn.cmd)
endif()
if(NOT MAVEN_EXECUTABLE)
  message(FATAL_ERROR "Check for maven Program: not found")
else()
  message(STATUS "Found Maven: ${MAVEN_EXECUTABLE}")
endif()

# Needed by java/CMakeLists.txt
set(JAVA_DOMAIN_NAME "google")
set(JAVA_DOMAIN_EXTENSION "com")

set(JAVA_GROUP "${JAVA_DOMAIN_EXTENSION}.${JAVA_DOMAIN_NAME}")
set(JAVA_ARTIFACT "ortools")

set(JAVA_PACKAGE "${JAVA_GROUP}.${JAVA_ARTIFACT}")
if(APPLE)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)")
    set(NATIVE_IDENTIFIER darwin-aarch64)
  else()
    set(NATIVE_IDENTIFIER darwin-x86-64)
  endif()
elseif(UNIX)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)")
    set(NATIVE_IDENTIFIER linux-aarch64)
  else()
    set(NATIVE_IDENTIFIER linux-x86-64)
  endif()
elseif(WIN32)
  set(NATIVE_IDENTIFIER win32-x86-64)
else()
  message(FATAL_ERROR "Unsupported system !")
endif()
set(JAVA_NATIVE_PROJECT ${JAVA_ARTIFACT}-${NATIVE_IDENTIFIER})
message(STATUS "Java runtime project: ${JAVA_NATIVE_PROJECT}")
set(JAVA_NATIVE_PROJECT_DIR ${PROJECT_BINARY_DIR}/java/${JAVA_NATIVE_PROJECT})
message(STATUS "Java runtime project build path: ${JAVA_NATIVE_PROJECT_DIR}")

set(JAVA_PROJECT ${JAVA_ARTIFACT}-java)
message(STATUS "Java project: ${JAVA_PROJECT}")
set(JAVA_PROJECT_DIR ${PROJECT_BINARY_DIR}/java/${JAVA_PROJECT})
message(STATUS "Java project build path: ${JAVA_PROJECT_DIR}")

##################
##  PROTO FILE  ##
##################
# Generate Protobuf java sources
set(PROTO_JAVAS)
file(GLOB_RECURSE proto_java_files RELATIVE ${PROJECT_SOURCE_DIR}
  "ortools/algorithms/*.proto"
  "ortools/bop/*.proto"
  "ortools/constraint_solver/*.proto"
  "ortools/glop/*.proto"
  "ortools/graph/*.proto"
  "ortools/linear_solver/*.proto"
  "ortools/routing/*.proto"
  "ortools/sat/*.proto"
  "ortools/util/*.proto"
  )
if(USE_PDLP)
  file(GLOB_RECURSE pdlp_proto_java_files RELATIVE ${PROJECT_SOURCE_DIR} "ortools/pdlp/*.proto")
  list(APPEND proto_java_files ${pdlp_proto_java_files})
endif()
list(REMOVE_ITEM proto_java_files "ortools/constraint_solver/demon_profiler.proto")
list(REMOVE_ITEM proto_java_files "ortools/constraint_solver/assignment.proto")
foreach(PROTO_FILE IN LISTS proto_java_files)
  #message(STATUS "protoc proto(java): ${PROTO_FILE}")
  get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
  string(REGEX REPLACE "_" "" PROTO_DIR ${PROTO_DIR})
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(PROTO_OUT java/${JAVA_PROJECT}/src/main/java/com/google/${PROTO_DIR})
  set(PROTO_JAVA ${PROTO_OUT}/${PROTO_NAME}_timestamp)
  #message(STATUS "protoc java: ${PROTO_JAVA}")
  add_custom_command(
    OUTPUT ${PROTO_JAVA}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_OUT}
    COMMAND ${PROTOC_PRG}
    "--proto_path=${PROJECT_SOURCE_DIR}"
    ${PROTO_DIRS}
    "--java_out=${PROJECT_BINARY_DIR}/java/${JAVA_PROJECT}/src/main/java"
    ${PROTO_FILE}
    COMMAND ${CMAKE_COMMAND} -E touch ${PROTO_JAVA}
    DEPENDS
      ${PROJECT_SOURCE_DIR}/${PROTO_FILE}
      ${PROTOC_PRG}
    COMMENT "Generate Java protocol buffer for ${PROJECT_SOURCE_DIR}/${PROTO_FILE}"
    VERBATIM
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
  list(APPEND PROTO_JAVAS ${PROTO_JAVA})
endforeach()
add_custom_target(Java${PROJECT_NAME}_proto
  DEPENDS
    ${PROTO_JAVAS}
    ${PROJECT_NAMESPACE}::ortools)

# Create the native library
add_library(jni${JAVA_ARTIFACT} SHARED "")
set_target_properties(jni${JAVA_ARTIFACT} PROPERTIES
  POSITION_INDEPENDENT_CODE ON)
# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(jni${JAVA_ARTIFACT} PROPERTIES INSTALL_RPATH "@loader_path")
  # Xcode fails to build if library doesn't contains at least one source file.
  if(XCODE)
    file(GENERATE
      OUTPUT ${PROJECT_BINARY_DIR}/jni${JAVA_ARTIFACT}/version.cpp
      CONTENT "namespace {char* version = \"${PROJECT_VERSION}\";}")
    target_sources(jni${JAVA_ARTIFACT} PRIVATE ${PROJECT_BINARY_DIR}/jni${JAVA_ARTIFACT}/version.cpp)
  endif()
elseif(UNIX)
  set_target_properties(jni${JAVA_ARTIFACT} PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

set(JAVA_SRC_PATH src/main/java/${JAVA_DOMAIN_EXTENSION}/${JAVA_DOMAIN_NAME}/${JAVA_ARTIFACT})
set(JAVA_TEST_PATH src/test/java/${JAVA_DOMAIN_EXTENSION}/${JAVA_DOMAIN_NAME}/${JAVA_ARTIFACT})
set(JAVA_RESSOURCES_PATH src/main/resources)

#################
##  Java Test  ##
#################
# add_java_test()
# CMake function to generate and build java test.
# Parameters:
#  FILE_NAME: the Java filename
#  COMPONENT_NAME: name of the ortools/ subdir where the test is located
#  note: automatically determined if located in ortools/<component>/java/
# e.g.:
# add_java_test(
#   FILE_NAME
#     ${PROJECT_SOURCE_DIR}/ortools/foo/java/BarTest.java
#   COMPONENT_NAME
#     foo
# )
function(add_java_test)
  set(options "")
  set(oneValueArgs FILE_NAME COMPONENT_NAME)
  set(multiValueArgs "")
  cmake_parse_arguments(TEST
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
  if(NOT TEST_FILE_NAME)
    message(FATAL_ERROR "no FILE_NAME provided")
  endif()
  get_filename_component(TEST_NAME ${TEST_FILE_NAME} NAME_WE)

  message(STATUS "Configuring test ${TEST_FILE_NAME} ...")

  if(NOT TEST_COMPONENT_NAME)
    # test is located in ortools/<component_name>/java/
    get_filename_component(WRAPPER_DIR ${TEST_FILE_NAME} DIRECTORY)
    get_filename_component(COMPONENT_DIR ${WRAPPER_DIR} DIRECTORY)
    get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)
  else()
    set(COMPONENT_NAME ${TEST_COMPONENT_NAME})
  endif()

  set(JAVA_TEST_DIR ${PROJECT_BINARY_DIR}/java/${COMPONENT_NAME}/${TEST_NAME})
  message(STATUS "build path: ${JAVA_TEST_DIR}")

  add_custom_command(
    OUTPUT ${JAVA_TEST_DIR}/${JAVA_TEST_PATH}/${TEST_NAME}.java
    COMMAND ${CMAKE_COMMAND} -E make_directory
    ${JAVA_TEST_DIR}/${JAVA_TEST_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${TEST_FILE_NAME} ${JAVA_TEST_DIR}/${JAVA_TEST_PATH}/
    MAIN_DEPENDENCY ${TEST_FILE_NAME}
    VERBATIM
    )

  string(TOLOWER ${TEST_NAME} JAVA_TEST_PROJECT)
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/java/pom-test.xml.in
    ${JAVA_TEST_DIR}/pom.xml
    @ONLY)

  add_custom_command(
    OUTPUT ${JAVA_TEST_DIR}/timestamp
    COMMAND ${MAVEN_EXECUTABLE} compile -B
    COMMAND ${CMAKE_COMMAND} -E touch ${JAVA_TEST_DIR}/timestamp
    DEPENDS
    ${JAVA_TEST_DIR}/pom.xml
    ${JAVA_TEST_DIR}/${JAVA_TEST_PATH}/${TEST_NAME}.java
    java_package
    BYPRODUCTS
    ${JAVA_TEST_DIR}/target
    COMMENT "Compiling Java ${COMPONENT_NAME}/${TEST_NAME}.java (${JAVA_TEST_DIR}/timestamp)"
    WORKING_DIRECTORY ${JAVA_TEST_DIR})

  add_custom_target(java_${COMPONENT_NAME}_${TEST_NAME} ALL
    DEPENDS
    ${JAVA_TEST_DIR}/timestamp
    WORKING_DIRECTORY ${JAVA_TEST_DIR})

  if(BUILD_TESTING)
    add_test(
      NAME java_${COMPONENT_NAME}_${TEST_NAME}
      COMMAND ${MAVEN_EXECUTABLE} test
      WORKING_DIRECTORY ${JAVA_TEST_DIR})
  endif()
  message(STATUS "Configuring test ${TEST_FILE_NAME} ...DONE")
endfunction()

#####################
##  JAVA WRAPPERS  ##
#####################
list(APPEND CMAKE_SWIG_FLAGS "-I${PROJECT_SOURCE_DIR}")

# Swig wrap all libraries
foreach(SUBPROJECT IN ITEMS
 algorithms
 graph
 init
 linear_solver
 constraint_solver
 routing
 sat
 util)
  add_subdirectory(ortools/${SUBPROJECT}/java)
  target_link_libraries(jni${JAVA_ARTIFACT} PRIVATE jni${SUBPROJECT})
endforeach()
target_link_libraries(jni${JAVA_ARTIFACT} PRIVATE jnimodelbuilder)

#################################
##  Java Native Maven Package  ##
#################################
file(MAKE_DIRECTORY ${JAVA_NATIVE_PROJECT_DIR}/${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT})

configure_file(
  ${PROJECT_SOURCE_DIR}/ortools/java/pom-native.xml.in
  ${JAVA_NATIVE_PROJECT_DIR}/pom.xml
  @ONLY)

set(is_not_windows "$<NOT:$<PLATFORM_ID:Windows>>")

set(need_zlib_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_ZLIB}>>")

set(need_absl_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_absl}>>")

set(need_re2_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_re2}>>")

set(need_protobuf_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_Protobuf}>>")

set(need_coinutils_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_CoinUtils}>>")
set(need_osi_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_Osi}>>")
set(need_clp_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_Clp}>>")
set(need_cgl_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_Cgl}>>")
set(need_cbc_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_Cbc}>>")

set(need_highs_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_HIGHS}>>")

set(is_ortools_shared "$<STREQUAL:$<TARGET_PROPERTY:ortools,TYPE>,SHARED_LIBRARY>")

add_custom_command(
  OUTPUT ${JAVA_NATIVE_PROJECT_DIR}/timestamp
  COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:jni${JAVA_ARTIFACT}>
    $<${is_ortools_shared}:$<TARGET_SONAME_FILE:ortools>>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_zlib_lib},copy,true>
    $<${need_zlib_lib}:$<TARGET_SONAME_FILE:ZLIB::ZLIB>>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_absl_lib},copy,true>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::base>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::bad_any_cast_impl>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::bad_optional_access>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::bad_variant_access>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::city>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::civil_time>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::cord>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::cord_internal>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::cordz_functions>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::cordz_handle>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::cordz_info>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::crc32c>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::crc_cord_state>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::crc_cpu_detect>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::crc_internal>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::debugging_internal>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::decode_rust_punycode>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::demangle_internal>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::demangle_rust>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::die_if_null>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::examine_stack>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::exponential_biased>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_commandlineflag>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_commandlineflag_internal>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_config>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_internal>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_marshalling>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_parse>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_private_handle_accessor>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_program_name>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_reflection>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_usage>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::flags_usage_internal>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::graphcycles_internal>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::hash>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::hashtablez_sampler>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::int128>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::kernel_timeout_internal>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::leak_check>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::log_entry>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::log_flags>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::log_globals>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::log_initialize>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_check_op>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_conditions>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_fnmatch>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_format>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_globals>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_log_sink_set>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_message>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_nullguard>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::log_internal_proto>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::log_severity>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::log_sink>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::low_level_hash>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::malloc_internal>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::random_distributions>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::random_internal_platform>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::random_internal_pool_urbg>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::random_internal_randen>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::random_internal_randen_hwaes>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::random_internal_randen_hwaes_impl>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::random_internal_randen_slow>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::random_internal_seed_material>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::random_seed_gen_exception>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::random_seed_sequences>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::raw_hash_set>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::raw_logging_internal>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::spinlock_wait>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::stacktrace>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::status>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::statusor>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::str_format_internal>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::strerror>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::string_view>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::strings>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::strings_internal>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::symbolize>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::synchronization>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::throw_delegate>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::time>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::time_zone>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::utf8_for_code_point>>
    $<${need_absl_lib}:$<TARGET_SONAME_FILE:absl::vlog_config_internal>>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/

  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_re2_lib},copy,true>
    $<${need_re2_lib}:$<TARGET_SONAME_FILE:re2::re2>>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/

  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_protobuf_lib},copy,true>
    $<${need_protobuf_lib}:$<TARGET_SONAME_FILE:protobuf::libprotobuf>>
    $<${need_protobuf_lib}:$<TARGET_SONAME_FILE:utf8_validity>>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/

  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_coinutils_lib},copy,true>
    $<${need_coinutils_lib}:$<TARGET_SONAME_FILE:Coin::CoinUtils>>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_osi_lib},copy,true>
    $<${need_osi_lib}:$<TARGET_SONAME_FILE:Coin::Osi>>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_clp_lib},copy,true>
    $<${need_clp_lib}:$<TARGET_SONAME_FILE:Coin::Clp>>
    $<${need_clp_lib}:$<TARGET_SONAME_FILE:Coin::OsiClp>>
    $<${need_clp_lib}:$<TARGET_SONAME_FILE:Coin::ClpSolver>>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_cgl_lib},copy,true>
    $<${need_cgl_lib}:$<TARGET_SONAME_FILE:Coin::Cgl>>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_cbc_lib},copy,true>
    $<${need_cbc_lib}:$<TARGET_SONAME_FILE:Coin::Cbc>>
    $<${need_cbc_lib}:$<TARGET_SONAME_FILE:Coin::OsiCbc>>
    $<${need_cbc_lib}:$<TARGET_SONAME_FILE:Coin::CbcSolver>>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/

  COMMAND ${CMAKE_COMMAND} -E
    $<IF:${need_highs_lib},copy,true>
    $<${need_highs_lib}:$<TARGET_SONAME_FILE:highs>>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/

  COMMAND ${MAVEN_EXECUTABLE} compile -B
  COMMAND ${MAVEN_EXECUTABLE} package -B $<$<BOOL:${BUILD_FAT_JAR}>:-Dfatjar=true>
  COMMAND ${MAVEN_EXECUTABLE} install -B $<$<BOOL:${SKIP_GPG}>:-Dgpg.skip=true>
  COMMAND ${CMAKE_COMMAND} -E touch ${JAVA_NATIVE_PROJECT_DIR}/timestamp
  DEPENDS
    ${JAVA_NATIVE_PROJECT_DIR}/pom.xml
    Java${PROJECT_NAME}_proto
    jni${JAVA_ARTIFACT}
  BYPRODUCTS
    ${JAVA_NATIVE_PROJECT_DIR}/target
  COMMENT "Generate Java native package ${JAVA_NATIVE_PROJECT} (${JAVA_NATIVE_PROJECT_DIR}/timestamp)"
  WORKING_DIRECTORY ${JAVA_NATIVE_PROJECT_DIR})

add_custom_target(java_native_package
  DEPENDS
    ${JAVA_NATIVE_PROJECT_DIR}/timestamp
  WORKING_DIRECTORY ${JAVA_NATIVE_PROJECT_DIR})

add_custom_target(java_native_deploy
  COMMAND ${MAVEN_EXECUTABLE} deploy
  WORKING_DIRECTORY ${JAVA_NATIVE_PROJECT_DIR})
add_dependencies(java_native_deploy java_native_package)

##########################
##  Java Maven Package  ##
##########################
file(MAKE_DIRECTORY ${JAVA_PROJECT_DIR}/${JAVA_SRC_PATH})

if(UNIVERSAL_JAVA_PACKAGE)
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/java/pom-full.xml.in
    ${JAVA_PROJECT_DIR}/pom.xml
    @ONLY)
else()
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/java/pom-local.xml.in
    ${JAVA_PROJECT_DIR}/pom.xml
    @ONLY)
endif()

file(GLOB_RECURSE java_files RELATIVE ${PROJECT_SOURCE_DIR}/ortools/java
  "ortools/java/*.java")
#message(WARNING "list: ${java_files}")
set(JAVA_SRCS)
foreach(JAVA_FILE IN LISTS java_files)
  #message(STATUS "java: ${JAVA_FILE}")
  set(JAVA_OUT ${JAVA_PROJECT_DIR}/src/main/java/${JAVA_FILE})
  #message(STATUS "java out: ${JAVA_OUT}")
  add_custom_command(
    OUTPUT ${JAVA_OUT}
    COMMAND ${CMAKE_COMMAND} -E copy
      ${PROJECT_SOURCE_DIR}/ortools/java/${JAVA_FILE}
      ${JAVA_OUT}
    DEPENDS ${PROJECT_SOURCE_DIR}/ortools/java/${JAVA_FILE}
    COMMENT "Copy Java file ${JAVA_FILE}"
    VERBATIM)
  list(APPEND JAVA_SRCS ${JAVA_OUT})
endforeach()

add_custom_command(
  OUTPUT ${JAVA_PROJECT_DIR}/timestamp
  COMMAND ${MAVEN_EXECUTABLE} compile -B
  COMMAND ${MAVEN_EXECUTABLE} package -B $<$<BOOL:${BUILD_FAT_JAR}>:-Dfatjar=true>
  COMMAND ${MAVEN_EXECUTABLE} install -B $<$<BOOL:${SKIP_GPG}>:-Dgpg.skip=true>
  COMMAND ${CMAKE_COMMAND} -E touch ${JAVA_PROJECT_DIR}/timestamp
  DEPENDS
    ${JAVA_PROJECT_DIR}/pom.xml
    ${JAVA_SRCS}
    Java${PROJECT_NAME}_proto
    ${JAVA_NATIVE_PROJECT_DIR}/timestamp
    java_native_package
  BYPRODUCTS
    ${JAVA_PROJECT_DIR}/target
  COMMENT "Generate Java package ${JAVA_PROJECT} (${JAVA_PROJECT_DIR}/timestamp)"
  WORKING_DIRECTORY ${JAVA_PROJECT_DIR})

add_custom_target(java_package ALL
  DEPENDS
    ${JAVA_PROJECT_DIR}/timestamp
  WORKING_DIRECTORY ${JAVA_PROJECT_DIR})

add_custom_target(java_deploy
  COMMAND ${MAVEN_EXECUTABLE} deploy
  WORKING_DIRECTORY ${JAVA_PROJECT_DIR})
add_dependencies(java_deploy java_package)

###############
## Doc rules ##
###############
if(BUILD_JAVA_DOC)
  # add a target to generate API documentation with Doxygen
  find_package(Doxygen)
  if(DOXYGEN_FOUND)
    configure_file(${PROJECT_SOURCE_DIR}/ortools/java/Doxyfile.in ${PROJECT_BINARY_DIR}/java/Doxyfile @ONLY)
    file(DOWNLOAD
      https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/v2.1.0/doxygen-awesome.css
      ${PROJECT_BINARY_DIR}/java/doxygen-awesome.css
      SHOW_PROGRESS
    )
    add_custom_target(${PROJECT_NAME}_java_doc ALL
      #COMMAND ${CMAKE_COMMAND} -E rm -rf ${PROJECT_BINARY_DIR}/docs/java
      COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/docs/java
      COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/java/Doxyfile
      DEPENDS
        java_package
        ${PROJECT_BINARY_DIR}/java/Doxyfile
        ${PROJECT_BINARY_DIR}/java/doxygen-awesome.css
        ${PROJECT_SOURCE_DIR}/ortools/java/stylesheet.css
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      COMMENT "Generating Java API documentation with Doxygen"
      VERBATIM)
  else()
    message(WARNING "cmd `doxygen` not found, Java doc generation is disable!")
  endif()

  # Java doc
  find_program(JAR_PRG NAMES jar)
  if (JAR_PRG)
    file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/docs/javadoc)
    add_custom_target(${PROJECT_NAME}_javadoc_doc ALL
      #COMMAND ${CMAKE_COMMAND} -E rm -rf ${PROJECT_BINARY_DIR}/docs/javadoc
      #COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/docs/javadoc
      COMMAND jar xvf
      "${PROJECT_BINARY_DIR}/java/ortools-java/target/ortools-java-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}-javadoc.jar"
      DEPENDS
        java_package
        ${PROJECT_BINARY_DIR}/docs/javadoc
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/docs/javadoc
      COMMENT "Generating Java API documentation with Doxygen"
      VERBATIM)
  else()
    message(WARNING "cmd `jar` not found, Javadoc doc generation is disable!")
  endif()
endif()

###################
##  Java Sample  ##
###################
# add_java_sample()
# CMake function to generate and build java sample.
# Parameters:
#  FILE_NAME: the Java filename
#  COMPONENT_NAME: name of the ortools/ subdir where the test is located
#  note: automatically determined if located in ortools/<component>/samples/
# e.g.:
# add_java_sample(
#   FILE_NAME
#     ${PROJECT_SOURCE_DIR}/ortools/foo/sample/Bar.java
#   COMPONENT_NAME
#     foo
# )
function(add_java_sample)
  set(options "")
  set(oneValueArgs FILE_NAME COMPONENT_NAME)
  set(multiValueArgs "")
  cmake_parse_arguments(SAMPLE
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
  if(NOT SAMPLE_FILE_NAME)
    message(FATAL_ERROR "no FILE_NAME provided")
  endif()
  get_filename_component(SAMPLE_NAME ${SAMPLE_FILE_NAME} NAME_WE)

  message(STATUS "Configuring sample ${SAMPLE_FILE_NAME} ...")

  if(NOT SAMPLE_COMPONENT_NAME)
    # sample is located in ortools/<component_name>/sample/
    get_filename_component(SAMPLE_DIR ${SAMPLE_FILE_NAME} DIRECTORY)
    get_filename_component(COMPONENT_DIR ${SAMPLE_DIR} DIRECTORY)
    get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)
  else()
    set(COMPONENT_NAME ${SAMPLE_COMPONENT_NAME})
  endif()
  string(REPLACE "_" "" COMPONENT_NAME_LOWER ${COMPONENT_NAME})

  set(SAMPLE_DIR ${PROJECT_BINARY_DIR}/java/${COMPONENT_NAME}/${SAMPLE_NAME})
  message(STATUS "build path: ${SAMPLE_DIR}")

  add_custom_command(
    OUTPUT ${SAMPLE_DIR}/${JAVA_SRC_PATH}/${COMPONENT_NAME_LOWER}/samples/${SAMPLE_NAME}.java
    COMMAND ${CMAKE_COMMAND} -E make_directory
      ${SAMPLE_DIR}/${JAVA_SRC_PATH}/${COMPONENT_NAME_LOWER}/samples
    COMMAND ${CMAKE_COMMAND} -E copy ${SAMPLE_FILE_NAME} ${SAMPLE_DIR}/${JAVA_SRC_PATH}/${COMPONENT_NAME_LOWER}/samples/
      MAIN_DEPENDENCY ${SAMPLE_FILE_NAME}
    VERBATIM
  )

  string(TOLOWER ${SAMPLE_NAME} JAVA_SAMPLE_PROJECT)
  set(JAVA_MAIN_CLASS
    "${JAVA_PACKAGE}.${COMPONENT_NAME_LOWER}.samples.${SAMPLE_NAME}")
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/java/pom-sample.xml.in
    ${SAMPLE_DIR}/pom.xml
    @ONLY)

  add_custom_command(
    OUTPUT ${SAMPLE_DIR}/timestamp
    COMMAND ${MAVEN_EXECUTABLE} compile -B
    COMMAND ${CMAKE_COMMAND} -E touch ${SAMPLE_DIR}/timestamp
    DEPENDS
      ${SAMPLE_DIR}/pom.xml
      ${SAMPLE_DIR}/${JAVA_SRC_PATH}/${COMPONENT_NAME_LOWER}/samples/${SAMPLE_NAME}.java
      java_package
    BYPRODUCTS
      ${SAMPLE_DIR}/target
    COMMENT "Compiling Java sample ${COMPONENT_NAME}/${SAMPLE_NAME}.java (${SAMPLE_DIR}/timestamp)"
    WORKING_DIRECTORY ${SAMPLE_DIR})

  add_custom_target(java_${COMPONENT_NAME}_${SAMPLE_NAME} ALL
    DEPENDS
      ${SAMPLE_DIR}/timestamp
    WORKING_DIRECTORY ${SAMPLE_DIR})

  if(BUILD_TESTING)
    add_test(
      NAME java_${COMPONENT_NAME}_${SAMPLE_NAME}
      COMMAND ${MAVEN_EXECUTABLE} exec:java
      WORKING_DIRECTORY ${SAMPLE_DIR})
  endif()
  message(STATUS "Configuring sample ${SAMPLE_FILE_NAME} ...DONE")
endfunction()

####################
##  Java Example  ##
####################
# add_java_example()
# CMake function to generate and build java example.
# Parameters:
#  FILE_NAME: the Java filename
#  COMPONENT_NAME: name of the example/ subdir where the test is located
#  note: automatically determined if located in examples/<component>/
# e.g.:
# add_java_example(
#   FILE_NAME
#     ${PROJECT_SOURCE_DIR}/examples/foo/Bar.java
#   COMPONENT_NAME
#     foo
# )
function(add_java_example)
  set(options "")
  set(oneValueArgs FILE_NAME COMPONENT_NAME)
  set(multiValueArgs "")
  cmake_parse_arguments(EXAMPLE
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
if(NOT EXAMPLE_FILE_NAME)
    message(FATAL_ERROR "no FILE_NAME provided")
  endif()
  get_filename_component(EXAMPLE_NAME ${EXAMPLE_FILE_NAME} NAME_WE)

  message(STATUS "Configuring example ${EXAMPLE_FILE_NAME} ...")

  if(NOT EXAMPLE_COMPONENT_NAME)
    # sample is located in examples/<component_name>/
    get_filename_component(COMPONENT_DIR ${EXAMPLE_FILE_NAME} DIRECTORY)
    get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)
  else()
    set(COMPONENT_NAME ${EXAMPLE_COMPONENT_NAME})
  endif()

  set(JAVA_EXAMPLE_DIR ${PROJECT_BINARY_DIR}/java/${COMPONENT_NAME}/${EXAMPLE_NAME})
  message(STATUS "build path: ${JAVA_EXAMPLE_DIR}")

  add_custom_command(
    OUTPUT ${JAVA_EXAMPLE_DIR}/${JAVA_SRC_PATH}/${COMPONENT_NAME}/${EXAMPLE_NAME}.java
    COMMAND ${CMAKE_COMMAND} -E make_directory
      ${JAVA_EXAMPLE_DIR}/${JAVA_SRC_PATH}/${COMPONENT_NAME}
    COMMAND ${CMAKE_COMMAND} -E copy ${EXAMPLE_FILE_NAME} ${JAVA_EXAMPLE_DIR}/${JAVA_SRC_PATH}/${COMPONENT_NAME}/
      MAIN_DEPENDENCY ${EXAMPLE_FILE_NAME}
    VERBATIM
  )

  string(TOLOWER ${EXAMPLE_NAME} JAVA_SAMPLE_PROJECT)
  set(JAVA_MAIN_CLASS
    "${JAVA_PACKAGE}.${COMPONENT_NAME}.${EXAMPLE_NAME}")
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/java/pom-sample.xml.in
    ${JAVA_EXAMPLE_DIR}/pom.xml
    @ONLY)

  add_custom_command(
    OUTPUT ${JAVA_EXAMPLE_DIR}/timestamp
    COMMAND ${MAVEN_EXECUTABLE} compile -B
    COMMAND ${CMAKE_COMMAND} -E touch ${JAVA_EXAMPLE_DIR}/timestamp
    DEPENDS
      ${JAVA_EXAMPLE_DIR}/pom.xml
      ${JAVA_EXAMPLE_DIR}/${JAVA_SRC_PATH}/${COMPONENT_NAME}/${EXAMPLE_NAME}.java
      java_package
    BYPRODUCTS
      ${JAVA_EXAMPLE_DIR}/target
    COMMENT "Compiling Java ${COMPONENT_NAME}/${EXAMPLE_NAME}.java (${JAVA_EXAMPLE_DIR}/timestamp)"
    WORKING_DIRECTORY ${JAVA_EXAMPLE_DIR})

  add_custom_target(java_${COMPONENT_NAME}_${EXAMPLE_NAME} ALL
    DEPENDS
      ${JAVA_EXAMPLE_DIR}/timestamp
    WORKING_DIRECTORY ${JAVA_EXAMPLE_DIR})

  if(BUILD_TESTING)
    add_test(
      NAME java_${COMPONENT_NAME}_${EXAMPLE_NAME}
      COMMAND ${MAVEN_EXECUTABLE} exec:java
      WORKING_DIRECTORY ${JAVA_EXAMPLE_DIR})
  endif()
  message(STATUS "Configuring example ${EXAMPLE_FILE_NAME} ...DONE")
endfunction()
