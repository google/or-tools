set_property(SOURCE linear_solver.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE linear_solver.i PROPERTY SWIG_MODULE_NAME operations_research_linear_solver)
set_property(SOURCE linear_solver.i PROPERTY COMPILE_OPTIONS
  -namespace ${OR_TOOLS_DOTNET}.LinearSolver
  -dllimport google-ortools-native
  )
swig_add_library(operations_research_linear_solver
  TYPE OBJECT
  LANGUAGE csharp
  OUTPUT_DIR ${PROJECT_BINARY_DIR}/dotnet/${PROJECT_NAME}/linear_solver
  SOURCES linear_solver.i)

#target_include_directories(operations_research_linear_solver PRIVATE ${DOTNET_INCLUDE_DIRS})
set_property(TARGET operations_research_linear_solver PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
target_link_libraries(operations_research_linear_solver PRIVATE ortools::ortools)
