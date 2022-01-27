set_property(SOURCE init.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE init.i PROPERTY SWIG_MODULE_NAME operations_research_init)
set_property(SOURCE init.i PROPERTY COMPILE_DEFINITIONS
  ${OR_TOOLS_COMPILE_DEFINITIONS} ABSL_MUST_USE_RESULT)
set_property(SOURCE init.i PROPERTY COMPILE_OPTIONS
  -namespace ${DOTNET_PROJECT}.Init
  -dllimport google-ortools-native)
swig_add_library(dotnet_init
  TYPE OBJECT
  LANGUAGE csharp
  OUTPUT_DIR ${DOTNET_PROJECT_DIR}/ortools/init
  SOURCES init.i)

#target_include_directories(dotnet_init PRIVATE ${DOTNET_INCLUDE_DIRS})
set_target_properties(dotnet_init PROPERTIES
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)
target_link_libraries(dotnet_init PRIVATE ortools::ortools)
