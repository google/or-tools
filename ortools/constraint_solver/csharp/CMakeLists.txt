set_property(SOURCE routing.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE routing.i PROPERTY SWIG_MODULE_NAME operations_research_constraint_solver)
set_property(SOURCE routing.i PROPERTY COMPILE_OPTIONS
  -namespace ${OR_TOOLS_DOTNET}.ConstraintSolver
  -dllimport google-ortools-native
  )
swig_add_library(dotnet_constraint_solver
  TYPE OBJECT
  LANGUAGE csharp
  OUTPUT_DIR ${PROJECT_BINARY_DIR}/dotnet/${PROJECT_NAME}/constraint_solver
  SOURCES routing.i)

#target_include_directories(dotnet_constraint_solver PRIVATE ${DOTNET_INCLUDE_DIRS})
set_property(TARGET dotnet_constraint_solver PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
target_link_libraries(dotnet_constraint_solver PRIVATE ortools::ortools)
