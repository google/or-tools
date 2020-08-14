if (NOT BUILD_CXX)
	return()
endif()

project(ortools_examples)

if (APPLE)
	set(CMAKE_INSTALL_RPATH
		"@loader_path/../..;@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
else()
	set(CMAKE_INSTALL_RPATH "$ORIGIN/../../:$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN/")
endif()

get_filename_component(PARENT_SOURCE_DIR ${PROJECT_SOURCE_DIR} DIRECTORY)

include(GNUInstallDirs)

foreach(EXECUTABLE
  constraint_programming_cp
  costas_array_sat
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
  golomb_sat
  integer_programming
  jobshop_sat
  knapsack
  linear_assignment_api
  linear_programming
  linear_solver_protocol_buffers
  magic_square_sat
  max_flow
  min_cost_flow
  mps_driver
  network_routing_sat
  nqueens
  pdptw
  random_tsp
  shift_minimization_sat
  slitherlink_sat
  solve
  sports_scheduling_sat
  stigler_diet
  strawberry_fields_with_column_generation
  uncapacitated_facility_location
  variable_intervals_sat
  weighted_tardiness_sat)
  add_executable(${EXECUTABLE} ${EXECUTABLE}.cc)
  target_include_directories(${EXECUTABLE} PUBLIC ${PARENT_SOURCE_DIR})
  target_compile_features(${EXECUTABLE} PRIVATE cxx_std_17)
  target_link_libraries(${EXECUTABLE} PRIVATE ortools::ortools)
  set_target_properties(${EXECUTABLE} PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE)
  install(TARGETS ${EXECUTABLE}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endforeach()

foreach(TEST
    costas_array_sat
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
    golomb_sat
    integer_programming
    #jobshop_sat
    knapsack
    linear_assignment_api
    linear_programming
    linear_solver_protocol_buffers
    magic_square_sat
    #mps_driver
    #network_routing_sat
    nqueens
    #pdptw
    #rcpsp_sat
    #shift_minimization_sat
    #solve
    #sports_scheduling_sat # Too long
    #stigler_diet
    #strawberry_fields_with_column_generation # Too long
    uncapacitated_facility_location
    #variable_intervals_sat
    #weighted_tardiness_sat
    )
  add_test(NAME cc_${TEST} COMMAND ${TEST})
endforeach()
