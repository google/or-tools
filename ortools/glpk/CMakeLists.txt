if(NOT USE_GLPK)
  return()
endif()

file(GLOB _SRCS "*.h" "*.cc")

set(NAME ${PROJECT_NAME}_glpk)

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
  absl::strings
  absl::str_format
  $<$<BOOL:${USE_GLPK}>:GLPK::GLPK>)
#add_library(${PROJECT_NAME}::glpk ALIAS ${NAME})
