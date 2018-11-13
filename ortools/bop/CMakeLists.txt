file(GLOB _SRCS "*.h" "*.cc")
set(NAME ${PROJECT_NAME}_bop)

# Will be merge in libortools.so
#add_library(${NAME} STATIC ${_SRCS})
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES CXX_STANDARD 11)
set_target_properties(${NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(${NAME} PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
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
#  protobuf::libprotobuf
#  gflags::gflags
#  glog::glog
#  Cbc::Cbc
#  ${PROJECT_NAME}::proto)
add_dependencies(${NAME}
  protobuf::libprotobuf gflags::gflags glog::glog Cbc::Cbc ${PROJECT_NAME}::proto)
add_library(${PROJECT_NAME}::bop ALIAS ${NAME})
