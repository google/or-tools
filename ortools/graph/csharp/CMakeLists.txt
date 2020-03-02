set_property(SOURCE graph.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE graph.i PROPERTY SWIG_MODULE_NAME operations_research_graph)
set_property(SOURCE graph.i PROPERTY COMPILE_OPTIONS
  -namespace ${OR_TOOLS_DOTNET}.Graph
  -dllimport google-ortools-native
  )
swig_add_library(operations_research_graph
  TYPE OBJECT
  LANGUAGE csharp
  OUTPUT_DIR ${PROJECT_BINARY_DIR}/dotnet/${PROJECT_NAME}/graph
  SOURCES graph.i)

#target_include_directories(operations_research_graph PRIVATE ${DOTNET_INCLUDE_DIRS})
set_property(TARGET operations_research_graph PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
target_link_libraries(operations_research_graph PRIVATE ortools::ortools)
