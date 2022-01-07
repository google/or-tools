set_property(SOURCE routing.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE routing.i PROPERTY SWIG_MODULE_NAME main)
set_property(SOURCE routing.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE routing.i PROPERTY COMPILE_OPTIONS
  -package ${JAVA_PACKAGE}.constraintsolver)
swig_add_library(jniconstraint_solver
  TYPE OBJECT
  LANGUAGE java
  OUTPUT_DIR ${JAVA_PROJECT_DIR}/${JAVA_SRC_PATH}/constraintsolver
  SOURCES routing.i)

target_include_directories(jniconstraint_solver PRIVATE ${JNI_INCLUDE_DIRS})
set_target_properties(jniconstraint_solver PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(jniconstraint_solver PRIVATE ortools::ortools)
