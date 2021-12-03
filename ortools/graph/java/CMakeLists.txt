set_property(SOURCE graph.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE graph.i PROPERTY SWIG_MODULE_NAME main)
set_property(SOURCE graph.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE graph.i PROPERTY COMPILE_OPTIONS
  -package ${JAVA_PACKAGE}.graph)
swig_add_library(jnigraph
  TYPE OBJECT
  LANGUAGE java
  OUTPUT_DIR ${JAVA_PROJECT_DIR}/${JAVA_SRC_PATH}/graph
  SOURCES graph.i)

target_include_directories(jnigraph PRIVATE ${JNI_INCLUDE_DIRS})
set_target_properties(jnigraph PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(jnigraph PRIVATE ortools::ortools)
