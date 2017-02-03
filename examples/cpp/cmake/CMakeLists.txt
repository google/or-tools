#TODO(include this once)
include_directories(${SRC_DIR} ${GEN_DIR} ${DEPENDENCIES_INCLUDE})

#add_executable(golomb ${EXAMPLES_DIR}/cpp/golomb.cc)
#target_link_libraries(golomb ortools)

#file(GLOB routing_examples ${CPP_EXAMPLES_DIR}/routing*.cc)

file(GLOB ortools_examples "${CPP_EXAMPLES_DIR}/*.cc")
file(GLOB fap_src "${CPP_EXAMPLES_DIR}/fap*.cc")

list(REMOVE_ITEM ortools_examples ${fap_src})
list(REMOVE_ITEM ortools_examples ${CPP_EXAMPLES_DIR}/dimacs_assignment.cc ${CPP_EXAMPLES_DIR}/frequency_assignment_problem.cc)

#TODO(remove this when the example works)
list(REMOVE_ITEM ortools_examples ${CPP_EXAMPLES_DIR}/acp_challenge.cc)

foreach(example ${ortools_examples})
  #get the example's name without extension
  get_filename_component(example_we ${example} NAME_WE)
  add_executable(${example_we} ${example})
  target_link_libraries(${example_we} ${ortools})
  #TODO(add tests)
endforeach(example)
