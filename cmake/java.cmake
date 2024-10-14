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

add_custom_command(
  OUTPUT ${JAVA_NATIVE_PROJECT_DIR}/timestamp
  COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:jni${JAVA_ARTIFACT}>
    $<$<NOT:$<PLATFORM_ID:Windows>>:$<TARGET_SONAME_FILE:${PROJECT_NAME}>>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:$<BOOL:${BUILD_absl}>,copy,true>
    $<TARGET_SONAME_FILE:absl::base>
    $<TARGET_SONAME_FILE:absl::city>
    $<TARGET_SONAME_FILE:absl::cord>
    $<TARGET_SONAME_FILE:absl::cord_internal>
    $<TARGET_SONAME_FILE:absl::cordz_functions>
    $<TARGET_SONAME_FILE:absl::cordz_handle>
    $<TARGET_SONAME_FILE:absl::cordz_info>
    $<TARGET_SONAME_FILE:absl::crc32c>
    $<TARGET_SONAME_FILE:absl::crc_cord_state>
    $<TARGET_SONAME_FILE:absl::crc_internal>
    $<TARGET_SONAME_FILE:absl::debugging_internal>
    $<TARGET_SONAME_FILE:absl::decode_rust_punycode>
    $<TARGET_SONAME_FILE:absl::demangle_internal>
    $<TARGET_SONAME_FILE:absl::demangle_rust>
    $<TARGET_SONAME_FILE:absl::die_if_null>
    $<TARGET_SONAME_FILE:absl::examine_stack>
    $<TARGET_SONAME_FILE:absl::exponential_biased>
    $<TARGET_SONAME_FILE:absl::flags_commandlineflag>
    $<TARGET_SONAME_FILE:absl::flags_commandlineflag_internal>
    $<TARGET_SONAME_FILE:absl::flags_config>
    $<TARGET_SONAME_FILE:absl::flags_internal>
    $<TARGET_SONAME_FILE:absl::flags_marshalling>
    $<TARGET_SONAME_FILE:absl::flags_parse>
    $<TARGET_SONAME_FILE:absl::flags_private_handle_accessor>
    $<TARGET_SONAME_FILE:absl::flags_program_name>
    $<TARGET_SONAME_FILE:absl::flags_reflection>
    $<TARGET_SONAME_FILE:absl::flags_usage>
    $<TARGET_SONAME_FILE:absl::flags_usage_internal>
    $<TARGET_SONAME_FILE:absl::hash>
    $<TARGET_SONAME_FILE:absl::int128>
    $<TARGET_SONAME_FILE:absl::kernel_timeout_internal>
    $<TARGET_SONAME_FILE:absl::leak_check>
    $<TARGET_SONAME_FILE:absl::log_flags>
    $<TARGET_SONAME_FILE:absl::log_globals>
    $<TARGET_SONAME_FILE:absl::log_initialize>
    $<TARGET_SONAME_FILE:absl::log_internal_check_op>
    $<TARGET_SONAME_FILE:absl::log_internal_conditions>
    $<TARGET_SONAME_FILE:absl::log_internal_fnmatch>
    $<TARGET_SONAME_FILE:absl::log_internal_format>
    $<TARGET_SONAME_FILE:absl::log_internal_globals>
    $<TARGET_SONAME_FILE:absl::log_internal_log_sink_set>
    $<TARGET_SONAME_FILE:absl::log_internal_message>
    $<TARGET_SONAME_FILE:absl::log_internal_nullguard>
    $<TARGET_SONAME_FILE:absl::log_internal_proto>
    $<TARGET_SONAME_FILE:absl::log_sink>
    $<TARGET_SONAME_FILE:absl::low_level_hash>
    $<TARGET_SONAME_FILE:absl::malloc_internal>
    $<TARGET_SONAME_FILE:absl::random_distributions>
    $<TARGET_SONAME_FILE:absl::random_internal_platform>
    $<TARGET_SONAME_FILE:absl::random_internal_pool_urbg>
    $<TARGET_SONAME_FILE:absl::random_internal_randen>
    $<TARGET_SONAME_FILE:absl::random_internal_randen_hwaes>
    $<TARGET_SONAME_FILE:absl::random_internal_randen_hwaes_impl>
    $<TARGET_SONAME_FILE:absl::random_internal_randen_slow>
    $<TARGET_SONAME_FILE:absl::random_internal_seed_material>
    $<TARGET_SONAME_FILE:absl::random_seed_gen_exception>
    $<TARGET_SONAME_FILE:absl::raw_hash_set>
    $<TARGET_SONAME_FILE:absl::raw_logging_internal>
    $<TARGET_SONAME_FILE:absl::spinlock_wait>
    $<TARGET_SONAME_FILE:absl::stacktrace>
    $<TARGET_SONAME_FILE:absl::status>
    $<TARGET_SONAME_FILE:absl::statusor>
    $<TARGET_SONAME_FILE:absl::str_format_internal>
    $<TARGET_SONAME_FILE:absl::strerror>
    $<TARGET_SONAME_FILE:absl::strings>
    $<TARGET_SONAME_FILE:absl::strings_internal>
    $<TARGET_SONAME_FILE:absl::symbolize>
    $<TARGET_SONAME_FILE:absl::synchronization>
    $<TARGET_SONAME_FILE:absl::throw_delegate>
    $<TARGET_SONAME_FILE:absl::time>
    $<TARGET_SONAME_FILE:absl::time_zone>
    $<TARGET_SONAME_FILE:absl::utf8_for_code_point>
    $<TARGET_SONAME_FILE:absl::vlog_config_internal>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:$<BOOL:${BUILD_Protobuf}>,copy,true>
    $<TARGET_SONAME_FILE:protobuf::libprotobuf>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:$<BOOL:${BUILD_re2}>,copy,true>
    $<TARGET_SONAME_FILE:re2::re2>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/

  COMMAND ${CMAKE_COMMAND} -E
    $<IF:$<BOOL:${BUILD_CoinUtils}>,copy,true>
    $<TARGET_SONAME_FILE:Coin::CoinUtils>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:$<BOOL:${BUILD_Osi}>,copy,true>
    $<TARGET_SONAME_FILE:Coin::Osi>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:$<BOOL:${BUILD_Clp}>,copy,true>
    $<TARGET_SONAME_FILE:Coin::Clp>
    $<TARGET_SONAME_FILE:Coin::OsiClp>
    $<TARGET_SONAME_FILE:Coin::ClpSolver>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:$<BOOL:${BUILD_Cgl}>,copy,true>
    $<TARGET_SONAME_FILE:Coin::Cgl>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/
  COMMAND ${CMAKE_COMMAND} -E
    $<IF:$<BOOL:${BUILD_Cbc}>,copy,true>
    $<TARGET_SONAME_FILE:Coin::Cbc>
    $<TARGET_SONAME_FILE:Coin::OsiCbc>
    $<TARGET_SONAME_FILE:Coin::CbcSolver>
    ${JAVA_RESSOURCES_PATH}/${JAVA_NATIVE_PROJECT}/

  COMMAND ${CMAKE_COMMAND} -E
    $<IF:$<BOOL:${BUILD_HIGHS}>,copy,true>
    $<TARGET_SONAME_FILE:highs>
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
