if (NOT BUILD_CXX)
  return()
endif()

project(ortools_examples)

if (APPLE)
  set(CMAKE_INSTALL_RPATH
    "@loader_path/../..;@loader_path/../lib;@loader_path")
else()
  set(CMAKE_INSTALL_RPATH "$ORIGIN/../../:$ORIGIN/../lib:$ORIGIN/")
endif()

set(_SRCS
  cvrptw_lib.cc
  fap_model_printer.cc
  fap_parser.cc
  fap_utilities.cc
  parse_dimacs_assignment.cc
  )

if(MSVC)
  add_library(${PROJECT_NAME} STATIC ${_SRCS})
else()
  add_library(${PROJECT_NAME} SHARED ${_SRCS})
endif()

get_filename_component(PARENT_SOURCE_DIR ${PROJECT_SOURCE_DIR} DIRECTORY)
get_filename_component(PARENT_SOURCE_DIR ${PARENT_SOURCE_DIR} DIRECTORY)
target_include_directories(${PROJECT_NAME} PUBLIC ${PARENT_SOURCE_DIR})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)
target_link_libraries(${PROJECT_NAME} PUBLIC ortools::ortools)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

foreach(EXECUTABLE
  costas_array
  cryptarithm
  cvrp_disjoint_tw
  cvrptw
  cvrptw_with_breaks
  cvrptw_with_refueling
  cvrptw_with_resources
  cvrptw_with_stop_times_and_resources
  dimacs_assignment
  dobble_ls
  flow_api
  frequency_assignment_problem
  golomb
  integer_programming
  jobshop
  jobshop_earlytardy
  jobshop_ls
  jobshop_sat
  linear_assignment_api
  linear_programming
  linear_solver_protocol_buffers
  ls_api
  magic_square
  model_util
  mps_driver
  multidim_knapsack
  network_routing
  nqueens
  pdptw
  shift_minimization_sat
  solve
  sports_scheduling
  strawberry_fields_with_column_generation
  tsp
  weighted_tardiness_sat)
  add_executable(${EXECUTABLE} ${EXECUTABLE}.cc)
  target_link_libraries(${EXECUTABLE} ${PROJECT_NAME})
  install(TARGETS ${EXECUTABLE}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endforeach()


foreach(TEST
    costas_array
    cryptarithm
    cvrp_disjoint_tw
    cvrptw
    #cvrptw_with_breaks # Too long
    #cvrptw_with_refueling # Too long
    cvrptw_with_resources
    cvrptw_with_stop_times_and_resources
    #dimacs_assignment
    #dobble_ls # Too long
    flow_api
    #frequency_assignment_problem
    golomb
    integer_programming
    #jobshop
    #jobshop_earlytardy
    #jobshop_ls
    #jobshop_sat
    linear_assignment_api
    linear_programming
    linear_solver_protocol_buffers
    ls_api
    magic_square
    #model_util
    mps_driver
    #multidim_knapsack
    #network_routing
    nqueens
    #pdptw
    #rcpsp_sat
    #shift_minimization_sat
    #solve
    #sports_scheduling # Too long
    #strawberry_fields_with_column_generation # Too long
    tsp
    #weighted_tardiness_sat
    )
  add_test(NAME cc_${TEST} COMMAND ${TEST})
endforeach()
