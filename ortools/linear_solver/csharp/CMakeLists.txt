set_property(SOURCE linear_solver.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE linear_solver.i PROPERTY SWIG_MODULE_NAME operations_research_linear_solver)
set_property(SOURCE linear_solver.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE linear_solver.i PROPERTY COMPILE_OPTIONS
  -namespace ${DOTNET_PROJECT}.LinearSolver
  -dllimport google-ortools-native)
swig_add_library(dotnet_linear_solver
  TYPE OBJECT
  LANGUAGE csharp
  OUTPUT_DIR ${DOTNET_PROJECT_DIR}/ortools/linear_solver
  SOURCES linear_solver.i)

#target_include_directories(dotnet_linear_solver PRIVATE ${DOTNET_INCLUDE_DIRS})
set_target_properties(dotnet_linear_solver PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(dotnet_linear_solver PRIVATE ortools::ortools)
