include(ortools_protobuf.cmake)
include(ortools_SWIG.cmake)

add_custom_target(
  python
  DEPENDS ${python_gen_protobuf} ${swig_python_libraries}
)

message(${swig_python_libraries})

list(APPEND swig_modules pywrapcp pywraplp pywrapknapsack_solver pywrapgraph)