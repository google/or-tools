include(CMakeParseArguments)
include(ortools_UseSWIG.cmake)
find_package(SWIG REQUIRED)

#List of swig files
PREFIX_SUFFIX(
  PREFIX ${SRC_DIR}/
  SUFFIX 
  LIST constraint_solver/java/routing.i algorithms/java/knapsack_solver.i graph/java/graph.i 
  RESULT swig_java_sources
)

#TODO(fix this)
set(JNI_INCLUDE_DIRS /usr/lib/jvm/java-8-openjdk-amd64/include /usr/lib/jvm/java-8-openjdk-amd64/include/linux)

include_directories( ${SRC_DIR} ${GEN_DIR} ${DEPENDENCIES_INCLUDE} ${JNI_INCLUDE_DIRS})

set_source_files_properties(${swig_java_sources} PROPERTIES CPLUSPLUS ON)

#Directory of swig C++ wrappers
set(CMAKE_SWIG_OUTDIR ${SWIG_WRAP_DIR})

set(SWIG_MODULE_jniortools_EXTRA_FLAGS "-DUSE_CBC;-DUSE_GLOP;-DUSE_BOP")

set(constraintsolver_outdir "${GEN_DIR}/com/google/ortools/constraintsolver")
set(algorithms_outdir "${GEN_DIR}/com/google/ortools/algorithms")
set(graph_outdir "${GEN_DIR}/com/google/ortools/graph")
set(linearsolver_outdir "${GEN_DIR}/com/google/ortools/linearsolver")

#Manually create java output directories because SWIG doesn't 
file(MAKE_DIRECTORY ${constraintsolver_outdir})
file(MAKE_DIRECTORY ${algorithms_outdir})
file(MAKE_DIRECTORY ${graph_outdir})
file(MAKE_DIRECTORY ${linearsolver_outdir})


#Set SWIG files compilation flags
set_source_files_properties(${SRC_DIR}/constraint_solver/java/routing.i PROPERTIES SWIG_FLAGS "-package;com.google.ortools.constraintsolver;-module;operations_research_constraint_solver;-outdir;${constraintsolver_outdir}")

set_source_files_properties(${SRC_DIR}/algorithms/java/knapsack_solver.i PROPERTIES SWIG_FLAGS "-package;com.google.ortools.algorithms;-module;operations_research_algorithms;-outdir;${algorithms_outdir}")

set_source_files_properties(${SRC_DIR}/graph/java/graph.i PROPERTIES SWIG_FLAGS "-package;com.google.ortools.graph;-module;operations_research_graph;-outdir;${graph_outdir}")

set_source_files_properties(${SRC_DIR}/linear_solver/java/linear_solver.i PROPERTIES SWIG_FLAGS "-package;com.google.ortools.linearsolver;-module;operations_research_linear_solver;-outdir;${linearsolver_outdir}")

#TODO(Add dependencies with other files)
#Add the swig module
swig_add_module(jniortools java ${swig_java_sources})

set_target_properties(jniortools PROPERTIES LINKER_LANGUAGE CXX)
swig_link_libraries(jniortools ortools)
