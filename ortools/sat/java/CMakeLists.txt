set_property(SOURCE sat.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE sat.i PROPERTY SWIG_MODULE_NAME main)
set_property(SOURCE sat.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE sat.i PROPERTY COMPILE_OPTIONS
  -package ${JAVA_PACKAGE}.sat)
swig_add_library(jnisat
  TYPE OBJECT
  LANGUAGE java
  OUTPUT_DIR ${JAVA_PROJECT_DIR}/${JAVA_SRC_PATH}/sat
  SOURCES sat.i)

target_include_directories(jnisat PRIVATE ${JNI_INCLUDE_DIRS})
set_target_properties(jnisat PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(jnisat PRIVATE ortools::ortools)
