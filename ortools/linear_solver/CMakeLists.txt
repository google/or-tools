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

list(APPEND _SRCS
  linear_expr.cc
  linear_expr.h
  linear_solver_callback.cc
  linear_solver_callback.h
  linear_solver.cc
  linear_solver.h
  model_exporter_swig_helper.h
  model_exporter.cc
  model_exporter.h
  model_validator.cc
  model_validator.h
  solve_mp_model.cc
  solve_mp_model.h
  sat_interface.cc
)

if(USE_BOP)
  list(APPEND _SRCS bop_interface.cc)
endif()

if(USE_COINOR)
  list(APPEND _SRCS cbc_interface.cc clp_interface.cc)
endif()

if(USE_CPLEX)
  list(APPEND _SRCS cplex_interface.cc)
endif()

if(USE_GLOP)
  list(APPEND _SRCS glop_interface.cc glop_utils.cc glop_utils.h)
endif()

if(USE_GUROBI)
  list(APPEND _SRCS gurobi_util.h gurobi_interface.cc gurobi_util.cc)
endif()

if(USE_GLPK)
  list(APPEND _SRCS glpk_interface.cc)
endif()

if(USE_HIGHS)
  list(APPEND _SRCS highs_interface.cc)
endif()

if(USE_PDLP)
  list(APPEND _SRCS pdlp_interface.cc)
endif()

if(USE_SCIP)
  list(APPEND _SRCS
    scip_callback.cc
    scip_callback.h
    scip_helper_macros.h
    scip_interface.cc
    ${LPI_GLOP_SRC})
endif()

if(USE_XPRESS)
  list(APPEND _SRCS xpress_interface.cc)
endif()

set(NAME ${PROJECT_NAME}_linear_solver)

# Will be merge in libortools.so
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC AND BUILD_SHARED_LIBS)
  target_compile_definitions(${NAME} PUBLIC "OR_BUILD_DLL")
  target_compile_definitions(${NAME} PRIVATE "OR_EXPORT")
endif()
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
  $<$<BOOL:${USE_SCIP}>:SCIP::libscip>
  ${PROJECT_NAMESPACE}::ortools_proto)

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

  if(USE_XPRESS)
    add_executable(test_xprs_interface xpress_interface_test.cc)
    target_compile_features(test_xprs_interface PRIVATE cxx_std_17)
    target_link_libraries(test_xprs_interface PRIVATE ortools::ortools GTest::gtest_main)

    add_test(NAME cxx_unittests_xpress_interface COMMAND test_xprs_interface)
  endif()
endif ()
