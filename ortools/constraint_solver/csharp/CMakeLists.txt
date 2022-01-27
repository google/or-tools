set_property(SOURCE routing.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE routing.i PROPERTY SWIG_MODULE_NAME operations_research_constraint_solver)
set_property(SOURCE routing.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE routing.i PROPERTY COMPILE_OPTIONS
  -namespace ${DOTNET_PROJECT}.ConstraintSolver
  -dllimport google-ortools-native)
swig_add_library(dotnet_constraint_solver
  TYPE OBJECT
  LANGUAGE csharp
  OUTPUT_DIR ${DOTNET_PROJECT_DIR}/ortools/constraint_solver
  SOURCES routing.i)

#target_include_directories(dotnet_constraint_solver PRIVATE ${DOTNET_INCLUDE_DIRS})
set_target_properties(dotnet_constraint_solver PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(dotnet_constraint_solver PRIVATE ortools::ortools)
