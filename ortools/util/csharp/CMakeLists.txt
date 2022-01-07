set_property(SOURCE sorted_interval_list.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE sorted_interval_list.i PROPERTY SWIG_MODULE_NAME operations_research_util)
set_property(SOURCE sorted_interval_list.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE sorted_interval_list.i PROPERTY COMPILE_OPTIONS
  -namespace ${DOTNET_PROJECT}.Util
  -dllimport google-ortools-native)
swig_add_library(dotnet_util
  TYPE OBJECT
  LANGUAGE csharp
  OUTPUT_DIR ${DOTNET_PROJECT_DIR}/ortools/util
  SOURCES sorted_interval_list.i)

#target_include_directories(dotnet_util PRIVATE ${DOTNET_INCLUDE_DIRS})
set_target_properties(dotnet_util PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(dotnet_util PRIVATE ortools::ortools)
