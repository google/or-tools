if(NOT BUILD_DOTNET_EXAMPLES)
  return()
endif()

file(GLOB DOTNET_SRCS "*.cs")
foreach(FILE_NAME IN LISTS DOTNET_SRCS)
  add_dotnet_example(${FILE_NAME})
endforeach()
