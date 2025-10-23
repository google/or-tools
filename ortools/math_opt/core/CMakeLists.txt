add_subdirectory(c_api)

file(GLOB _SRCS "*.h" "*.cc")
#list(APPEND _SRCS "${CMAKE_CURRENT_SOURCE_DIR}/c_api/solver.cc")
list(FILTER _SRCS EXCLUDE REGEX "/[^/]*_test\\.cc$")
# Remove files pulling gmock and protobuf_matchers.
list(FILTER _SRCS EXCLUDE REGEX "/solver_interface_mock\\.h$")
list(FILTER _SRCS EXCLUDE REGEX "/solver_interface_mock\\.cc$")
list(FILTER _SRCS EXCLUDE REGEX "/sparse_collection_matchers\\.h$")
list(FILTER _SRCS EXCLUDE REGEX "/sparse_collection_matchers\\.cc$")

set(NAME ${PROJECT_NAME}_math_opt_core)
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC AND BUILD_SHARED_LIBS)
  target_compile_definitions(${NAME} PUBLIC "OR_BUILD_DLL")
  target_compile_definitions(${NAME} PRIVATE "OR_EXPORT")
endif()
target_include_directories(${NAME} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)
target_link_libraries(${NAME} PRIVATE
  absl::strings
  ${PROJECT_NAMESPACE}::math_opt_proto
)
