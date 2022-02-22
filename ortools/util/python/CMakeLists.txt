
pybind11_add_module(sorted_interval_list MODULE sorted_interval_list.cc)
# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(sorted_interval_list PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs"
    )
  set_property(TARGET sorted_interval_list APPEND PROPERTY
    LINK_FLAGS "-flat_namespace -undefined suppress"
    )
elseif(UNIX)
  set_target_properties(sorted_interval_list PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs"
    )
endif()
target_link_libraries(sorted_interval_list PRIVATE ${PROJECT_NAMESPACE}::ortools)
add_library(${PROJECT_NAMESPACE}::sorted_interval_list ALIAS sorted_interval_list)
