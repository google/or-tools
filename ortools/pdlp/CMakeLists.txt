if(NOT USE_PDLP)
  return()
endif()

file(GLOB _SRCS "*.h" "*.cc")
list(FILTER _SRCS EXCLUDE REGEX ".*/.*_test.cc")
list(FILTER _SRCS EXCLUDE REGEX ".*/gtest.*")
list(FILTER _SRCS EXCLUDE REGEX ".*/test.*")

set(NAME ${PROJECT_NAME}_pdlp)

if(MSVC)
  set(STANDARD 20)
else()
  set(STANDARD 17)
endif()

# Will be merge in libortools.so
#add_library(${NAME} STATIC ${_SRCS})
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES
  CXX_STANDARD ${STANDARD}
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON
  )
target_include_directories(${NAME} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)
target_link_libraries(${NAME} PRIVATE
  absl::memory
  absl::strings
  absl::str_format
  protobuf::libprotobuf
  Eigen3::Eigen
  ${PROJECT_NAME}::proto)
#add_library(${PROJECT_NAME}::pdlp ALIAS ${NAME})
