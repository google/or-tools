set_property(SOURCE sorted_interval_list.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE sorted_interval_list.i PROPERTY SWIG_MODULE_NAME main)
set_property(SOURCE sorted_interval_list.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE sorted_interval_list.i PROPERTY COMPILE_OPTIONS
  -package ${JAVA_PACKAGE}.util)
swig_add_library(jniutil
  TYPE OBJECT
  LANGUAGE java
  OUTPUT_DIR ${JAVA_PROJECT_DIR}/${JAVA_SRC_PATH}/util
  SOURCES sorted_interval_list.i)

target_include_directories(jniutil PRIVATE ${JNI_INCLUDE_DIRS})
set_target_properties(jniutil PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(jniutil PRIVATE ortools::ortools)
