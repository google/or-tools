# Copyright 2010-2022 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if(NOT BUILD_CXX)
  return()
endif()

# Main Target
add_library(${PROJECT_NAME} "")
# Xcode fails to build if library doesn't contains at least one source file.
if(XCODE)
  file(GENERATE
    OUTPUT ${PROJECT_BINARY_DIR}/${PROJECT_NAME}/version.cpp
    CONTENT "namespace {char* version = \"${PROJECT_VERSION}\";}")
  target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}/version.cpp)
endif()

if(BUILD_SHARED_LIBS)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "OR_TOOLS_AS_DYNAMIC_LIB")
endif()
list(APPEND OR_TOOLS_COMPILE_DEFINITIONS
  "USE_BOP" # enable BOP support
  "USE_GLOP" # enable GLOP support
  )
if(BUILD_LP_PARSER)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "USE_LP_PARSER")
endif()
if(USE_COINOR)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS
    "USE_CBC" # enable COIN-OR CBC support
    "USE_CLP" # enable COIN-OR CLP support
  )
endif()
if(USE_GLPK)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "USE_GLPK")
  set(GLPK_DIR glpk)
endif()
if(USE_PDLP)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "USE_PDLP")
  set(PDLP_DIR pdlp)
endif()
if(USE_SCIP)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "USE_SCIP")
  set(GSCIP_DIR gscip)
endif()
if(USE_CPLEX)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "USE_CPLEX")
endif()
if(USE_XPRESS)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "USE_XPRESS")
  if(MSVC)
    list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "XPRESS_PATH=\"${XPRESS_ROOT}\"")
  else()
    list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "XPRESS_PATH=${XPRESS_ROOT}")
  endif()
endif()

if(WIN32)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "__WIN32__")
endif()
if(MSVC)
  list(APPEND OR_TOOLS_COMPILE_OPTIONS
    "/bigobj" # Allow big object
    "/DNOMINMAX"
    "/DWIN32_LEAN_AND_MEAN=1"
    "/D_CRT_SECURE_NO_WARNINGS"
    "/D_CRT_SECURE_NO_DEPRECATE"
    "/MP" # Build with multiple processes
    "/Zc:preprocessor" # Enable preprocessor conformance mode
    "/DNDEBUG"
    )
  # MSVC warning suppressions
  list(APPEND OR_TOOLS_COMPILE_OPTIONS
    "/wd4005" # 'macro-redefinition'
    "/wd4018" # 'expression' : signed/unsigned mismatch
    "/wd4065" # switch statement contains 'default' but no 'case' labels
    "/wd4068" # 'unknown pragma'
    "/wd4101" # 'identifier' : unreferenced local variable
    "/wd4146" # unary minus operator applied to unsigned type, result still unsigned
    "/wd4200" # nonstandard extension used : zero-sized array in struct/union
    "/wd4244" # 'conversion' conversion from 'type1' to 'type2', possible loss of data
    "/wd4251" # 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'
    "/wd4267" # 'var' : conversion from 'size_t' to 'type', possible loss of data
    "/wd4305" # 'identifier' : truncation from 'type1' to 'type2'
    "/wd4307" # 'operator' : integral constant overflow
    "/wd4309" # 'conversion' : truncation of constant value
    "/wd4334" # 'operator' : result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
    "/wd4355" # 'this' : used in base member initializer list
    "/wd4477" # 'fwprintf' : format string '%s' requires an argument of type 'wchar_t *'
    "/wd4506" # no definition for inline function 'function'
    "/wd4715" # function' : not all control paths return a value
    "/wd4800" # 'type' : forcing value to bool 'true' or 'false' (performance warning)
    "/wd4996" # The compiler encountered a deprecated declaration.
    )
else()
  list(APPEND OR_TOOLS_COMPILE_OPTIONS "-fwrapv")
endif()

# Includes
target_include_directories(${PROJECT_NAME} INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
  $<INSTALL_INTERFACE:include>
  )

# Compile options
if(MSVC)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20)
else()
  set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17)
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  )
target_compile_features(${PROJECT_NAME} PUBLIC
  $<IF:$<CXX_COMPILER_ID:MSVC>,cxx_std_20,cxx_std_17>)
target_compile_definitions(${PROJECT_NAME} PUBLIC ${OR_TOOLS_COMPILE_DEFINITIONS})
target_compile_options(${PROJECT_NAME} PUBLIC ${OR_TOOLS_COMPILE_OPTIONS})

# Properties
if(NOT APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
else()
  # Clang don't support version x.y.z with z > 255
  set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "@loader_path"
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES
  SOVERSION ${PROJECT_VERSION_MAJOR}
  POSITION_INDEPENDENT_CODE ON
  INTERFACE_POSITION_INDEPENDENT_CODE ON
)
set_target_properties(${PROJECT_NAME} PROPERTIES INTERFACE_${PROJECT_NAME}_MAJOR_VERSION ${PROJECT_VERSION_MAJOR})
set_target_properties(${PROJECT_NAME} PROPERTIES COMPATIBLE_INTERFACE_STRING ${PROJECT_NAME}_MAJOR_VERSION)

# Dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC
  ${CMAKE_DL_LIBS}
  ZLIB::ZLIB
  ${ABSL_DEPS}
  protobuf::libprotobuf
  ${RE2_DEPS}
  ${COINOR_DEPS}
  $<$<BOOL:${USE_CPLEX}>:CPLEX::CPLEX>
  $<$<BOOL:${USE_GLPK}>:GLPK::GLPK>
  ${PDLP_DEPS}
  $<$<BOOL:${USE_SCIP}>:libscip>
  $<$<BOOL:${USE_XPRESS}>:XPRESS::XPRESS>
  Threads::Threads)
if(WIN32)
  target_link_libraries(${PROJECT_NAME} PUBLIC psapi.lib ws2_32.lib)
endif()

# ALIAS
add_library(${PROJECT_NAMESPACE}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# Generate Protobuf cpp sources
set(PROTO_HDRS)
set(PROTO_SRCS)
file(GLOB_RECURSE proto_files RELATIVE ${PROJECT_SOURCE_DIR}
  "ortools/bop/*.proto"
  "ortools/constraint_solver/*.proto"
  "ortools/glop/*.proto"
  "ortools/graph/*.proto"
  "ortools/linear_solver/*.proto"
  "ortools/linear_solver/*.proto"
  "ortools/packing/*.proto"
  "ortools/sat/*.proto"
  "ortools/scheduling/*.proto"
  "ortools/util/*.proto"
  )
if(USE_PDLP)
  file(GLOB_RECURSE pdlp_proto_files RELATIVE ${PROJECT_SOURCE_DIR} "ortools/pdlp/*.proto")
  list(APPEND proto_files ${pdlp_proto_files})
endif()
if(USE_SCIP)
  file(GLOB_RECURSE gscip_proto_files RELATIVE ${PROJECT_SOURCE_DIR} "ortools/gscip/*.proto")
  list(APPEND proto_files ${gscip_proto_files})
endif()

## Get Protobuf include dir
get_target_property(protobuf_dirs protobuf::libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
foreach(dir IN LISTS protobuf_dirs)
  if (NOT "${dir}" MATCHES "INSTALL_INTERFACE|-NOTFOUND")
    message(STATUS "Adding proto path: ${dir}")
    list(APPEND PROTO_DIRS "--proto_path=${dir}")
  endif()
endforeach()

foreach(PROTO_FILE IN LISTS proto_files)
  #message(STATUS "protoc proto(cc): ${PROTO_FILE}")
  get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(PROTO_HDR ${PROJECT_BINARY_DIR}/${PROTO_DIR}/${PROTO_NAME}.pb.h)
  set(PROTO_SRC ${PROJECT_BINARY_DIR}/${PROTO_DIR}/${PROTO_NAME}.pb.cc)
  #message(STATUS "protoc hdr: ${PROTO_HDR}")
  #message(STATUS "protoc src: ${PROTO_SRC}")
  add_custom_command(
    OUTPUT ${PROTO_SRC} ${PROTO_HDR}
    COMMAND ${PROTOC_PRG}
    "--proto_path=${PROJECT_SOURCE_DIR}"
    ${PROTO_DIRS}
    "--cpp_out=${PROJECT_BINARY_DIR}"
    ${PROTO_FILE}
    DEPENDS ${PROTO_FILE} ${PROTOC_PRG}
    COMMENT "Generate C++ protocol buffer for ${PROTO_FILE}"
    VERBATIM)
  list(APPEND PROTO_HDRS ${PROTO_HDR})
  list(APPEND PROTO_SRCS ${PROTO_SRC})
endforeach()

if(MSVC)
  set(CMAKE_CXX_STANDARD 20)
else()
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#add_library(${PROJECT_NAME}_proto STATIC ${PROTO_SRCS} ${PROTO_HDRS})
add_library(${PROJECT_NAME}_proto OBJECT ${PROTO_SRCS} ${PROTO_HDRS})
set_target_properties(${PROJECT_NAME}_proto PROPERTIES
  POSITION_INDEPENDENT_CODE ON)
target_include_directories(${PROJECT_NAME}_proto PRIVATE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR}
  $<TARGET_PROPERTY:protobuf::libprotobuf,INTERFACE_INCLUDE_DIRECTORIES>
  )
target_compile_definitions(${PROJECT_NAME}_proto PUBLIC ${OR_TOOLS_COMPILE_DEFINITIONS})
target_compile_options(${PROJECT_NAME}_proto PUBLIC ${OR_TOOLS_COMPILE_OPTIONS})
#target_link_libraries(${PROJECT_NAME}_proto PRIVATE protobuf::libprotobuf)
add_dependencies(${PROJECT_NAME}_proto protobuf::libprotobuf)
add_library(${PROJECT_NAMESPACE}::proto ALIAS ${PROJECT_NAME}_proto)
# Add ${PROJECT_NAMESPACE}::proto to libortools
#target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAMESPACE}::proto)
target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${PROJECT_NAMESPACE}::proto>)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAMESPACE}::proto)

foreach(SUBPROJECT IN ITEMS
 algorithms
 base
 bop
 constraint_solver
 ${GLPK_DIR}
 ${PDLP_DIR}
 ${GSCIP_DIR}
 glop
 graph
 gurobi
 init
 linear_solver
 lp_data
 packing
 port
 sat
 scheduling
 util)
  add_subdirectory(ortools/${SUBPROJECT})
  #target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_${SUBPROJECT})
  target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${PROJECT_NAME}_${SUBPROJECT}>)
  add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_${SUBPROJECT})
endforeach()

add_subdirectory(ortools/linear_solver/wrappers)
target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${PROJECT_NAME}_linear_solver_wrappers>)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_linear_solver_wrappers)

add_subdirectory(ortools/linear_solver/proto_solver)
target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${PROJECT_NAME}_linear_solver_proto_solver>)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_linear_solver_proto_solver)

###################
## Install rules ##
###################
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
  NAMESPACE ${PROJECT_NAMESPACE}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
install(DIRECTORY ortools
  TYPE INCLUDE
  COMPONENT Devel
  FILES_MATCHING
  PATTERN "*.h")
install(DIRECTORY ${PROJECT_BINARY_DIR}/ortools
  TYPE INCLUDE
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
if(BUILD_LP_PARSER)
  install(
    FILES
    "${PROJECT_SOURCE_DIR}/cmake/Findre2.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}/modules"
    COMPONENT Devel)
endif()
if(USE_COINOR)
  install(
    FILES
    "${PROJECT_SOURCE_DIR}/cmake/FindCbc.cmake"
    "${PROJECT_SOURCE_DIR}/cmake/FindClp.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}/modules"
    COMPONENT Devel)
endif()
if(USE_GLPK)
  install(
    FILES
    "${PROJECT_SOURCE_DIR}/cmake/FindGLPK.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}/modules"
    COMPONENT Devel)
endif()
if(USE_PDLP)
  install(
    FILES
    "${PROJECT_SOURCE_DIR}/cmake/FindEigen3.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}/modules"
    COMPONENT Devel)
endif()
if(USE_SCIP)
  install(
    FILES
    "${PROJECT_SOURCE_DIR}/cmake/FindSCIP.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}/modules"
    COMPONENT Devel)
endif()

if(MSVC)
# Bundle lib for MSVC
configure_file(
${PROJECT_SOURCE_DIR}/cmake/bundle-install.cmake.in
${PROJECT_BINARY_DIR}/bundle-install.cmake
@ONLY)
install(SCRIPT ${PROJECT_BINARY_DIR}/bundle-install.cmake)
endif()

install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
  DESTINATION "${CMAKE_INSTALL_DOCDIR}"
  COMPONENT Devel)
if(INSTALL_DOC)
install(DIRECTORY ortools/sat/docs/
  DESTINATION "${CMAKE_INSTALL_DOCDIR}/sat"
  FILES_MATCHING
  PATTERN "*.md")
install(DIRECTORY ortools/constraint_solver/docs/
  DESTINATION "${CMAKE_INSTALL_DOCDIR}/constraint_solver"
  FILES_MATCHING
  PATTERN "*.md")
endif()

############################
## Samples/Examples/Tests ##
############################
# add_cxx_sample()
# CMake function to generate and build C++ sample.
# Parameters:
#  the C++ filename
# e.g.:
# add_cxx_sample(foo.cc)
function(add_cxx_sample FILE_NAME)
  message(STATUS "Configuring sample ${FILE_NAME}: ...")
  get_filename_component(SAMPLE_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(SAMPLE_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_DIR ${SAMPLE_DIR} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  if(APPLE)
    set(CMAKE_INSTALL_RPATH
      "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
  elseif(UNIX)
    set(CMAKE_INSTALL_RPATH
      "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN/../lib64:$ORIGIN/../lib:$ORIGIN")
  endif()

  add_executable(${SAMPLE_NAME} ${FILE_NAME})
  target_include_directories(${SAMPLE_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(${SAMPLE_NAME} PRIVATE cxx_std_17)
  target_link_libraries(${SAMPLE_NAME} PRIVATE ${PROJECT_NAMESPACE}::ortools)

  include(GNUInstallDirs)
  install(TARGETS ${SAMPLE_NAME})

  if(BUILD_TESTING)
    add_test(NAME cxx_${COMPONENT_NAME}_${SAMPLE_NAME} COMMAND ${SAMPLE_NAME})
  endif()
  message(STATUS "Configuring sample ${FILE_NAME}: ...DONE")
endfunction()

# add_cxx_example()
# CMake function to generate and build C++ example.
# Parameters:
#  the C++ filename
# e.g.:
# add_cxx_example(foo.cc)
function(add_cxx_example FILE_NAME)
  message(STATUS "Configuring example ${FILE_NAME}: ...")
  get_filename_component(EXAMPLE_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(COMPONENT_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  if(APPLE)
    set(CMAKE_INSTALL_RPATH
      "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
  elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN/../lib64:$ORIGIN/../lib:$ORIGIN")
  endif()

  add_executable(${EXAMPLE_NAME} ${FILE_NAME})
  target_include_directories(${EXAMPLE_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(${EXAMPLE_NAME} PRIVATE cxx_std_17)
  target_link_libraries(${EXAMPLE_NAME} PRIVATE ${PROJECT_NAMESPACE}::ortools)

  include(GNUInstallDirs)
  install(TARGETS ${EXAMPLE_NAME})

  if(BUILD_TESTING)
    add_test(NAME cxx_${COMPONENT_NAME}_${EXAMPLE_NAME} COMMAND ${EXAMPLE_NAME})
  endif()
  message(STATUS "Configuring example ${FILE_NAME}: ...DONE")
endfunction()

# add_cxx_test()
# CMake function to generate and build C++ test.
# Parameters:
#  the C++ filename
# e.g.:
# add_cxx_test(foo.cc)
function(add_cxx_test FILE_NAME)
  message(STATUS "Configuring test ${FILE_NAME}: ...")
  get_filename_component(TEST_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(COMPONENT_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  if(APPLE)
    set(CMAKE_INSTALL_RPATH
      "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
  elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:$ORIGIN/../lib64:$ORIGIN/../lib:$ORIGIN")
  endif()

  add_executable(${TEST_NAME} ${FILE_NAME})
  target_include_directories(${TEST_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(${TEST_NAME} PRIVATE cxx_std_17)
  target_link_libraries(${TEST_NAME} PRIVATE ${PROJECT_NAMESPACE}::ortools)

  if(BUILD_TESTING)
    add_test(NAME cxx_${COMPONENT_NAME}_${TEST_NAME} COMMAND ${TEST_NAME})
  endif()
  message(STATUS "Configuring test ${FILE_NAME}: ...DONE")
endfunction()
