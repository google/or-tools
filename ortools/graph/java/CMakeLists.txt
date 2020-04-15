set_property(SOURCE graph.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE graph.i PROPERTY SWIG_MODULE_NAME main)
set_property(SOURCE graph.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE graph.i PROPERTY COMPILE_OPTIONS
  -package ${OR_TOOLS_JAVA}.graph)
swig_add_library(java_graph
  TYPE OBJECT
  LANGUAGE java
  OUTPUT_DIR ${PROJECT_BINARY_DIR}/java/com/google/${PROJECT_NAME}/graph
  SOURCES graph.i)

target_include_directories(java_graph PRIVATE ${JNI_INCLUDE_DIRS})
set_target_properties(java_graph PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(java_graph PRIVATE ortools::ortools)
