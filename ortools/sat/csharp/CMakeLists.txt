set_property(SOURCE sat.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE sat.i PROPERTY SWIG_MODULE_NAME operations_research_sat)
set_property(SOURCE sat.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE sat.i PROPERTY COMPILE_OPTIONS
  -namespace ${DOTNET_PROJECT}.Sat
  -dllimport google-ortools-native)
swig_add_library(dotnet_sat
  TYPE OBJECT
  LANGUAGE csharp
  OUTPUT_DIR ${DOTNET_PROJECT_DIR}/ortools/sat
  SOURCES sat.i)

#target_include_directories(dotnet_sat PRIVATE ${DOTNET_INCLUDE_DIRS})
set_target_properties(dotnet_sat PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(dotnet_sat PRIVATE ortools::ortools)
