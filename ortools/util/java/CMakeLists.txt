set_property(SOURCE sorted_interval_list.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE sorted_interval_list.i PROPERTY SWIG_MODULE_NAME main)
set_property(SOURCE sorted_interval_list.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE sorted_interval_list.i PROPERTY COMPILE_OPTIONS
  -package ${OR_TOOLS_JAVA}.util)
swig_add_library(java_util
  TYPE OBJECT
  LANGUAGE java
  OUTPUT_DIR ${PROJECT_BINARY_DIR}/java/com/google/${PROJECT_NAME}/util
  SOURCES sorted_interval_list.i)

target_include_directories(java_util PRIVATE ${JNI_INCLUDE_DIRS})
set_target_properties(java_util PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(java_util PRIVATE ortools::ortools)
