if(NOT BUILD_DOTNET)
  return()
endif()

find_package(SWIG)
find_program (DOTNET_CLI NAMES dotnet)
