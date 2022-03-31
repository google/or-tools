pybind11_add_module(linear_sum_assignment_pybind11 MODULE linear_sum_assignment.cc)
# note: macOS is APPLE and also UNIX !
set_target_properties(linear_sum_assignment_pybind11 PROPERTIES
  LIBRARY_OUTPUT_NAME "linear_sum_assignment")
if(APPLE)
  set_target_properties(linear_sum_assignment_pybind11 PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs"
    LINK_FLAGS "-flat_namespace -undefined suppress"
    )
elseif(UNIX)
  set_target_properties(linear_sum_assignment_pybind11 PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs"
    )
endif()
target_link_libraries(linear_sum_assignment_pybind11 PRIVATE ${PROJECT_NAMESPACE}::ortools)
add_library(${PROJECT_NAMESPACE}::linear_sum_assignment_pybind11 ALIAS linear_sum_assignment_pybind11)

pybind11_add_module(max_flow_pybind11 MODULE max_flow.cc)
# note: macOS is APPLE and also UNIX !
set_target_properties(max_flow_pybind11 PROPERTIES
  LIBRARY_OUTPUT_NAME "max_flow")
if(APPLE)
  set_target_properties(max_flow_pybind11 PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs"
    LINK_FLAGS "-flat_namespace -undefined suppress"
    )
elseif(UNIX)
  set_target_properties(max_flow_pybind11 PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs"
    )
endif()
target_link_libraries(max_flow_pybind11 PRIVATE ${PROJECT_NAMESPACE}::ortools)
add_library(${PROJECT_NAMESPACE}::max_flow_pybind11 ALIAS max_flow_pybind11)

pybind11_add_module(min_cost_flow_pybind11 MODULE min_cost_flow.cc)
# note: macOS is APPLE and also UNIX !
set_target_properties(min_cost_flow_pybind11 PROPERTIES
  LIBRARY_OUTPUT_NAME "min_cost_flow")
if(APPLE)
  set_target_properties(min_cost_flow_pybind11 PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs"
    LINK_FLAGS "-flat_namespace -undefined suppress"
    )
elseif(UNIX)
  set_target_properties(min_cost_flow_pybind11 PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs"
    )
endif()
target_link_libraries(min_cost_flow_pybind11 PRIVATE ${PROJECT_NAMESPACE}::ortools)
add_library(${PROJECT_NAMESPACE}::min_cost_flow_pybind11 ALIAS min_cost_flow_pybind11)
