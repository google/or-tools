pybind11_add_module(pywrap_model_builder_helper MODULE pywrap_model_builder_helper.cc)

# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(pywrap_model_builder_helper PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs"
    )
  set_property(TARGET pywrap_model_builder_helper APPEND PROPERTY
    LINK_FLAGS "-flat_namespace -undefined suppress"
    )
elseif(UNIX)
  set_target_properties(pywrap_model_builder_helper PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs"
    )
endif()

target_link_libraries(pywrap_model_builder_helper PRIVATE ${PROJECT_NAMESPACE}::ortools)
add_library(${PROJECT_NAMESPACE}::pywrap_model_builder_helper ALIAS pywrap_model_builder_helper)
