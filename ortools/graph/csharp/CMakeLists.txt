set_property(SOURCE graph.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE graph.i PROPERTY SWIG_MODULE_NAME operations_research_graph)
set_property(SOURCE graph.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE graph.i PROPERTY COMPILE_OPTIONS
  -namespace ${DOTNET_PROJECT}.Graph
  -dllimport google-ortools-native)
swig_add_library(dotnet_graph
  TYPE OBJECT
  LANGUAGE csharp
  OUTPUT_DIR ${DOTNET_PROJECT_DIR}/ortools/graph
  SOURCES graph.i)

#target_include_directories(dotnet_graph PRIVATE ${DOTNET_INCLUDE_DIRS})
set_target_properties(dotnet_graph PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(dotnet_graph PRIVATE ortools::ortools)
