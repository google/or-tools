set_property(SOURCE sat.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE sat.i PROPERTY SWIG_MODULE_NAME operations_research_sat)
set_property(SOURCE sat.i PROPERTY COMPILE_OPTIONS
  -namespace ${OR_TOOLS_DOTNET}.Sat
  -dllimport google-ortools-native
  )
swig_add_library(dotnet_sat
  TYPE OBJECT
  LANGUAGE csharp
  OUTPUT_DIR ${PROJECT_BINARY_DIR}/dotnet/${PROJECT_NAME}/sat
  SOURCES sat.i)

#target_include_directories(dotnet_sat PRIVATE ${DOTNET_INCLUDE_DIRS})
set_property(TARGET dotnet_sat PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
target_link_libraries(dotnet_sat PRIVATE ortools::ortools)
