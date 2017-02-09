include(ortools_protobuf.cmake)

include_directories(${SRC_DIR} ${GEN_DIR} ${DEPENDENCIES_INCLUDE})

set(LINKER_LANGUAGE CXX)
if(UNIX)
    set(CMAKE_CXX_COMPILER "g++" CACHE INTERNAL "" FORCE)
    set(CMAKE_CXX_FLAGS "-fPIC -std=c++0x -fwrapv -DARCH_K8 -Wno-deprecated -DUSE_CLP -DUSE_GLOP -DUSE_BOP" CACHE INTERNAL "" FORCE)
    set(CMAKE_CXX_FLAGS_DEBUG "-DNDEBUG" CACHE INTERNAL "" FORCE)
endif()

file(GLOB flatzinc_src "${SRC_DIR}/flatzinc/*.cc")
#file(GLOB routing_src "${SRC_DIR}/constraint_solver/routing*.cc")

file (GLOB_RECURSE gen_src "${GEN_DIR}/*.cc")

file(GLOB_RECURSE ortools_src "${SRC_DIR}/*.cc")
#list(REMOVE_ITEM ortools_src ${routing_src})
list(REMOVE_ITEM ortools_src ${flatzinc_src})
list(REMOVE_ITEM ortools_src ${SRC_DIR}/cpp/parse_dimacs_assignment.cc)
if(NOT "${gen_src}" STREQUAL "")
    list(REMOVE_ITEM ortools_src ${gen_src})
endif()

file(GLOB_RECURSE ortools_proto_files RELATIVE ${SRC_DIR} "${SRC_DIR}/*.proto")
#list(REMOVE_ITEM ortools_proto_files "${SRC_DIR}/constraint_solver/routing_enums.proto" "${SRC_DIR}/constraint_solver/routing_parameters.proto")

OR_TOOLS_PROTOBUF_GENERATE_CPP(ortools_proto_srcs ortools_proto_hdrs ${ortools_proto_files})

#TODO(find a way to factorize this)
set(ortools_static_lnk ${DEPENDENCIES_LIB}/libCbcSolver${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libCbc${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libCgl${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libOsi${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libOsiCbc${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libOsiClp${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libClp${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libCoinUtils${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libgflags${CMAKE_STATIC_LIBRARY_SUFFIX} ${DEPENDENCIES_LIB}/libprotobuf${CMAKE_STATIC_LIBRARY_SUFFIX})

add_library( ortools SHARED ${ortools_src} ${ortools_proto_srcs})

target_link_libraries(ortools ${ortools_static_lnk} z rt pthread)

#[[ The following line is added to avoid the following default scenario :
  ortools depends on ${ortools_static_lnk}
+  
  target1 depends on ortools
>>
  then target1 depends by default on ${ortools_static_lnk}, which is bad!
]]
set_property(TARGET ortools PROPERTY INTERFACE_LINK_LIBRARIES z rt pthread)

#TODO(add fap, flatzinc and dimacs libs)
