set_property(SOURCE sat.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE sat.i PROPERTY SWIG_MODULE_NAME operations_research_sat)
set_property(SOURCE sat.i PROPERTY COMPILE_OPTIONS
  -namespace ${OR_TOOLS_DOTNET}.Sat
  -dllimport google-ortools-native
  )
swig_add_library(operations_research_sat
  TYPE OBJECT
  LANGUAGE csharp
  OUTPUT_DIR ${PROJECT_BINARY_DIR}/dotnet/${PROJECT_NAME}/sat
  SOURCES sat.i)

#target_include_directories(operations_research_sat PRIVATE ${DOTNET_INCLUDE_DIRS})
set_property(TARGET operations_research_sat PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
target_link_libraries(operations_research_sat PRIVATE ortools::ortools)
