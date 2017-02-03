#TODO(include this once)
include_directories(${SRC_DIR} ${GEN_DIR} ${DEPENDENCIES_INCLUDE} ${EX_DIR})

file(GLOB cvrptw_examples "${CPP_EX_DIR}/cvrp*.cc")
list(REMOVE_ITEM cvrptw_examples ${CPP_EX_DIR}/cvrptw_lib.cc)

file(GLOB ortools_examples "${CPP_EX_DIR}/*.cc")
#remove cvrptw sources & examples
list(REMOVE_ITEM ortools_examples ${CPP_EX_DIR}/cvrptw_lib.cc)
list(REMOVE_ITEM ortools_examples ${cvrptw_examples})
#remove fap sources & examples
list(REMOVE_ITEM ortools_examples ${fap_src})
list(REMOVE_ITEM ortools_examples ${CPP_EX_DIR}/frequency_assignment_problem.cc)
#remove dimacs sources & examples
list(REMOVE_ITEM ortools_examples ${CPP_EX_DIR}/parse_dimacs_assignment.cc ${CPP_EX_DIR}/print_dimacs_assignment.cc)
list(REMOVE_ITEM ortools_examples ${CPP_EX_DIR}/dimacs_assignment.cc)

#TODO(remove this when the example works)
list(REMOVE_ITEM ortools_examples ${CPP_EX_DIR}/acp_challenge.cc)
list(REMOVE_ITEM ortools_examples ${CPP_EX_DIR}/global_arith.cc)

#Compile or-tools examples
foreach(example ${ortools_examples})
  #get the example's name without extension
  get_filename_component(example_we ${example} NAME_WE)
  add_executable(${example_we} ${example})
  target_link_libraries(${example_we} ortools)
  #TODO(add tests)
endforeach(example)

#compile cvrptw examples
foreach(example ${cvrptw_examples})
  #get the example's name without extension
  get_filename_component(example_we ${example} NAME_WE)
  add_executable(${example_we} ${example})
  target_link_libraries(${example_we} cvrptw_lib)
  #TODO(add tests)
endforeach(example)

#compile fap examples
add_executable(frequency_assignment_problem ${CPP_EX_DIR}/frequency_assignment_problem.cc)
target_link_libraries(frequency_assignment_problem fap)

#compile dimacs examples
add_executable(dimacs_assignment ${CPP_EX_DIR}/dimacs_assignment.cc)
target_link_libraries(dimacs_assignment dimacs)

