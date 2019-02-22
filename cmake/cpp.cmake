include(utils)
set_version(VERSION)
project(ortools LANGUAGES CXX VERSION ${VERSION})
message(STATUS "ortools version: ${PROJECT_VERSION}")

# config options
if(MSVC)
  # Allow big object
  add_definitions(/bigobj)
  add_definitions(/DNOMINMAX /DWIN32_LEAN_AND_MEAN=1 /D_CRT_SECURE_NO_WARNINGS /D_CRT_SECURE_NO_DEPRECATE)
  # Build with multiple processes
  add_definitions(/MP)
  # Prefer /MD over /MT and add NDEBUG in Release
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(/MDd)
  else()
    add_definitions(/MD /DNDEBUG)
  endif()
  # MSVC warning suppressions
  add_definitions(
    /wd4005 # 'macro-redefinition'
    /wd4018 # 'expression' : signed/unsigned mismatch
    /wd4065 # switch statement contains 'default' but no 'case' labels
    /wd4068 # 'unknown pragma'
    /wd4101 # 'identifier' : unreferenced local variable
    /wd4146 # unary minus operator applied to unsigned type, result still unsigned
    /wd4200 # nonstandard extension used : zero-sized array in struct/union
    /wd4244 # 'conversion' conversion from 'type1' to 'type2', possible loss of data
    /wd4251 # 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'
    /wd4267 # 'var' : conversion from 'size_t' to 'type', possible loss of data
    /wd4305 # 'identifier' : truncation from 'type1' to 'type2'
    /wd4307 # 'operator' : integral constant overflow
    /wd4309 # 'conversion' : truncation of constant value
    /wd4334 # 'operator' : result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
    /wd4355 # 'this' : used in base member initializer list
    /wd4477 # 'fwprintf' : format string '%s' requires an argument of type 'wchar_t *'
    /wd4506 # no definition for inline function 'function'
    /wd4715 # function' : not all control paths return a value
    /wd4800 # 'type' : forcing value to bool 'true' or 'false' (performance warning)
    /wd4996 # The compiler encountered a deprecated declaration.
    )
else()
  add_definitions(-fwrapv)
endif()
add_definitions(-DUSE_GLOP -DUSE_BOP -DUSE_CBC -DUSE_CLP)

# Verify Dependencies
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

# Main Target
add_library(${PROJECT_NAME} "")
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_11)
if(NOT APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
else()
  # Clang don't support version x.y.z with z > 255
  set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 11)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${PROJECT_NAME} PROPERTIES INTERFACE_POSITION_INDEPENDENT_CODE ON)
set_target_properties(${PROJECT_NAME} PROPERTIES INTERFACE_${PROJECT_NAME}_MAJOR_VERSION ${PROJECT_VERSION_MAJOR})
set_target_properties(${PROJECT_NAME} PROPERTIES COMPATIBLE_INTERFACE_STRING ${PROJECT_NAME}_MAJOR_VERSION)
if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH
    "@loader_path")
endif()
target_include_directories(${PROJECT_NAME} INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
  $<INSTALL_INTERFACE:include>
  )
target_link_libraries(${PROJECT_NAME} PUBLIC
  ZLIB::ZLIB
  absl::base
  absl::raw_hash_set
  absl::hash
  absl::memory
  absl::meta
  absl::str_format
  absl::strings
  absl::synchronization
  absl::any
  gflags::gflags glog::glog
  protobuf::libprotobuf
  Cbc::CbcSolver Cbc::OsiCbc Cbc::ClpSolver Cbc::OsiClp
  Threads::Threads)
if(WIN32)
	target_link_libraries(${PROJECT_NAME} PUBLIC psapi.lib ws2_32.lib)
target_compile_definitions(${PROJECT_NAME} PUBLIC __WIN32__)
endif()
target_compile_definitions(${PROJECT_NAME}
	PUBLIC	USE_BOP USE_GLOP USE_CBC USE_CLP)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# Generate Protobuf cpp sources
set(PROTO_HDRS)
set(PROTO_SRCS)
file(GLOB_RECURSE proto_files RELATIVE ${PROJECT_SOURCE_DIR} "ortools/*.proto")

# Get Protobuf include dir
get_target_property(protobuf_dirs protobuf::libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
foreach(dir ${protobuf_dirs})
  if ("${dir}" MATCHES "BUILD_INTERFACE")
    list(APPEND PROTO_DIRS "--proto_path=${dir}")
  endif()
endforeach()

foreach (PROTO_FILE ${proto_files})
  #message(STATUS "protoc proto(cc): ${PROTO_FILE}")
  get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(PROTO_HDR ${PROJECT_BINARY_DIR}/${PROTO_DIR}/${PROTO_NAME}.pb.h)
  set(PROTO_SRC ${PROJECT_BINARY_DIR}/${PROTO_DIR}/${PROTO_NAME}.pb.cc)
  #message(STATUS "protoc hdr: ${PROTO_HDR}")
  #message(STATUS "protoc src: ${PROTO_SRC}")
  add_custom_command(
    OUTPUT ${PROTO_SRC} ${PROTO_HDR}
    COMMAND protobuf::protoc
    "--proto_path=${PROJECT_SOURCE_DIR}"
    "${PROTO_DIRS}"
    "--cpp_out=${PROJECT_BINARY_DIR}"
    ${PROTO_FILE}
    DEPENDS ${PROTO_FILE} protobuf::protoc
    COMMENT "Running C++ protocol buffer compiler on ${PROTO_FILE}"
    VERBATIM)
  list(APPEND PROTO_HDRS ${PROTO_HDR})
  list(APPEND PROTO_SRCS ${PROTO_SRC})
endforeach()
#add_library(${PROJECT_NAME}_proto STATIC ${PROTO_SRCS} ${PROTO_HDRS})
add_library(${PROJECT_NAME}_proto OBJECT ${PROTO_SRCS} ${PROTO_HDRS})
set_target_properties(${PROJECT_NAME}_proto PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${PROJECT_NAME}_proto PROPERTIES CXX_STANDARD 11)
set_target_properties(${PROJECT_NAME}_proto PROPERTIES CXX_STANDARD_REQUIRED ON)
set_target_properties(${PROJECT_NAME}_proto PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(${PROJECT_NAME}_proto PRIVATE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR}
  $<TARGET_PROPERTY:protobuf::libprotobuf,INTERFACE_INCLUDE_DIRECTORIES>
  )
#target_link_libraries(${PROJECT_NAME}_proto PRIVATE protobuf::libprotobuf)
add_dependencies(${PROJECT_NAME}_proto protobuf::libprotobuf)
add_library(${PROJECT_NAME}::proto ALIAS ${PROJECT_NAME}_proto)
# Add ortools::proto to libortools
#target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}::proto)
target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${PROJECT_NAME}::proto>)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}::proto)

foreach(SUBPROJECT
    algorithms base bop constraint_solver data glop graph linear_solver lp_data
    port sat util)
  add_subdirectory(ortools/${SUBPROJECT})
  #target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}::${SUBPROJECT})
  target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${PROJECT_NAME}::${SUBPROJECT}>)
  add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}::${SUBPROJECT})
endforeach()

# Install rules
include(GNUInstallDirs)

include(GenerateExportHeader)
GENERATE_EXPORT_HEADER(${PROJECT_NAME})
install(FILES ${PROJECT_BINARY_DIR}/${PROJECT_NAME}_export.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}Targets
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

install(EXPORT ${PROJECT_NAME}Targets
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
install(DIRECTORY ortools
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT Devel
  FILES_MATCHING
  PATTERN "*.h")
install(DIRECTORY ${PROJECT_BINARY_DIR}/ortools
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT Devel
  FILES_MATCHING
  PATTERN "*.pb.h"
  PATTERN CMakeFiles EXCLUDE)

include(CMakePackageConfigHelpers)
string (TOUPPER "${PROJECT_NAME}" PACKAGE_PREFIX)
configure_package_config_file(cmake/${PROJECT_NAME}Config.cmake.in
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  NO_CHECK_REQUIRED_COMPONENTS_MACRO)
write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  COMPATIBILITY SameMajorVersion
  )
install(
  FILES
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  COMPONENT Devel)
