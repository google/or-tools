file(GLOB _SRCS "*.h" "*.cc")
set(NAME ${PROJECT_NAME}_base)

# Will be merge in libortools.so
#add_library(${NAME} STATIC ${_SRCS})
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON
  )
target_compile_definitions(${NAME} PRIVATE
  -DOR_TOOLS_MAJOR=${PROJECT_VERSION_MAJOR}
  -DOR_TOOLS_MINOR=${PROJECT_VERSION_MINOR}
  -DOR_TOOLS_PATCH=${PROJECT_VERSION_PATCH})
if(MSVC)
  target_compile_definitions(${NAME} PRIVATE -DOR_TOOLS_EXPORTS)
endif()
target_include_directories(${NAME} PRIVATE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR})
target_link_libraries(${NAME} PRIVATE
  ZLIB::ZLIB
  absl::base
  absl::strings
  absl::str_format
  protobuf::libprotobuf
  ${PROJECT_NAME}::proto)
#add_library(${PROJECT_NAME}::base ALIAS ${NAME})
