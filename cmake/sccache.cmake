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

if(CMAKE_C_COMPILER_LAUNCHER OR CMAKE_CXX_COMPILER_LAUNCHER)
  message(STATUS "Configuring SCCache - skipped")
  return()
endif()

find_program(SCCACHE_PROGRAM sccache)
if(SCCACHE_PROGRAM)
  if(WIN32)
    set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE_PROGRAM})
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
    cmake_policy(SET CMP0141 NEW)
  else()
    # Set up wrapper scripts
    set(C_LAUNCHER   "${SCCACHE_PROGRAM}")
    set(CXX_LAUNCHER "${SCCACHE_PROGRAM}")

    file(WRITE "${CMAKE_BINARY_DIR}/launch-c"
      "#!/usr/bin/env sh\n"
      "# Xcode generator doesn't include the compiler as the\n"
      "# first argument, Ninja and Makefiles do. Handle both cases.\n"
      "if [ \"$1\" = \"${CMAKE_C_COMPILER}\" ]; then\n"
      "  shift\n"
      "fi\n"
      "exec \"${C_LAUNCHER}\" \"${CMAKE_C_COMPILER}\" \"$@\"\n"
    )
    file(WRITE "${CMAKE_BINARY_DIR}/launch-cxx"
      "#!/usr/bin/env sh\n"
      "# Xcode generator doesn't include the compiler as the\n"
      "# first argument, Ninja and Makefiles do. Handle both cases.\n"
      "if [ \"$1\" = \"${CMAKE_CXX_COMPILER}\" ]; then\n"
      "  shift\n"
      "fi\n"
      "exec \"${CXX_LAUNCHER}\" \"${CMAKE_CXX_COMPILER}\" \"$@\"\n"
    )
    file(CHMOD
      "${CMAKE_BINARY_DIR}/launch-c"
      "${CMAKE_BINARY_DIR}/launch-cxx"
      FILE_PERMISSIONS
      OWNER_READ OWNER_EXECUTE
      GROUP_READ GROUP_EXECUTE
      WORLD_READ WORLD_EXECUTE)

    if(CMAKE_GENERATOR STREQUAL "Xcode")
      # Set Xcode project attributes to route compilation and linking
      # through our scripts
      set(CMAKE_XCODE_ATTRIBUTE_CC         "${CMAKE_BINARY_DIR}/launch-c")
      set(CMAKE_XCODE_ATTRIBUTE_CXX        "${CMAKE_BINARY_DIR}/launch-cxx")
      set(CMAKE_XCODE_ATTRIBUTE_LD         "${CMAKE_BINARY_DIR}/launch-c")
      set(CMAKE_XCODE_ATTRIBUTE_LDPLUSPLUS "${CMAKE_BINARY_DIR}/launch-cxx")
    else()
      # Support Unix Makefiles and Ninja
      set(CMAKE_C_COMPILER_LAUNCHER   "${CMAKE_BINARY_DIR}/launch-c")
      set(CMAKE_CXX_COMPILER_LAUNCHER "${CMAKE_BINARY_DIR}/launch-cxx")
    endif()
  endif()
  message(STATUS "Configuring SCCache - done")
else()
  message(STATUS "Looking for SCCache - not found")
endif()
