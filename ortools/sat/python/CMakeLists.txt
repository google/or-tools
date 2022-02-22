pybind11_add_module(swig_helper MODULE swig_helper.cc)
# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(swig_helper PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs"
    )
  set_property(TARGET swig_helper APPEND PROPERTY
    LINK_FLAGS "-flat_namespace -undefined suppress"
    )
elseif(UNIX)
  set_target_properties(swig_helper PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs"
    )
endif()
target_link_libraries(swig_helper PRIVATE ${PROJECT_NAMESPACE}::ortools)
add_library(${PROJECT_NAMESPACE}::swig_helper ALIAS swig_helper)
