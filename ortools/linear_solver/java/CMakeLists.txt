set_property(SOURCE linear_solver.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE linear_solver.i PROPERTY SWIG_MODULE_NAME main)
set_property(SOURCE linear_solver.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE linear_solver.i PROPERTY COMPILE_OPTIONS
  -package ${JAVA_PACKAGE}.linearsolver)
swig_add_library(jnilinear_solver
  TYPE OBJECT
  LANGUAGE java
  OUTPUT_DIR ${JAVA_PROJECT_DIR}/${JAVA_SRC_PATH}/linearsolver
  SOURCES linear_solver.i)

target_include_directories(jnilinear_solver PRIVATE ${JNI_INCLUDE_DIRS})
set_target_properties(jnilinear_solver PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(jnilinear_solver PRIVATE ortools::ortools)
