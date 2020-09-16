function(get_patch_from_git VERSION_PATCH)
  find_package(Git QUIET)
  if(NOT GIT_FOUND)
    message(STATUS "Did not find git package.")
    set(PATCH 9999)
  else()
    # If no tags can be found, it is a git shallow clone
    execute_process(COMMAND
      ${GIT_EXECUTABLE} describe --tags
      RESULT_VARIABLE _OUTPUT_VAR
      OUTPUT_VARIABLE FULL
      ERROR_QUIET
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    if(NOT _OUTPUT_VAR)
      execute_process(COMMAND
        ${GIT_EXECUTABLE} rev-list HEAD --count
        RESULT_VARIABLE _OUTPUT_VAR
        OUTPUT_VARIABLE PATCH
        ERROR_QUIET
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
      STRING(STRIP PATCH ${PATCH})
      STRING(REGEX REPLACE "\n$" "" PATCH ${PATCH})
      STRING(REGEX REPLACE " " "" PATCH ${PATCH})
    else()
      message(STATUS "Did not find any tag.")
      set(PATCH 9999)
    endif()
  endif()
  set(${VERSION_PATCH} ${PATCH} PARENT_SCOPE)
endfunction()

function(set_version VERSION)
  # Get Major and Minor from Version.txt
  file(STRINGS "Version.txt" VERSION_STR)
  foreach(STR ${VERSION_STR})
    if(${STR} MATCHES "OR_TOOLS_MAJOR=(.*)")
      set(MAJOR ${CMAKE_MATCH_1})
    endif()
    if(${STR} MATCHES "OR_TOOLS_MINOR=(.*)")
      set(MINOR ${CMAKE_MATCH_1})
    endif()
  endforeach()

  # Compute Patch if .git is present otherwise set it to 9999
  get_filename_component(GIT_DIR ".git" ABSOLUTE)
  if(EXISTS ${GIT_DIR})
    get_patch_from_git(PATCH)
  else()
    set(PATCH 9999)
  endif()
  set(${VERSION} "${MAJOR}.${MINOR}.${PATCH}" PARENT_SCOPE)
endfunction()


# fetch_git_dependency()
#
# CMake function to download, build and install (in staging area) a dependency at configure
# time.
#
# Parameters:
# NAME: name of the dependency
# REPOSITORY: git url of the dependency
# TAG: tag of the dependency
# PATCH_COMMAND: apply patch
# SOURCE_SUBDIR: Path to source CMakeLists.txt relative to root dir
# CMAKE_ARGS: List of specific CMake args to add
#
# e.g.:
# fetch_git_dependency(
#   NAME
#     abseil-cpp
#   URL
#     https://github.com/abseil/abseil-cpp.git
#   TAG
#     master
#   PATCH_COMMAND
#     "git apply ${CMAKE_SOURCE_DIR}/patches/abseil-cpp.patch"
# )
function(fetch_git_dependency)
  set(options "")
  set(oneValueArgs NAME REPOSITORY TAG PATCH_COMMAND SOURCE_SUBDIR)
  set(multiValueArgs CMAKE_ARGS)
  cmake_parse_arguments(GIT_DEP
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
  message(STATUS "Building ${GIT_DEP_NAME}: ...")
  string(TOLOWER ${GIT_DEP_NAME} NAME_LOWER)

  if(GIT_DEP_PATCH_COMMAND)
    set(PATCH_CMD "${GIT_DEP_PATCH_COMMAND}")
  else()
    set(PATCH_CMD "")
  endif()
  configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt.in
    ${CMAKE_BINARY_DIR}/_deps/${NAME_LOWER}-subbuild/CMakeLists.txt @ONLY)

  execute_process(
    COMMAND ${CMAKE_COMMAND} -S. -Bproject_build -G "${CMAKE_GENERATOR}"
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/_deps/${NAME_LOWER}-subbuild)
  if(result)
    message(FATAL_ERROR "CMake step for ${GIT_DEP_NAME} failed: ${result}")
  endif()

  execute_process(
    COMMAND ${CMAKE_COMMAND} --build project_build --config ${CMAKE_BUILD_TYPE}
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/_deps/${NAME_LOWER}-subbuild)
  if(result)
    message(FATAL_ERROR "Build step for ${GIT_DEP_NAME} failed: ${result}")
  endif()

  if(GIT_DEP_SOURCE_SUBDIR)
    add_subdirectory(
      ${CMAKE_BINARY_DIR}/_deps/${NAME_LOWER}-src/${GIT_DEP_SOURCE_SUBDIR}
      ${CMAKE_BINARY_DIR}/_deps/${NAME_LOWER}-build)
  else()
    add_subdirectory(
      ${CMAKE_BINARY_DIR}/_deps/${NAME_LOWER}-src
      ${CMAKE_BINARY_DIR}/_deps/${NAME_LOWER}-build)
  endif()

  message(STATUS "Building ${GIT_DEP_NAME}: ...DONE")
endfunction()


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
  #message(FATAL_ERROR "component name: ${COMPONENT_NAME}")
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
      NAME java_${SAMPLE_NAME}
      COMMAND ${MAVEN_EXECUTABLE} exec:java
      WORKING_DIRECTORY ${SAMPLE_PATH})
  endif()

  message(STATUS "Building ${FILE_NAME}: ...DONE")
endfunction()

# add_java_example()
# CMake function to generate and build java sample.
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
      NAME java_${EXAMPLE_NAME}
      COMMAND ${MAVEN_EXECUTABLE} exec:java
      WORKING_DIRECTORY ${EXAMPLE_PATH})
  endif()

  message(STATUS "Building ${FILE_NAME}: ...DONE")
endfunction()

# add_java_test()
# CMake function to generate and build java sample.
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
      NAME java_${TEST_NAME}
      COMMAND ${MAVEN_EXECUTABLE} test
      WORKING_DIRECTORY ${TEST_PATH})
  endif()

  message(STATUS "Building ${FILE_NAME}: ...DONE")
endfunction()
