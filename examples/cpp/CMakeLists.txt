if(NOT BUILD_CXX_EXAMPLES)
  return()
endif()

file(GLOB CXX_SRCS "*.cc")
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/cvrptw_with_breaks.cc") # too long
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/cvrptw_with_refueling.cc") # too long
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/dimacs_assignment.cc")
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/dobble_ls.cc") # Too long
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/frequency_assignment_problem.cc")
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/jobshop_sat.cc")
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/mps_driver.cc")
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/network_routing_sat.cc")
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/pdptw.cc")
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/rcpsp_sat.cc")
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/shift_minimization_sat.cc")
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/solve.cc")
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/sports_scheduling_sat.cc") # Too long
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/stigler_diet.cc")
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/strawberry_fields_with_column_generation.cc") # Too long
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/variable_intervals_sat.cc")
#list(FILTER CXX_SRCS EXCLUDE REGEX ".*/weighted_tardiness_sat.cc")
foreach(SAMPLE IN LISTS CXX_SRCS)
  add_cxx_example(${SAMPLE})
endforeach()
