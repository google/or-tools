file(GLOB _SRCS "*.h" "*.cc")
if(NOT BUILD_LP_PARSER)
  list(FILTER _SRCS EXCLUDE REGEX ".*/lp_parser.h")
  list(FILTER _SRCS EXCLUDE REGEX ".*/lp_parser.cc")
endif()

set(NAME ${PROJECT_NAME}_lp_data)

# Will be merge in libortools.so
#add_library(${NAME} STATIC ${_SRCS})
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON
  )
target_include_directories(${NAME} PRIVATE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR})
target_link_libraries(${NAME} PRIVATE
  absl::synchronization
  absl::strings
  absl::str_format
  protobuf::libprotobuf
  ${RE2_DEPS}
  ${PROJECT_NAME}::proto)
#add_library(${PROJECT_NAME}::lp_data ALIAS ${NAME})
