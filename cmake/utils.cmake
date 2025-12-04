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

function(get_patch_from_git VERSION_PATCH VERSION_MAJOR)
  find_package(Git QUIET)
  if(NOT GIT_FOUND)
    message(STATUS "Did not find git package.")
    set(PATCH 9999)
  else()
    # If no tags can be found, it is a git shallow clone or a new major
    execute_process(COMMAND
      ${GIT_EXECUTABLE} rev-list --count v${VERSION_MAJOR}.0..HEAD
      RESULT_VARIABLE RESULT_VAR
      OUTPUT_VARIABLE PATCH
      ERROR_QUIET
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    if(RESULT_VAR) # since 0 is success, need invert it
      message(STATUS "Did not be able to compute patch from 'v${VERSION_MAJOR}.0'.")
      execute_process(COMMAND
        ${GIT_EXECUTABLE} rev-parse --is-shallow-repository
        OUTPUT_VARIABLE IS_SHALLOW_VAR
        ERROR_QUIET
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      )
      if(${IS_SHALLOW_VAR} MATCHES "false")
        message(STATUS "Repo is not shallow, use 0 as patch.")
        set(PATCH 0)
      else()
        message(STATUS "Repo is shallow, use 9999 as patch.")
        set(PATCH 9999)
      endif()
    endif()
    STRING(STRIP PATCH ${PATCH})
    STRING(REGEX REPLACE "\n$" "" PATCH ${PATCH})
    STRING(REGEX REPLACE " " "" PATCH ${PATCH})
  endif()
  set(${VERSION_PATCH} ${PATCH} PARENT_SCOPE)
endfunction()

function(set_version VERSION RELEASE)
  if(DEFINED ENV{OR_TOOLS_MAJOR} AND DEFINED ENV{OR_TOOLS_MINOR})
    set(MAJOR $ENV{OR_TOOLS_MAJOR})
    set(MINOR $ENV{OR_TOOLS_MINOR})
  else()
    # Get Major and Minor from Version.txt
    file(STRINGS "Version.txt" VERSION_STR)
    set(IS_RELEASE TRUE)
    foreach(STR ${VERSION_STR})
      if(${STR} MATCHES "OR_TOOLS_MAJOR=(.*)")
        set(MAJOR ${CMAKE_MATCH_1})
      endif()
      if(${STR} MATCHES "OR_TOOLS_MINOR=(.*)")
        set(MINOR ${CMAKE_MATCH_1})
      endif()
      if(${STR} MATCHES "^RELEASE_CANDIDATE=YES\$")
        set(IS_RELEASE FALSE)
      endif()
    endforeach()
  endif()

  if(DEFINED ENV{OR_TOOLS_PATCH})
    set(PATCH $ENV{OR_TOOLS_PATCH})
  else()
    # Compute Patch if .git is present otherwise set it to 9999
    get_filename_component(GIT_DIR ".git" ABSOLUTE)
    if(EXISTS ${GIT_DIR})
      get_patch_from_git(PATCH ${MAJOR})
    else()
      set(PATCH 9999)
    endif()
  endif()
  set(${VERSION} "${MAJOR}.${MINOR}.${PATCH}" PARENT_SCOPE)
  set(${RELEASE} ${IS_RELEASE} PARENT_SCOPE)
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
#     main
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
