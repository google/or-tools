set_property(SOURCE knapsack_solver.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE knapsack_solver.i PROPERTY SWIG_MODULE_NAME operations_research_algorithms)
set_property(SOURCE knapsack_solver.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE knapsack_solver.i PROPERTY COMPILE_OPTIONS
  -namespace ${DOTNET_PROJECT}.Algorithms
  -dllimport google-ortools-native)
swig_add_library(dotnet_algorithms
  TYPE OBJECT
  LANGUAGE csharp
  OUTPUT_DIR ${DOTNET_PROJECT_DIR}/ortools/algorithms
  SOURCES knapsack_solver.i)

#target_include_directories(dotnet_algorithms PRIVATE ${DOTNET_INCLUDE_DIRS})
set_target_properties(dotnet_algorithms PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(dotnet_algorithms PRIVATE ortools::ortools)
