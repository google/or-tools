# Copyright 2010-2024 Google LLC
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

# Basic type
include(CMakePushCheckState)
cmake_push_check_state(RESET)
set(CMAKE_EXTRA_INCLUDE_FILES "cstdint")
include(CheckTypeSize)
check_type_size("long" SIZEOF_LONG LANGUAGE CXX)
message(STATUS "Found long size: ${SIZEOF_LONG}")
check_type_size("long long" SIZEOF_LONG_LONG LANGUAGE CXX)
message(STATUS "Found long long size: ${SIZEOF_LONG_LONG}")
check_type_size("int64_t" SIZEOF_INT64_T LANGUAGE CXX)
message(STATUS "Found int64_t size: ${SIZEOF_INT64_T}")

check_type_size("unsigned long" SIZEOF_ULONG LANGUAGE CXX)
message(STATUS "Found unsigned long size: ${SIZEOF_ULONG}")
check_type_size("unsigned long long" SIZEOF_ULONG_LONG LANGUAGE CXX)
message(STATUS "Found unsigned long long size: ${SIZEOF_ULONG_LONG}")
check_type_size("uint64_t" SIZEOF_UINT64_T LANGUAGE CXX)
message(STATUS "Found uint64_t size: ${SIZEOF_UINT64_T}")

check_type_size("int *" SIZEOF_INT_P LANGUAGE CXX)
message(STATUS "Found int * size: ${SIZEOF_INT_P}")
cmake_pop_check_state()

#############
##  FLAGS  ##
#############
set(OR_TOOLS_COMPILE_DEFINITIONS)
set(OR_TOOLS_COMPILE_OPTIONS)
set(OR_TOOLS_LINK_OPTIONS)

if(BUILD_SHARED_LIBS)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "OR_TOOLS_AS_DYNAMIC_LIB")
endif()
# Optional built-in components
if(BUILD_LP_PARSER)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "USE_LP_PARSER")
endif()
if(BUILD_MATH_OPT)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "USE_MATH_OPT")
  set(MATH_OPT_DIR math_opt)
endif()
# Optional solvers
if(USE_BOP)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "USE_BOP")
endif()
if(USE_COINOR)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS
    "USE_CBC" # enable COIN-OR CBC support
    "USE_CLP" # enable COIN-OR CLP support
  )
endif()
if(USE_GLOP)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "USE_GLOP")
endif()
if(USE_GLPK)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "USE_GLPK")
  set(GLPK_DIR glpk)
endif()
if(USE_GUROBI)
  set(GUROBI_DIR gurobi)
endif()
if(USE_HIGHS)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "USE_HIGHS")
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

if(WIN32)
  list(APPEND OR_TOOLS_COMPILE_DEFINITIONS "__WIN32__")
endif()

# Compiler options
if(MSVC)
  list(APPEND OR_TOOLS_COMPILE_OPTIONS
    "/bigobj" # Allow big object
    "/DNOMINMAX"
    "/DWIN32_LEAN_AND_MEAN=1"
    "/D_CRT_SECURE_NO_WARNINGS"
    "/D_CRT_SECURE_NO_DEPRECATE"
    "/MP" # Build with multiple processes
    "/Zc:preprocessor" # Enable preprocessor conformance mode
    "/fp:precise"
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

# Link option
if(MSVC)
  list(APPEND OR_TOOLS_LINK_OPTIONS
   "/WHOLEARCHIVE:${PROJECT_NAME}"
  )
endif()

##################
##  PROTO FILE  ##
##################
# Get Protobuf include dir
set(PROTO_DIRS)
get_target_property(protobuf_dirs protobuf::libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
foreach(dir IN LISTS protobuf_dirs)
  if (NOT "${dir}" MATCHES "INSTALL_INTERFACE|-NOTFOUND")
    #message(STATUS "Adding proto path: ${dir}")
    list(APPEND PROTO_DIRS "--proto_path=${dir}")
  endif()
endforeach()

# Generate C++ OBJECT library from proto files,
# e.g
# generate_proto_library(
#   NAME
#     ortools_proto
#   FILES
#     ortools/foo/foo.proto
#     ortools/bar/bar.proto
#  LINK_LIBRARIES
#     ortools::ortools_proto
#   NO_ALIAS
# )
function(generate_proto_library)
  set(options NO_ALIAS)
  set(oneValueArgs NAME)
  set(multiValueArgs FILES LINK_LIBRARIES)
  cmake_parse_arguments(PROTO
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )

 if(NOT PROTOC_PRG)
   message(FATAL_ERROR "protoc binary not found.")
 endif()

 # Generate proto C++ files.
 set(PROTO_HDRS)
 set(PROTO_SRCS)
 foreach(PROTO_FILE IN LISTS PROTO_FILES)
   #message(STATUS "protoc proto(cc): ${PROTO_FILE}")
   get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
   get_filename_component(PROTO_NAME_WE ${PROTO_FILE} NAME_WE)
   set(PROTO_HDR ${PROJECT_BINARY_DIR}/${PROTO_DIR}/${PROTO_NAME_WE}.pb.h)
   set(PROTO_SRC ${PROJECT_BINARY_DIR}/${PROTO_DIR}/${PROTO_NAME_WE}.pb.cc)
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

 # Create library
 add_library(${PROTO_NAME}_proto OBJECT ${PROTO_SRCS} ${PROTO_HDRS})
 target_compile_features(${PROTO_NAME}_proto PUBLIC $<IF:$<CXX_COMPILER_ID:MSVC>,cxx_std_20,cxx_std_17>)
 if(MSVC)
   set_target_properties(${PROTO_NAME}_proto PROPERTIES CXX_STANDARD 20)
 else()
   set_target_properties(${PROTO_NAME}_proto PROPERTIES CXX_STANDARD 17)
 endif()
 set_target_properties(${PROTO_NAME}_proto PROPERTIES
   CXX_STANDARD_REQUIRED ON
   CXX_EXTENSIONS OFF
   POSITION_INDEPENDENT_CODE ON)
 target_include_directories(${PROTO_NAME}_proto PRIVATE
   ${PROJECT_SOURCE_DIR}
   ${PROJECT_BINARY_DIR}
   #$<TARGET_PROPERTY:protobuf::libprotobuf,INTERFACE_INCLUDE_DIRECTORIES>
 )
 target_compile_definitions(${PROTO_NAME}_proto PUBLIC ${OR_TOOLS_COMPILE_DEFINITIONS})
 target_compile_options(${PROTO_NAME}_proto PUBLIC ${OR_TOOLS_COMPILE_OPTIONS})
 target_link_libraries(${PROTO_NAME}_proto PUBLIC protobuf::libprotobuf ${PROTO_LINK_LIBRARIES})
 add_library(${PROJECT_NAMESPACE}::${PROTO_NAME}_proto ALIAS ${PROTO_NAME}_proto)
 #message(FATAL_ERROR "Proto target alias: ${PROJECT_NAMESPACE}::${PROTO_NAME}_proto")
endfunction()

# Generate Protobuf cpp sources
set(OR_TOOLS_PROTO_FILES)
file(GLOB_RECURSE OR_TOOLS_PROTO_FILES RELATIVE ${PROJECT_SOURCE_DIR}
  "ortools/algorithms/*.proto"
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
if(USE_PDLP OR BUILD_MATH_OPT)
  file(GLOB_RECURSE PDLP_PROTO_FILES RELATIVE ${PROJECT_SOURCE_DIR} "ortools/pdlp/*.proto")
  list(APPEND OR_TOOLS_PROTO_FILES ${PDLP_PROTO_FILES})
endif()
if(USE_SCIP OR BUILD_MATH_OPT)
  file(GLOB_RECURSE GSCIP_PROTO_FILES RELATIVE ${PROJECT_SOURCE_DIR} "ortools/gscip/*.proto")
  list(APPEND OR_TOOLS_PROTO_FILES ${GSCIP_PROTO_FILES})
endif()

# ORTools proto
generate_proto_library(
  NAME ortools
  FILES ${OR_TOOLS_PROTO_FILES})

# MathOpt proto
if(BUILD_MATH_OPT)
  file(GLOB_RECURSE MATH_OPT_PROTO_FILES RELATIVE ${PROJECT_SOURCE_DIR}
    "ortools/math_opt/*.proto"
    "ortools/math_opt/solvers/*.proto"
  )
  generate_proto_library(
    NAME math_opt
    FILES ${MATH_OPT_PROTO_FILES}
    LINK_LIBRARIES ${PROJECT_NAMESPACE}::ortools_proto)
endif()

###############
##  ORTOOLS  ##
###############
# Main Target
add_library(${PROJECT_NAME} "")
# Compile options
if(MSVC)
  set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
else()
  set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF)
target_compile_features(${PROJECT_NAME} PUBLIC
  $<IF:$<CXX_COMPILER_ID:MSVC>,cxx_std_20,cxx_std_17>)
target_compile_definitions(${PROJECT_NAME} PUBLIC ${OR_TOOLS_COMPILE_DEFINITIONS})
target_compile_options(${PROJECT_NAME} PUBLIC ${OR_TOOLS_COMPILE_OPTIONS})
target_link_options(${PROJECT_NAME} INTERFACE ${OR_TOOLS_LINK_OPTIONS})
# Properties
if(NOT APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION})
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
  INTERFACE_${PROJECT_NAME}_MAJOR_VERSION ${PROJECT_VERSION_MAJOR}
  COMPATIBLE_INTERFACE_STRING ${PROJECT_NAME}_MAJOR_VERSION
)

# Includes
target_include_directories(${PROJECT_NAME} INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
  $<INSTALL_INTERFACE:include>
)
# Xcode fails to build if library doesn't contains at least one source file.
if(XCODE)
  file(GENERATE
    OUTPUT ${PROJECT_BINARY_DIR}/${PROJECT_NAME}/version.cpp
    CONTENT "namespace {char* version = \"${PROJECT_VERSION}\";}")
  target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}/version.cpp)
endif()

# Add ${PROJECT_NAMESPACE}::ortools_proto to libortools
target_sources(${PROJECT_NAME} PRIVATE
  $<TARGET_OBJECTS:${PROJECT_NAMESPACE}::ortools_proto>)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAMESPACE}::ortools_proto)

if(BUILD_MATH_OPT)
  # Add ${PROJECT_NAMESPACE}::math_opt_proto to libortools
  target_sources(${PROJECT_NAME} PRIVATE
    $<TARGET_OBJECTS:${PROJECT_NAMESPACE}::math_opt_proto>)
  add_dependencies(${PROJECT_NAME} ${PROJECT_NAMESPACE}::math_opt_proto)
endif()

foreach(SUBPROJECT IN ITEMS
 base
 init
 algorithms
 graph
 constraint_solver
 linear_solver
 bop
 glop
 ${GLPK_DIR}
 ${GSCIP_DIR}
 ${GUROBI_DIR}
 ${PDLP_DIR}
 sat
 xpress
 knitro
 lp_data
 packing
 scheduling
 port
 util)
  add_subdirectory(ortools/${SUBPROJECT})
  #target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_${SUBPROJECT})
  target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${PROJECT_NAME}_${SUBPROJECT}>)
  add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_${SUBPROJECT})
endforeach()

if(BUILD_MATH_OPT)
  add_subdirectory(ortools/${MATH_OPT_DIR})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_math_opt)
endif()

add_subdirectory(ortools/linear_solver/wrappers)
target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${PROJECT_NAME}_linear_solver_wrappers>)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_linear_solver_wrappers)

add_subdirectory(ortools/linear_solver/proto_solver)
target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${PROJECT_NAME}_linear_solver_proto_solver>)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_linear_solver_proto_solver)

# Dependencies
if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "@loader_path")
elseif(UNIX)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "$ORIGIN")
endif()
target_link_libraries(${PROJECT_NAME} PUBLIC
  ${CMAKE_DL_LIBS}
  ZLIB::ZLIB
  ${ABSL_DEPS}
  protobuf::libprotobuf
  ${RE2_DEPS}
  ${COINOR_DEPS}
  $<$<BOOL:${USE_CPLEX}>:CPLEX::CPLEX>
  $<$<BOOL:${USE_GLPK}>:GLPK::GLPK>
  $<$<BOOL:${USE_HIGHS}>:highs::highs>
  ${PDLP_DEPS}
  $<$<BOOL:${USE_SCIP}>:libscip>
  Threads::Threads)
if(WIN32)
  target_link_libraries(${PROJECT_NAME} PUBLIC psapi.lib ws2_32.lib)
endif()
# ALIAS
add_library(${PROJECT_NAMESPACE}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

###############
## Doc rules ##
###############
if(BUILD_CXX_DOC)
  # add a target to generate API documentation with Doxygen
  find_package(Doxygen REQUIRED)
  if(DOXYGEN_FOUND)
    configure_file(${PROJECT_SOURCE_DIR}/ortools/cpp/Doxyfile.in ${PROJECT_BINARY_DIR}/cpp/Doxyfile @ONLY)
    file(DOWNLOAD
      https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/v2.1.0/doxygen-awesome.css
      ${PROJECT_BINARY_DIR}/cpp/doxygen-awesome.css
      SHOW_PROGRESS
    )
    add_custom_target(${PROJECT_NAME}_cxx_doc ALL
      #COMMAND ${CMAKE_COMMAND} -E rm -rf ${PROJECT_BINARY_DIR}/docs/cpp
      COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/docs/cpp
      COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/cpp/Doxyfile
      DEPENDS
        ${PROJECT_BINARY_DIR}/cpp/Doxyfile
        ${PROJECT_BINARY_DIR}/cpp/doxygen-awesome.css
        ${PROJECT_SOURCE_DIR}/ortools/cpp/stylesheet.css
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      COMMENT "Generating C++ API documentation with Doxygen"
      VERBATIM)
  else()
    message(WARNING "cmd `doxygen` not found, C++ doc generation is disable!")
  endif()
endif()

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

################
##  C++ Test  ##
################
# add_cxx_test()
# CMake function to generate and build C++ test.
# Parameters:
#  FILE_NAME: the C++ filename
#  COMPONENT_NAME: name of the ortools/ subdir where the test is located
#  note: automatically determined if located in ortools/<component>/
# e.g.:
# add_cxx_test(
#   FILE_NAME
#     ${PROJECT_SOURCE_DIR}/ortools/foo/foo_test.cc
#   COMPONENT_NAME
#     foo
# )
function(add_cxx_test)
  set(options "")
  set(oneValueArgs FILE_NAME COMPONENT_NAME)
  set(multiValueArgs "")
  cmake_parse_arguments(TEST
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
if(NOT TEST_FILE_NAME)
    message(FATAL_ERROR "no FILE_NAME provided")
  endif()
  get_filename_component(TEST_NAME ${TEST_FILE_NAME} NAME_WE)

  message(STATUS "Configuring test ${TEST_FILE_NAME} ...")

  if(NOT TEST_COMPONENT_NAME)
    # test is located in ortools/<component_name>/
    get_filename_component(COMPONENT_DIR ${TEST_FILE_NAME} DIRECTORY)
    get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)
  else()
    set(COMPONENT_NAME ${TEST_COMPONENT_NAME})
  endif()

  add_executable(${TEST_NAME} ${TEST_FILE_NAME})
  target_include_directories(${TEST_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(${TEST_NAME} PRIVATE cxx_std_17)
  target_link_libraries(${TEST_NAME} PRIVATE ${PROJECT_NAMESPACE}::ortools)

  include(GNUInstallDirs)
  if(APPLE)
    set_target_properties(${TEST_NAME} PROPERTIES INSTALL_RPATH
      "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
  elseif(UNIX)
    cmake_path(RELATIVE_PATH CMAKE_INSTALL_FULL_LIBDIR
      BASE_DIRECTORY ${CMAKE_INSTALL_FULL_BINDIR}
      OUTPUT_VARIABLE libdir_relative_path)
    set_target_properties(${TEST_NAME} PROPERTIES
      INSTALL_RPATH "$ORIGIN/${libdir_relative_path}")
  endif()

  if(BUILD_TESTING)
    add_test(
      NAME cxx_${COMPONENT_NAME}_${TEST_NAME}
      COMMAND ${TEST_NAME})
  endif()
  message(STATUS "Configuring test ${TEST_FILE_NAME} ...DONE")
endfunction()

##################
##  C++ Sample  ##
##################
# add_cxx_sample()
# CMake function to generate and build C++ sample.
# Parameters:
#  FILE_NAME: the C++ filename
#  COMPONENT_NAME: name of the ortools/ subdir where the test is located
#  note: automatically determined if located in ortools/<component>/samples/
# e.g.:
# add_cxx_sample(
#   FILE_NAME
#     ${PROJECT_SOURCE_DIR}/ortools/foo/sample/bar.cc
#   COMPONENT_NAME
#     foo
# )
function(add_cxx_sample)
  set(options "")
  set(oneValueArgs FILE_NAME COMPONENT_NAME)
  set(multiValueArgs "")
  cmake_parse_arguments(SAMPLE
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
  if(NOT SAMPLE_FILE_NAME)
    message(FATAL_ERROR "no FILE_NAME provided")
  endif()
  get_filename_component(SAMPLE_NAME ${SAMPLE_FILE_NAME} NAME_WE)

  message(STATUS "Configuring sample ${SAMPLE_FILE_NAME} ...")

  if(NOT SAMPLE_COMPONENT_NAME)
    # sample is located in ortools/<component_name>/sample/
    get_filename_component(SAMPLE_DIR ${SAMPLE_FILE_NAME} DIRECTORY)
    get_filename_component(COMPONENT_DIR ${SAMPLE_DIR} DIRECTORY)
    get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)
  else()
    set(COMPONENT_NAME ${SAMPLE_COMPONENT_NAME})
  endif()

  add_executable(${SAMPLE_NAME} ${SAMPLE_FILE_NAME})
  target_include_directories(${SAMPLE_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(${SAMPLE_NAME} PRIVATE cxx_std_17)
  target_link_libraries(${SAMPLE_NAME} PRIVATE ${PROJECT_NAMESPACE}::ortools)

  include(GNUInstallDirs)
  if(APPLE)
    set_target_properties(${SAMPLE_NAME} PROPERTIES INSTALL_RPATH
      "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
  elseif(UNIX)
    cmake_path(RELATIVE_PATH CMAKE_INSTALL_FULL_LIBDIR
      BASE_DIRECTORY ${CMAKE_INSTALL_FULL_BINDIR}
      OUTPUT_VARIABLE libdir_relative_path)
    set_target_properties(${SAMPLE_NAME} PROPERTIES
      INSTALL_RPATH "$ORIGIN/${libdir_relative_path}")
  endif()
  install(TARGETS ${SAMPLE_NAME})

  if(BUILD_TESTING)
    add_test(
      NAME cxx_${COMPONENT_NAME}_${SAMPLE_NAME}
      COMMAND ${SAMPLE_NAME})
  endif()
  message(STATUS "Configuring sample ${SAMPLE_FILE_NAME} ...DONE")
endfunction()

###################
##  C++ Example  ##
###################
# add_cxx_example()
# CMake function to generate and build C++ example.
# Parameters:
#  FILE_NAME: the C++ filename
#  COMPONENT_NAME: name of the examples/ subdir where the test is located
#  note: automatically determined if located in examples/<subdir>/
# e.g.:
# add_cxx_example(
#   FILE_NAME
#     ${PROJECT_SOURCE_DIR}/example/foo/bar.cc
#   COMPONENT_NAME
#     foo
# )
function(add_cxx_example)
  set(options "")
  set(oneValueArgs FILE_NAME COMPONENT_NAME)
  set(multiValueArgs "")
  cmake_parse_arguments(EXAMPLE
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
  if(NOT EXAMPLE_FILE_NAME)
    message(FATAL_ERROR "no FILE_NAME provided")
  endif()
  get_filename_component(EXAMPLE_NAME ${EXAMPLE_FILE_NAME} NAME_WE)

  message(STATUS "Configuring example ${EXAMPLE_FILE_NAME} ...")

  if(NOT EXAMPLE_COMPONENT_NAME)
    # example is located in examples/<component_name>/
    get_filename_component(EXAMPLE_DIR ${EXAMPLE_FILE_NAME} DIRECTORY)
    get_filename_component(COMPONENT_NAME ${EXAMPLE_DIR} NAME)
  else()
    set(COMPONENT_NAME ${EXAMPLE_COMPONENT_NAME})
  endif()

  add_executable(${EXAMPLE_NAME} ${EXAMPLE_FILE_NAME})
  target_include_directories(${EXAMPLE_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(${EXAMPLE_NAME} PRIVATE cxx_std_17)
  target_link_libraries(${EXAMPLE_NAME} PRIVATE ${PROJECT_NAMESPACE}::ortools)

  include(GNUInstallDirs)
  if(APPLE)
    set_target_properties(${EXAMPLE_NAME} PROPERTIES INSTALL_RPATH
      "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
  elseif(UNIX)
    cmake_path(RELATIVE_PATH CMAKE_INSTALL_FULL_LIBDIR
      BASE_DIRECTORY ${CMAKE_INSTALL_FULL_BINDIR}
      OUTPUT_VARIABLE libdir_relative_path)
    set_target_properties(${EXAMPLE_NAME} PROPERTIES
      INSTALL_RPATH "$ORIGIN/${libdir_relative_path}")
  endif()
  install(TARGETS ${EXAMPLE_NAME})

  if(BUILD_TESTING)
    add_test(
      NAME cxx_${COMPONENT_NAME}_${EXAMPLE_NAME}
      COMMAND ${EXAMPLE_NAME})
  endif()
  message(STATUS "Configuring example ${EXAMPLE_FILE_NAME} ...DONE")
endfunction()
