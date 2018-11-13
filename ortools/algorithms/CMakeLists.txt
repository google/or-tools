file(GLOB _SRCS "*.h" "*.cc")
list(REMOVE_ITEM _SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/hungarian_test.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/hungarian_test.h
  )

set(NAME ${PROJECT_NAME}_algorithms)

# Will be merge in libortools.so
#add_library(${NAME} STATIC ${_SRCS})
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON
  )
target_include_directories(${NAME} PRIVATE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR}
  $<TARGET_PROPERTY:gflags::gflags,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:glog::glog,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:protobuf::libprotobuf,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:Cbc::Cbc,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:${PROJECT_NAME}::proto,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_options(${NAME} PRIVATE
  $<TARGET_PROPERTY:gflags::gflags,INTERFACE_COMPILE_OPTIONS>
  $<TARGET_PROPERTY:glog::glog,INTERFACE_COMPILE_OPTIONS>
  $<TARGET_PROPERTY:protobuf::libprotobuf,INTERFACE_COMPILE_OPTIONS>
  $<TARGET_PROPERTY:Cbc::Cbc,INTERFACE_COMPILE_OPTIONS>
  $<TARGET_PROPERTY:${PROJECT_NAME}::proto,INTERFACE_COMPILE_OPTIONS>
  )
target_compile_definitions(${NAME} PRIVATE
  $<TARGET_PROPERTY:gflags::gflags,INTERFACE_COMPILE_DEFINITIONS>
  $<TARGET_PROPERTY:glog::glog,INTERFACE_COMPILE_DEFINITIONS>
  $<TARGET_PROPERTY:protobuf::libprotobuf,INTERFACE_COMPILE_DEFINITIONS>
  $<TARGET_PROPERTY:Cbc::Cbc,INTERFACE_COMPILE_DEFINITIONS>
  $<TARGET_PROPERTY:${PROJECT_NAME}::proto,INTERFACE_COMPILE_DEFINITIONS>
  )
# CMakeError: Object library may not link to anything.
#target_link_libraries(${NAME} PRIVATE
#  gflags::gflags
#  glog::glog
#  protobuf::libprotobuf
#  Cbc::Cbc
#  ${PROJECT_NAME}::proto)
add_dependencies(${NAME}
  protobuf::libprotobuf gflags::gflags glog::glog Cbc::Cbc ${PROJECT_NAME}::proto)
add_library(${PROJECT_NAME}::algorithms ALIAS ${NAME})
