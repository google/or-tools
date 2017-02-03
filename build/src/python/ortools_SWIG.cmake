find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})
find_package(PythonLibs)

include_directories( ${SRC_DIR} ${GEN_DIR} ${DEPENDENCIES_INCLUDE} ${PYTHON_INCLUDE_PATH} )


function(SWIG_PYTHON_MODULE module_name source_file out_dir libraries)
  file(MAKE_DIRECTORY ${out_dir})
  set(CMAKE_SWIG_OUTDIR ${out_dir})
  set_source_files_properties(${source_file} PROPERTIES CPLUSPLUS ON)
  set_source_files_properties(${source_file} PROPERTIES SWIG_FLAGS "-module;${module_name}")
  set_property(SOURCE ${source_file} PROPERTY SWIG_MODULE_NAME ${module_name})
  swig_add_module(${module_name} python ${source_file})
  swig_link_libraries(${module_name} ortools)
  list(APPEND ${libraries} ${SWIG_MODULE_${module_name}_REAL_NAME})
  set(${libraries} ${${libraries}} PARENT_SCOPE)
endfunction()

SWIG_PYTHON_MODULE(pywrapcp ${SRC_DIR}/constraint_solver/python/routing.i ${GEN_DIR}/ortools/constraint_solver swig_python_libraries)
SWIG_PYTHON_MODULE(pywraplp ${SRC_DIR}/linear_solver/python/linear_solver.i ${GEN_DIR}/ortools/linear_solver swig_python_libraries)
SWIG_PYTHON_MODULE(pywrapknapsack_solver ${SRC_DIR}/algorithms/python/knapsack_solver.i ${GEN_DIR}/ortools/algorithms swig_python_libraries)
SWIG_PYTHON_MODULE(pywrapgraph ${SRC_DIR}/graph/python/graph.i ${GEN_DIR}/ortools/graph swig_python_libraries)

