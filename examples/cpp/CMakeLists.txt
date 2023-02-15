if(NOT BUILD_CXX_EXAMPLES)
  return()
endif()

#file(GLOB_RECURSE proto_files RELATIVE ${PROJECT_SOURCE_DIR} "*.proto")
#foreach(PROTO_FILE IN LISTS proto_files)
#  message(STATUS "protoc proto(cc): ${PROTO_FILE}")
#  get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
#  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
#  set(PROTO_HDR ${PROJECT_BINARY_DIR}/${PROTO_DIR}/${PROTO_NAME}.pb.h)
#  set(PROTO_SRC ${PROJECT_BINARY_DIR}/${PROTO_DIR}/${PROTO_NAME}.pb.cc)
#  message(STATUS "protoc hdr: ${PROTO_HDR}")
#  message(STATUS "protoc src: ${PROTO_SRC}")
#  add_custom_command(
#    OUTPUT ${PROTO_SRC} ${PROTO_HDR}
#    COMMAND ${PROTOC_PRG}
#    "--proto_path=${PROJECT_SOURCE_DIR}"
#    ${PROTO_DIRS}
#    "--cpp_out=${PROJECT_BINARY_DIR}"
#    ${PROTO_FILE}
#    DEPENDS ${PROTO_FILE} ${PROTOC_PRG}
#    COMMENT "Generate C++ protocol buffer for ${PROTO_FILE}"
#    VERBATIM)
#  list(APPEND PROTO_HDRS ${PROTO_HDR})
#  list(APPEND PROTO_SRCS ${PROTO_SRC})
#endforeach()

file(GLOB CXX_SRCS "*.cc")
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/binpacking_2d_sat.cc")
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/course_scheduling_run.cc") # missing proto
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/course_scheduling.cc") # missing proto
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/cvrptw_with_breaks.cc") # too long
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/cvrptw_with_refueling.cc") # too long
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/dimacs_assignment.cc") # crash
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/dobble_ls.cc") # Too long
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/frequency_assignment_problem.cc") # crash
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/jobshop_sat.cc") # crash
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/knapsack_2d_sat.cc")
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/mps_driver.cc") # crash
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/multi_knapsack_sat.cc") # crash
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/network_routing_sat.cc")
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/pdlp_solve.cc")
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/pdptw.cc")
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/sat_runner.cc")
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/shift_minimization_sat.cc")
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/pdlp_solve.cc")
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/solve.cc")
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/sports_scheduling_sat.cc") # Too long
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/strawberry_fields_with_column_generation.cc") # Too long
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/vector_bin_packing_solver.cc")
list(FILTER CXX_SRCS EXCLUDE REGEX ".*/weighted_tardiness_sat.cc")
foreach(SAMPLE IN LISTS CXX_SRCS)
  add_cxx_example(${SAMPLE})
endforeach()

if (USE_SIRIUS)
	add_test(NAME "cc_strawberry_fields_with_column_generation_glop" COMMAND strawberry_fields_with_column_generation -colgen_solver glop)
	add_test(NAME "cc_strawberry_fields_with_column_generation_clp" COMMAND strawberry_fields_with_column_generation -colgen_solver clp)
	add_test(NAME "cc_strawberry_fields_with_column_generation_sirius" COMMAND strawberry_fields_with_column_generation -colgen_solver sirius)
endif()

if (USE_CPLEX)
	add_test(NAME "cc_strawberry_fields_with_column_generation_cplex" COMMAND strawberry_fields_with_column_generation -colgen_solver cplex)
endif()
# Sat Runner
include(GNUInstallDirs)
if(APPLE)
  set(CMAKE_INSTALL_RPATH
    "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
elseif(UNIX)
  set(CMAKE_INSTALL_RPATH
    "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN/../lib64:$ORIGIN/../lib:$ORIGIN")
endif()

add_executable(sat_runner)
target_sources(sat_runner PRIVATE
 "sat_runner.cc"
 "opb_reader.h"
 "sat_cnf_reader.h")
target_include_directories(sat_runner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(sat_runner PRIVATE cxx_std_17)
target_link_libraries(sat_runner PRIVATE ${PROJECT_NAMESPACE}::ortools)

install(TARGETS sat_runner)

