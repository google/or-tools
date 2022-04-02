set_property(SOURCE modelbuilder.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE modelbuilder.i PROPERTY SWIG_MODULE_NAME main)
set_property(SOURCE modelbuilder.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE modelbuilder.i PROPERTY COMPILE_OPTIONS
  -package ${JAVA_PACKAGE}.modelbuilder)
swig_add_library(jnimodel_builder
  TYPE OBJECT
  LANGUAGE java
  OUTPUT_DIR ${JAVA_PROJECT_DIR}/${JAVA_SRC_PATH}/modelbuilder
  SOURCES modelbuilder.i)

target_include_directories(jnimodel_builder PRIVATE ${JNI_INCLUDE_DIRS})
set_target_properties(jnimodel_builder PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(jnimodel_builder PRIVATE ortools::ortools)
