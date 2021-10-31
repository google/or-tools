file(GLOB _SRCS "*.h")
set(NAME ${PROJECT_NAME}_init)

# Will be merge in libortools.so
#add_library(${NAME} STATIC ${_SRCS})
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  LINKER_LANGUAGE CXX
  POSITION_INDEPENDENT_CODE ON
  )
target_include_directories(${NAME} PRIVATE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR})
target_link_libraries(${NAME} PRIVATE
  absl::flags
  absl::strings
  protobuf::libprotobuf
  ${PROJECT_NAME}::proto)
#add_library(${PROJECT_NAME}::init ALIAS ${NAME})
