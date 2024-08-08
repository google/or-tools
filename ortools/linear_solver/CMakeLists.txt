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

file(GLOB _SRCS "*.h" "*.cc")
list(REMOVE_ITEM _SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/solve.cc
)
list(FILTER _SRCS EXCLUDE REGEX "/model_exporter_main\\.cc")
list(FILTER _SRCS EXCLUDE REGEX ".*/.*_test.cc")
if(USE_SCIP)
  list(APPEND _SRCS ${LPI_GLOP_SRC})
endif()

set(NAME ${PROJECT_NAME}_linear_solver)

# Will be merge in libortools.so
#add_library(${NAME} STATIC ${_SRCS})
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  )
target_include_directories(${NAME} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)
target_link_libraries(${NAME} PRIVATE
  absl::memory
  absl::strings
  absl::status
  absl::str_format
  protobuf::libprotobuf
  $<$<BOOL:${USE_COINOR}>:Coin::Cbc>
  $<$<BOOL:${USE_COINOR}>:Coin::Clp>
  $<$<BOOL:${USE_CPLEX}>:CPLEX::CPLEX>
  $<$<BOOL:${USE_GLPK}>:GLPK::GLPK>
  $<$<BOOL:${USE_HIGHS}>:highs::highs>
  $<$<BOOL:${USE_PDLP}>:Eigen3::Eigen>
  $<$<BOOL:${USE_SCIP}>:libscip>
  ${PROJECT_NAMESPACE}::ortools_proto)
#add_library(${PROJECT_NAMESPACE}::linear_solver ALIAS ${NAME})

# solve
add_executable(solve)
target_sources(solve PRIVATE "solve.cc")
target_include_directories(solve PRIVATE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
)
target_compile_features(solve PRIVATE cxx_std_17)
target_link_libraries(solve PRIVATE ${PROJECT_NAMESPACE}::ortools)

include(GNUInstallDirs)
if(APPLE)
  set_target_properties(solve PROPERTIES INSTALL_RPATH
    "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
elseif(UNIX)
  cmake_path(RELATIVE_PATH CMAKE_INSTALL_FULL_LIBDIR
             BASE_DIRECTORY ${CMAKE_INSTALL_FULL_BINDIR}
             OUTPUT_VARIABLE libdir_relative_path)
  set_target_properties(solve PROPERTIES INSTALL_RPATH
    "$ORIGIN/${libdir_relative_path}")
endif()

install(TARGETS solve
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(BUILD_TESTING)
  if (APPLE)
    set(CMAKE_INSTALL_RPATH
            "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
  elseif (UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN:$ORIGIN/../lib:$ORIGIN")
  endif ()

  # add_executable(test_xprs_interface xpress_interface_test.cc)
  # target_compile_features(test_xprs_interface PRIVATE cxx_std_17)
  # target_link_libraries(test_xprs_interface PRIVATE ortools::ortools GTest::gtest_main)

  # add_test(NAME cxx_unittests_xpress_interface COMMAND test_xprs_interface)

  if(USE_KNITRO)
    add_executable(test_knitro_interface knitro_interface_test.cc)
    target_compile_features(test_knitro_interface PRIVATE cxx_std_17)
    target_link_libraries(test_knitro_interface PRIVATE ortools::ortools GTest::gtest_main)

    add_test(NAME cxx_unittests_knitro_interface COMMAND test_knitro_interface)

    add_executable(test_knitro_interface_parallel knitro_interface_parallel_test.cc)
    target_compile_features(test_knitro_interface_parallel PRIVATE cxx_std_17)
    target_link_libraries(test_knitro_interface_parallel PRIVATE ortools::ortools GTest::gtest_main)

    # add_test(NAME cxx_unittests_knitro_interface COMMAND test_knitro_interface_parallel)
  endif()
endif ()
