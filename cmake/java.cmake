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
  set(NATIVE_IDENTIFIER darwin-x86-64)
elseif(UNIX)
  set(NATIVE_IDENTIFIER linux-x86-64)
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

# CMake will remove all '-D' prefix (i.e. -DUSE_FOO become USE_FOO)
#get_target_property(FLAGS ${PROJECT_NAMESPACE}::ortools COMPILE_DEFINITIONS)
set(FLAGS -DUSE_BOP -DUSE_GLOP -DABSL_MUST_USE_RESULT)
if(USE_COINOR)
  list(APPEND FLAGS "-DUSE_CBC" "-DUSE_CLP")
endif()
if(USE_GLPK)
  list(APPEND FLAGS "-DUSE_GLPK")
endif()
if(USE_SCIP)
  list(APPEND FLAGS "-DUSE_SCIP")
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

# Swig wrap all libraries
set(JAVA_SRC_PATH src/main/java/${JAVA_DOMAIN_EXTENSION}/${JAVA_DOMAIN_NAME}/${JAVA_ARTIFACT})
set(JAVA_TEST_PATH src/test/java/${JAVA_DOMAIN_EXTENSION}/${JAVA_DOMAIN_NAME}/${JAVA_ARTIFACT})
set(JAVA_RESSOURCES_PATH src/main/resources)
foreach(SUBPROJECT IN ITEMS algorithms graph init linear_solver constraint_solver sat util)
  add_subdirectory(ortools/${SUBPROJECT}/java)
  target_link_libraries(jni${JAVA_ARTIFACT} PRIVATE jni${SUBPROJECT})
endforeach()

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
  COMMAND ${MAVEN_EXECUTABLE} compile -B
  COMMAND ${MAVEN_EXECUTABLE} package -B $<$<BOOL:${BUILD_FAT_JAR}>:-Dfatjar=true>
  COMMAND ${MAVEN_EXECUTABLE} install -B $<$<BOOL:${SKIP_GPG}>:-Dgpg.skip=true>
  COMMAND ${CMAKE_COMMAND} -E touch ${JAVA_NATIVE_PROJECT_DIR}/timestamp
  DEPENDS
    ${JAVA_NATIVE_PROJECT_DIR}/pom.xml
  BYPRODUCTS
    ${JAVA_NATIVE_PROJECT_DIR}/target
  COMMENT "Generate Java native package ${JAVA_NATIVE_PROJECT} (${JAVA_NATIVE_PROJECT_DIR}/timestamp)"
  WORKING_DIRECTORY ${JAVA_NATIVE_PROJECT_DIR})

add_custom_target(java_native_package
  DEPENDS
    ${JAVA_NATIVE_PROJECT_DIR}/timestamp
  WORKING_DIRECTORY ${JAVA_NATIVE_PROJECT_DIR})

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
  set(JAVA_OUT ${JAVA_PROJECT_DIR}/${JAVA_SRC_PATH}/${JAVA_FILE})
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
  java_native_package
  BYPRODUCTS
    ${JAVA_PROJECT_DIR}/target
  COMMENT "Generate Java package ${JAVA_PROJECT} (${JAVA_PROJECT_DIR}/timestamp)"
  WORKING_DIRECTORY ${JAVA_PROJECT_DIR})

add_custom_target(java_package ALL
  DEPENDS
    ${JAVA_PROJECT_DIR}/timestamp
  WORKING_DIRECTORY ${JAVA_PROJECT_DIR})

#################
##  Java Test  ##
#################
# add_java_test()
# CMake function to generate and build java test.
# Parameters:
#  the java filename
# e.g.:
# add_java_test(FooTests.java)
function(add_java_test FILE_NAME)
  message(STATUS "Configuring test ${FILE_NAME}: ...")
  get_filename_component(TEST_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(COMPONENT_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  set(JAVA_TEST_DIR ${PROJECT_BINARY_DIR}/java/${COMPONENT_NAME}/${TEST_NAME})
  message(STATUS "build path: ${JAVA_TEST_DIR}")

  add_custom_command(
    OUTPUT ${JAVA_TEST_DIR}/${JAVA_TEST_PATH}/${TEST_NAME}.java
    COMMAND ${CMAKE_COMMAND} -E make_directory
      ${JAVA_TEST_DIR}/${JAVA_TEST_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy
      ${FILE_NAME}
      ${JAVA_TEST_DIR}/${JAVA_TEST_PATH}/
    MAIN_DEPENDENCY ${FILE_NAME}
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
  message(STATUS "Configuring test ${FILE_NAME}: ...DONE")
endfunction()

###################
##  Java Sample  ##
###################
# add_java_sample()
# CMake function to generate and build java sample.
# Parameters:
#  the java filename
# e.g.:
# add_java_sample(Foo.java)
function(add_java_sample FILE_NAME)
  message(STATUS "Configuring sample ${FILE_NAME}: ...")
  get_filename_component(SAMPLE_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(SAMPLE_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_DIR ${SAMPLE_DIR} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)
  string(REPLACE "_" "" COMPONENT_NAME_LOWER ${COMPONENT_NAME})

  set(SAMPLE_DIR ${PROJECT_BINARY_DIR}/java/${COMPONENT_NAME}/${SAMPLE_NAME})
  message(STATUS "build path: ${SAMPLE_DIR}")

  add_custom_command(
    OUTPUT ${SAMPLE_DIR}/${JAVA_SRC_PATH}/${COMPONENT_NAME_LOWER}/samples/${SAMPLE_NAME}.java
    COMMAND ${CMAKE_COMMAND} -E make_directory
      ${SAMPLE_DIR}/${JAVA_SRC_PATH}/${COMPONENT_NAME_LOWER}/samples
    COMMAND ${CMAKE_COMMAND} -E copy
      ${FILE_NAME}
      ${SAMPLE_DIR}/${JAVA_SRC_PATH}/${COMPONENT_NAME_LOWER}/samples/
    MAIN_DEPENDENCY ${FILE_NAME}
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
  message(STATUS "Configuring sample ${FILE_NAME}: ...DONE")
endfunction()

####################
##  Java Example  ##
####################
# add_java_example()
# CMake function to generate and build java example.
# Parameters:
#  the java filename
# e.g.:
# add_java_example(Foo.java)
function(add_java_example FILE_NAME)
  message(STATUS "Configuring example ${FILE_NAME}: ...")
  get_filename_component(EXAMPLE_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(COMPONENT_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  set(JAVA_EXAMPLE_DIR ${PROJECT_BINARY_DIR}/java/${COMPONENT_NAME}/${EXAMPLE_NAME})
  message(STATUS "build path: ${JAVA_EXAMPLE_DIR}")

  add_custom_command(
    OUTPUT ${JAVA_EXAMPLE_DIR}/${JAVA_SRC_PATH}/${COMPONENT_NAME}/${EXAMPLE_NAME}.java
    COMMAND ${CMAKE_COMMAND} -E make_directory
      ${JAVA_EXAMPLE_DIR}/${JAVA_SRC_PATH}/${COMPONENT_NAME}
    COMMAND ${CMAKE_COMMAND} -E copy
      ${FILE_NAME}
      ${JAVA_EXAMPLE_DIR}/${JAVA_SRC_PATH}/${COMPONENT_NAME}/
    MAIN_DEPENDENCY ${FILE_NAME}
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
  message(STATUS "Configuring example ${FILE_NAME}: ...DONE")
endfunction()
