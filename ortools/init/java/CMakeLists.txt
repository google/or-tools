set_property(SOURCE init.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE init.i PROPERTY SWIG_MODULE_NAME main)
set_property(SOURCE init.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE init.i PROPERTY COMPILE_OPTIONS
  -package ${JAVA_PACKAGE}.init)
swig_add_library(jniinit
  TYPE OBJECT
  LANGUAGE java
  OUTPUT_DIR ${JAVA_PROJECT_DIR}/${JAVA_SRC_PATH}/init
  SOURCES init.i)

target_include_directories(jniinit PRIVATE ${JNI_INCLUDE_DIRS})
set_target_properties(jniinit PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(jniinit PRIVATE ortools::ortools)
