if(NOT BUILD_JAVA)
  return()
endif()

if(NOT TARGET ortools::ortools)
  message(FATAL_ERROR "Java: missing ortools::ortools TARGET")
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

# Find Java
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
set(JAVA_PACKAGE com.google.ortools)
set(JAVA_PACKAGE_PATH src/main/java/com/google/ortools)
set(JAVA_TEST_PATH src/test/java/com/google/ortools)
set(JAVA_RESOURCES_PATH src/main/resources)
if(APPLE)
  set(NATIVE_IDENTIFIER darwin)
elseif(UNIX)
  set(NATIVE_IDENTIFIER linux-x86-64)
elseif(WIN32)
  set(NATIVE_IDENTIFIER win32-x86-64)
else()
  message(FATAL_ERROR "Unsupported system !")
endif()
set(JAVA_NATIVE_PROJECT ortools-${NATIVE_IDENTIFIER})
set(JAVA_PROJECT ortools-java)

# CMake will remove all '-D' prefix (i.e. -DUSE_FOO become USE_FOO)
#get_target_property(FLAGS ortools::ortools COMPILE_DEFINITIONS)
set(FLAGS -DUSE_BOP -DUSE_GLOP -DABSL_MUST_USE_RESULT)
if(USE_SCIP)
  list(APPEND FLAGS "-DUSE_SCIP")
endif()
if(USE_COINOR)
  list(APPEND FLAGS "-DUSE_CBC" "-DUSE_CLP")
endif()
list(APPEND CMAKE_SWIG_FLAGS ${FLAGS} "-I${PROJECT_SOURCE_DIR}")

# Generate Protobuf java sources
set(PROTO_JAVAS)
file(GLOB_RECURSE proto_java_files RELATIVE ${PROJECT_SOURCE_DIR}
  "ortools/constraint_solver/*.proto"
  "ortools/linear_solver/*.proto"
  "ortools/sat/*.proto"
  "ortools/util/*.proto"
  )
list(REMOVE_ITEM proto_java_files "ortools/constraint_solver/demon_profiler.proto")
list(REMOVE_ITEM proto_java_files "ortools/constraint_solver/assignment.proto")
foreach(PROTO_FILE ${proto_java_files})
  #message(STATUS "protoc proto(java): ${PROTO_FILE}")
  get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
  string(REGEX REPLACE "_" "" PROTO_DIR ${PROTO_DIR})
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(PROTO_OUT ${PROJECT_BINARY_DIR}/java/${JAVA_PROJECT}/src/main/java/com/google/${PROTO_DIR})
  set(PROTO_JAVA ${PROTO_OUT}/${PROTO_NAME}.java)
  #message(STATUS "protoc java: ${PROTO_JAVA}")
  add_custom_command(
    OUTPUT ${PROTO_JAVA}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_OUT}
    COMMAND protobuf::protoc
    "--proto_path=${PROJECT_SOURCE_DIR}"
    ${PROTO_DIRS}
    "--java_out=${PROJECT_BINARY_DIR}/java/${JAVA_PROJECT}/src/main/java"
    ${PROTO_FILE}
    DEPENDS ${PROTO_FILE} protobuf::protoc
    COMMENT "Generate Java protocol buffer for ${PROTO_FILE}"
    VERBATIM)
  list(APPEND PROTO_JAVAS ${PROTO_JAVA})
endforeach()
add_custom_target(Java${PROJECT_NAME}_proto DEPENDS ${PROTO_JAVAS} ortools::ortools)

# Create the native library
add_library(jniortools SHARED "")
set_target_properties(jniortools PROPERTIES
  POSITION_INDEPENDENT_CODE ON)
# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(jniortools PROPERTIES INSTALL_RPATH "@loader_path")
  # Xcode fails to build if library doesn't contains at least one source file.
  if(XCODE)
    file(GENERATE
      OUTPUT ${PROJECT_BINARY_DIR}/jniortools/version.cpp
      CONTENT "namespace {char* version = \"${PROJECT_VERSION}\";}")
    target_sources(jniortools PRIVATE ${PROJECT_BINARY_DIR}/jniortools/version.cpp)
  endif()
elseif(UNIX)
  set_target_properties(jniortools PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

# Swig wrap all libraries
foreach(SUBPROJECT IN ITEMS algorithms graph linear_solver constraint_solver sat util)
  add_subdirectory(ortools/${SUBPROJECT}/java)
  target_link_libraries(jniortools PRIVATE jni${SUBPROJECT})
endforeach()

#################################
##  Java Native Maven Package  ##
#################################
configure_file(
  ${PROJECT_SOURCE_DIR}/ortools/java/pom-native.xml.in
  ${PROJECT_BINARY_DIR}/java/pom-native.xml.in
  @ONLY)

add_custom_command(
  OUTPUT java/${JAVA_NATIVE_PROJECT}/pom.xml
  DEPENDS ${PROJECT_BINARY_DIR}/java/pom-native.xml.in
  COMMAND ${CMAKE_COMMAND} -E make_directory ${JAVA_NATIVE_PROJECT}
  COMMAND ${CMAKE_COMMAND} -E copy ./pom-native.xml.in ${JAVA_NATIVE_PROJECT}/pom.xml
  BYPRODUCTS
  java/${JAVA_NATIVE_PROJECT}
  WORKING_DIRECTORY java)

add_custom_target(java_native_package
  DEPENDS
  java/${JAVA_NATIVE_PROJECT}/pom.xml
  COMMAND ${CMAKE_COMMAND} -E remove_directory src
  COMMAND ${CMAKE_COMMAND} -E make_directory ${JAVA_RESOURCES_PATH}/${NATIVE_IDENTIFIER}
  COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:jniortools>
    $<$<NOT:$<PLATFORM_ID:Windows>>:$<TARGET_SONAME_FILE:${PROJECT_NAME}>>
    ${JAVA_RESOURCES_PATH}/${NATIVE_IDENTIFIER}/
  COMMAND ${MAVEN_EXECUTABLE} compile
  COMMAND ${MAVEN_EXECUTABLE} package
  COMMAND ${MAVEN_EXECUTABLE} install
  WORKING_DIRECTORY java/${JAVA_NATIVE_PROJECT})

##########################
##  Java Maven Package  ##
##########################
configure_file(
  ${PROJECT_SOURCE_DIR}/ortools/java/pom-local.xml.in
  ${PROJECT_BINARY_DIR}/java/pom-local.xml.in
  @ONLY)

add_custom_command(
  OUTPUT java/${JAVA_PROJECT}/pom.xml
  DEPENDS ${PROJECT_BINARY_DIR}/java/pom-local.xml.in
  COMMAND ${CMAKE_COMMAND} -E make_directory ${JAVA_PROJECT}
  COMMAND ${CMAKE_COMMAND} -E copy ./pom-local.xml.in ${JAVA_PROJECT}/pom.xml
  BYPRODUCTS
  java/${JAVA_PROJECT}
  WORKING_DIRECTORY java)

add_custom_target(java_package ALL
  DEPENDS
  java/${JAVA_PROJECT}/pom.xml
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/ortools/java/com src/main/java/com
  COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/ortools/java/Loader.java ${JAVA_PACKAGE_PATH}/
  COMMAND ${MAVEN_EXECUTABLE} compile
  COMMAND ${MAVEN_EXECUTABLE} package
  COMMAND ${MAVEN_EXECUTABLE} install
  WORKING_DIRECTORY java/${JAVA_PROJECT})
add_dependencies(java_package java_native_package Java${PROJECT_NAME}_proto)

#################
##  Java Test  ##
#################
if(BUILD_TESTING)
  set(TEST_PATH ${PROJECT_BINARY_DIR}/java/tests/ortools-test)
  file(MAKE_DIRECTORY ${TEST_PATH}/${JAVA_TEST_PATH})

  file(COPY ${PROJECT_SOURCE_DIR}/ortools/java/CMakeTest.java
    DESTINATION ${TEST_PATH}/${JAVA_TEST_PATH})

  set(JAVA_TEST_PROJECT ortools-test)
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/java/pom-test.xml.in
    ${TEST_PATH}/pom.xml
    @ONLY)

  add_custom_target(java_test_Test ALL
    DEPENDS ${TEST_PATH}/pom.xml
    COMMAND ${MAVEN_EXECUTABLE} compile
    WORKING_DIRECTORY ${TEST_PATH})
  add_dependencies(java_test_Test java_package)

  add_test(
    NAME java_tests_Test
    COMMAND ${MAVEN_EXECUTABLE} test
    WORKING_DIRECTORY ${TEST_PATH})
endif()

# add_java_sample()
# CMake function to generate and build java sample.
# Parameters:
#  the java filename
# e.g.:
# add_java_sample(Foo.java)
function(add_java_sample FILE_NAME)
  message(STATUS "Building ${FILE_NAME}: ...")
  get_filename_component(SAMPLE_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(SAMPLE_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_DIR ${SAMPLE_DIR} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)
  string(REPLACE "_" "" COMPONENT_NAME_LOWER ${COMPONENT_NAME})

  set(SAMPLE_PATH ${PROJECT_BINARY_DIR}/java/${COMPONENT_NAME}/${SAMPLE_NAME})
  file(MAKE_DIRECTORY ${SAMPLE_PATH}/${JAVA_PACKAGE_PATH})

  file(COPY ${FILE_NAME} DESTINATION ${SAMPLE_PATH}/${JAVA_PACKAGE_PATH})

  string(TOLOWER ${SAMPLE_NAME} JAVA_SAMPLE_PROJECT)
  set(JAVA_MAIN_CLASS
    "${JAVA_PACKAGE}.${COMPONENT_NAME_LOWER}.samples.${SAMPLE_NAME}")
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/java/pom-sample.xml.in
    ${SAMPLE_PATH}/pom.xml
    @ONLY)

  add_custom_target(java_sample_${SAMPLE_NAME} ALL
    DEPENDS ${SAMPLE_PATH}/pom.xml
    COMMAND ${MAVEN_EXECUTABLE} compile
    WORKING_DIRECTORY ${SAMPLE_PATH})
  add_dependencies(java_sample_${SAMPLE_NAME} java_package)

  if(BUILD_TESTING)
    add_test(
      NAME java_${COMPONENT_NAME}_${SAMPLE_NAME}
      COMMAND ${MAVEN_EXECUTABLE} exec:java
      WORKING_DIRECTORY ${SAMPLE_PATH})
  endif()

  message(STATUS "Building ${FILE_NAME}: ...DONE")
endfunction()

# add_java_example()
# CMake function to generate and build java example.
# Parameters:
#  the java filename
# e.g.:
# add_java_example(Foo.java)
function(add_java_example FILE_NAME)
  message(STATUS "Building ${FILE_NAME}: ...")
  get_filename_component(EXAMPLE_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(COMPONENT_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  set(EXAMPLE_PATH ${PROJECT_BINARY_DIR}/java/${COMPONENT_NAME}/${EXAMPLE_NAME})
  file(MAKE_DIRECTORY ${EXAMPLE_PATH}/${JAVA_PACKAGE_PATH})

  file(COPY ${FILE_NAME} DESTINATION ${EXAMPLE_PATH}/${JAVA_PACKAGE_PATH})

  string(TOLOWER ${EXAMPLE_NAME} JAVA_SAMPLE_PROJECT)
  set(JAVA_MAIN_CLASS
    "${JAVA_PACKAGE}.${COMPONENT_NAME}.${EXAMPLE_NAME}")
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/java/pom-sample.xml.in
    ${EXAMPLE_PATH}/pom.xml
    @ONLY)

  add_custom_target(java_example_${EXAMPLE_NAME} ALL
    DEPENDS ${EXAMPLE_PATH}/pom.xml
    COMMAND ${MAVEN_EXECUTABLE} compile
    WORKING_DIRECTORY ${EXAMPLE_PATH})
  add_dependencies(java_example_${EXAMPLE_NAME} java_package)

  if(BUILD_TESTING)
    add_test(
      NAME java_${COMPONENT_NAME}_${EXAMPLE_NAME}
      COMMAND ${MAVEN_EXECUTABLE} exec:java
      WORKING_DIRECTORY ${EXAMPLE_PATH})
  endif()

  message(STATUS "Building ${FILE_NAME}: ...DONE")
endfunction()

# add_java_test()
# CMake function to generate and build java test.
# Parameters:
#  the java filename
# e.g.:
# add_java_test(Foo.java)
function(add_java_test FILE_NAME)
  message(STATUS "Building ${FILE_NAME}: ...")
  get_filename_component(TEST_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(COMPONENT_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  set(TEST_PATH ${PROJECT_BINARY_DIR}/java/${COMPONENT_NAME}/${TEST_NAME})
  file(MAKE_DIRECTORY ${TEST_PATH}/${JAVA_TEST_PATH})

  file(COPY ${FILE_NAME} DESTINATION ${TEST_PATH}/${JAVA_TEST_PATH})

  string(TOLOWER ${TEST_NAME} JAVA_TEST_PROJECT)
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/java/pom-test.xml.in
    ${TEST_PATH}/pom.xml
    @ONLY)

  add_custom_target(java_sample_${TEST_NAME} ALL
    DEPENDS ${TEST_PATH}/pom.xml
    COMMAND ${MAVEN_EXECUTABLE} compile
    WORKING_DIRECTORY ${TEST_PATH})
  add_dependencies(java_sample_${TEST_NAME} java_package)

  if(BUILD_TESTING)
    add_test(
      NAME java_tests_${TEST_NAME}
      COMMAND ${MAVEN_EXECUTABLE} test
      WORKING_DIRECTORY ${TEST_PATH})
  endif()

  message(STATUS "Building ${FILE_NAME}: ...DONE")
endfunction()
