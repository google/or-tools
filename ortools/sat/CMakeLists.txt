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

file(GLOB _SRCS "*.h" "*.cc" "python/linear_expr.*" "c_api/*.h" "c_api/*.cc")
list(FILTER _SRCS EXCLUDE REGEX ".*/.*_test.cc")
list(FILTER _SRCS EXCLUDE REGEX ".*/.*_fuzz.cc")
list(REMOVE_ITEM _SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/opb_reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/sat_cnf_reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/sat_runner.cc
)
set(NAME ${PROJECT_NAME}_sat)

# Will be merge in libortools.so
#add_library(${NAME} STATIC ${_SRCS})
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  )
target_include_directories(${NAME} PRIVATE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR})
target_link_libraries(${NAME} PRIVATE
  absl::any
  absl::memory
  absl::synchronization
  absl::strings
  absl::str_format
  protobuf::libprotobuf
  $<$<BOOL:${USE_COINOR}>:Coin::Cbc>
  ${PROJECT_NAMESPACE}::ortools_proto)
#add_library(${PROJECT_NAMESPACE}::sat ALIAS ${NAME})

if(BUILD_TESTING)
  file(GLOB _TEST_SRCS "*_test.cc")
  foreach(_FULL_FILE_NAME IN LISTS _TEST_SRCS)
    get_filename_component(_NAME ${_FULL_FILE_NAME} NAME_WE)
    get_filename_component(_FILE_NAME ${_FULL_FILE_NAME} NAME)
    ortools_cxx_test(
      NAME
        sat_${_NAME}
      SOURCES
        ${_FILE_NAME}
      LINK_LIBRARIES
        benchmark::benchmark
        GTest::gmock
        GTest::gtest_main
    )
  endforeach()
endif()

# Sat Runner
add_executable(sat_runner)
target_sources(sat_runner PRIVATE "sat_runner.cc")
target_include_directories(sat_runner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(sat_runner PRIVATE cxx_std_17)
target_link_libraries(sat_runner PRIVATE ${PROJECT_NAMESPACE}::ortools)

include(GNUInstallDirs)
if(APPLE)
  set_target_properties(sat_runner PROPERTIES INSTALL_RPATH
    "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
elseif(UNIX)
  cmake_path(RELATIVE_PATH CMAKE_INSTALL_FULL_LIBDIR
             BASE_DIRECTORY ${CMAKE_INSTALL_FULL_BINDIR}
             OUTPUT_VARIABLE libdir_relative_path)
  set_target_properties(sat_runner PROPERTIES
                        INSTALL_RPATH "$ORIGIN/${libdir_relative_path}")
endif()

install(TARGETS sat_runner)
