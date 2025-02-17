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

if(NOT BUILD_SAMPLES AND NOT BUILD_GLOP)
  return()
endif()


if(BUILD_GLOP)
  if(APPLE)
    set(CMAKE_INSTALL_RPATH
      "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
  elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN")
  endif()

  add_executable(simple_glop_program simple_glop_program.cc)
  target_include_directories(simple_glop_program PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(simple_glop_program PRIVATE cxx_std_17)
  target_link_libraries(simple_glop_program PRIVATE glop)

  install(TARGETS simple_glop_program)

  if(BUILD_TESTING)
    add_test(NAME cxx_simple_glop_program COMMAND simple_glop_program)
  endif()
endif()

if(BUILD_CXX_SAMPLES)
  file(GLOB CXX_SRCS "*.cc")
  foreach(SAMPLE IN LISTS CXX_SRCS)
    add_cxx_sample(FILE_NAME ${SAMPLE})
  endforeach()
endif()
