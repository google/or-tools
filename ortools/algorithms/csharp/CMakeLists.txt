set_property(SOURCE knapsack_solver.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE knapsack_solver.i PROPERTY SWIG_MODULE_NAME operations_research_algorithms)
set_property(SOURCE knapsack_solver.i PROPERTY COMPILE_OPTIONS
  -namespace ${OR_TOOLS_DOTNET}.Algorithms
  -dllimport google-ortools-native
  )
swig_add_library(dotnet_algorithms
  TYPE OBJECT
  LANGUAGE csharp
  OUTPUT_DIR ${PROJECT_BINARY_DIR}/dotnet/${PROJECT_NAME}/algorithms
  SOURCES knapsack_solver.i)

#target_include_directories(dotnet_algorithms PRIVATE ${DOTNET_INCLUDE_DIRS})
set_property(TARGET dotnet_algorithms PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
target_link_libraries(dotnet_algorithms PRIVATE ortools::ortools)
