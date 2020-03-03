set_property(SOURCE sorted_interval_list.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE sorted_interval_list.i PROPERTY SWIG_MODULE_NAME operations_research_util)
set_property(SOURCE sorted_interval_list.i PROPERTY COMPILE_OPTIONS
  -namespace ${OR_TOOLS_DOTNET}.Util
  -dllimport google-ortools-native
  )
swig_add_library(dotnet_util
  TYPE OBJECT
  LANGUAGE csharp
  OUTPUT_DIR ${PROJECT_BINARY_DIR}/dotnet/${PROJECT_NAME}/util
  SOURCES sorted_interval_list.i)

#target_include_directories(dotnet_util PRIVATE ${DOTNET_INCLUDE_DIRS})
set_property(TARGET dotnet_util PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
target_link_libraries(dotnet_util PRIVATE ortools::ortools)
