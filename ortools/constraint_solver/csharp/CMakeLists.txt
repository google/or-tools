# Can't use SWIG_COMPILE_OPTIONS property
# see: https://gitlab.kitware.com/cmake/cmake/issues/20389
list(APPEND CMAKE_SWIG_FLAGS -dllimport google-ortools-native)

set_property(SOURCE routing.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE routing.i PROPERTY SWIG_MODULE_NAME operations_research_constraint_solver)
set_property(SOURCE routing.i PROPERTY SWIG_COMPILE_OPTIONS
  -nofastunpack
  -namespace ${OR_TOOLS_DOTNET}.ConstraintSolver
  -dllimport google-ortools-native
  )
swig_add_library(operations_research_constraint_solver
  TYPE OBJECT
  LANGUAGE csharp
  OUTPUT_DIR ${PROJECT_BINARY_DIR}/dotnet/${PROJECT_NAME}/constraint_solver
  SOURCES routing.i)

#target_include_directories(operations_research_constraint_solver PRIVATE ${DOTNET_INCLUDE_DIRS})
set_property(TARGET operations_research_constraint_solver PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
target_link_libraries(operations_research_constraint_solver PRIVATE ortools::ortools)
