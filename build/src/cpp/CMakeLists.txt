include(ortools_protobuf.cmake)

#TODO(include this once)
include_directories(${SRC_DIR} ${GEN_DIR} ${DEPENDENCIES_INCLUDE} ${EX_DIR})

set(LINKER_LANGUAGE CXX)
if(UNIX)
    set(CMAKE_CXX_COMPILER "g++" CACHE INTERNAL "" FORCE)
    #TODO(verify if we can always compile with -DUSE_CBC)
    set(CMAKE_CXX_FLAGS "-fPIC -std=c++0x -fwrapv -DARCH_K8 -Wno-deprecated -DUSE_CBC -DUSE_CLP -DUSE_GLOP -DUSE_BOP" CACHE INTERNAL "" FORCE)
    set(CMAKE_CXX_FLAGS_DEBUG "-DNDEBUG" CACHE INTERNAL "" FORCE)
endif()

file(GLOB flatzinc_src "${SRC_DIR}/flatzinc/*.cc" CACHE INTERNAL "" FORCE)
file (GLOB_RECURSE gen_src "${GEN_DIR}/*.cc")

file(GLOB_RECURSE ortools_src "${SRC_DIR}/*.cc")
list(REMOVE_ITEM ortools_src ${flatzinc_src})
if(NOT "${gen_src}" STREQUAL "")
    list(REMOVE_ITEM ortools_src ${gen_src})
endif()

file(GLOB_RECURSE ortools_proto_files RELATIVE ${SRC_DIR} "${SRC_DIR}/*.proto")
OR_TOOLS_PROTOBUF_GENERATE_CPP(ortools_proto_srcs ortools_proto_hdrs ${ortools_proto_files})

#TODO(find a way to factorize this)
set(ortools_static_lnk ${DEPENDENCIES_LIB}/libCbcSolver${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libCbc${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libCgl${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libOsi${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libOsiCbc${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libOsiClp${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libClp${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libCoinUtils${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libgflags${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libprotobuf${CMAKE_STATIC_LIBRARY_SUFFIX})

add_library(ortools SHARED ${ortools_proto_srcs} ${ortools_src})

target_link_libraries(ortools ${ortools_static_lnk} z rt pthread)

#[[ The following line is added to AVOID the following default scenario :
  ortools depends on ${ortools_static_lnk}
+  
  target1 depends on ortools
>>
  then target1 depends by default on ${ortools_static_lnk}
]]
set_property(TARGET ortools PROPERTY INTERFACE_LINK_LIBRARIES z rt pthread)

#TODO(ref1: merge cvrptw_lib with or-tools)
add_library(cvrptw_lib SHARED ${CPP_EX_DIR}/cvrptw_lib.cc)
target_link_libraries(cvrptw_lib ortools)

set(fap_src ${CPP_EX_DIR}/fap_model_printer.cc ${CPP_EX_DIR}/fap_parser.cc ${CPP_EX_DIR}/fap_utilities.cc CACHE INTERNAL "" FORCE)
add_library(fap SHARED ${fap_src})
target_link_libraries(fap ortools)

#TODO(compile print_dimacs_assignment.cc)
add_library(dimacs SHARED ${CPP_EX_DIR}/parse_dimacs_assignment.cc)
target_link_libraries(dimacs ortools)

