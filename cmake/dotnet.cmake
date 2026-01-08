# Copyright 2010-2025 Google LLC
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

if(NOT BUILD_DOTNET)
  return()
endif()

if(NOT TARGET ${PROJECT_NAMESPACE}::ortools)
  message(FATAL_ERROR ".Net: missing ortools TARGET")
endif()

# Will need swig
set(SWIG_SOURCE_FILE_EXTENSIONS ".i" ".swig")
set(CMAKE_SWIG_FLAGS)
find_package(SWIG REQUIRED)
include(UseSWIG)

#if(${SWIG_VERSION} VERSION_GREATER_EQUAL 4)
#  list(APPEND CMAKE_SWIG_FLAGS "-doxygen")
#endif()

if(UNIX AND NOT APPLE)
  list(APPEND CMAKE_SWIG_FLAGS "-DSWIGWORDSIZE64")
endif()
list(APPEND CMAKE_SWIG_FLAGS "-DOR_DLL=")

# Find dotnet cli
find_program(DOTNET_EXECUTABLE NAMES dotnet)
if(NOT DOTNET_EXECUTABLE)
  message(FATAL_ERROR "Check for dotnet Program: not found")
else()
  message(STATUS "Found dotnet Program: ${DOTNET_EXECUTABLE}")
endif()

# Needed by dotnet/CMakeLists.txt
set(DOTNET_PACKAGE Google.OrTools)
set(DOTNET_PACKAGES_DIR "${PROJECT_BINARY_DIR}/dotnet/packages")

# Runtime IDentifier
# see: https://docs.microsoft.com/en-us/dotnet/core/rid-catalog
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)")
  set(DOTNET_PLATFORM arm64)
else()
  set(DOTNET_PLATFORM x64)
endif()

if (CMAKE_GENERATOR_PLATFORM MATCHES "^(aarch64|arm64|ARM64)")
  set(DOTNET_PLATFORM arm64)
elseif(CMAKE_GENERATOR_PLATFORM MATCHES "^(X64|x64)")
  set(DOTNET_PLATFORM x64)
endif()

if(APPLE)
  set(DOTNET_RID osx-${DOTNET_PLATFORM})
elseif(UNIX)
  set(DOTNET_RID linux-${DOTNET_PLATFORM})
elseif(WIN32)
  set(DOTNET_RID win-${DOTNET_PLATFORM})
else()
  message(FATAL_ERROR "Unsupported system !")
endif()
message(STATUS ".Net RID: ${DOTNET_RID}")

set(DOTNET_NATIVE_PROJECT ${DOTNET_PACKAGE}.runtime.${DOTNET_RID})
message(STATUS ".Net runtime project: ${DOTNET_NATIVE_PROJECT}")
set(DOTNET_NATIVE_PROJECT_DIR ${PROJECT_BINARY_DIR}/dotnet/${DOTNET_NATIVE_PROJECT})
message(STATUS ".Net runtime project build path: ${DOTNET_NATIVE_PROJECT_DIR}")

# Targeted Framework Moniker
# see: https://docs.microsoft.com/en-us/dotnet/standard/frameworks
# see: https://learn.microsoft.com/en-us/dotnet/standard/net-standard
if(USE_DOTNET_46)
  list(APPEND TFM "net46")
endif()
if(USE_DOTNET_461)
  list(APPEND TFM "net461")
endif()
if(USE_DOTNET_462)
  list(APPEND TFM "net462")
endif()
if(USE_DOTNET_48)
  list(APPEND TFM "net48")
endif()
if(USE_DOTNET_STD_21)
  list(APPEND TFM "netstandard2.1")
endif()
if(USE_DOTNET_CORE_31)
  list(APPEND TFM "netcoreapp3.1")
endif()
if(USE_DOTNET_6)
  list(APPEND TFM "net6.0")
endif()
if(USE_DOTNET_7)
  list(APPEND TFM "net7.0")
endif()
if(USE_DOTNET_8)
  list(APPEND TFM "net8.0")
endif()
if(USE_DOTNET_9)
  list(APPEND TFM "net9.0")
endif()

list(LENGTH TFM TFM_LENGTH)
if(TFM_LENGTH EQUAL "0")
  message(FATAL_ERROR "No .Net SDK selected !")
endif()

string(JOIN ";" DOTNET_TFM ${TFM})
message(STATUS ".Net TFM: ${DOTNET_TFM}")
if(TFM_LENGTH GREATER "1")
  string(CONCAT DOTNET_TFM "<TargetFrameworks>" "${DOTNET_TFM}" "</TargetFrameworks>")
else()
  string(CONCAT DOTNET_TFM "<TargetFramework>" "${DOTNET_TFM}" "</TargetFramework>")
endif()

set(DOTNET_PROJECT ${DOTNET_PACKAGE})
message(STATUS ".Net project: ${DOTNET_PROJECT}")
set(DOTNET_PROJECT_DIR ${PROJECT_BINARY_DIR}/dotnet/${DOTNET_PROJECT})
message(STATUS ".Net project build path: ${DOTNET_PROJECT_DIR}")

if(RELEASE)
  set(DOTNET_RELEASE "")
else()
  set(DOTNET_RELEASE "-rc.1")
endif()

##################
##  PROTO FILE  ##
##################
# Generate Protobuf .Net sources
set(PROTO_DOTNETS)
file(GLOB_RECURSE proto_dotnet_files RELATIVE ${PROJECT_SOURCE_DIR}
  "ortools/algorithms/*.proto"
  "ortools/bop/*.proto"
  "ortools/constraint_solver/*.proto"
  "ortools/glop/*.proto"
  "ortools/graph/*.proto"
  "ortools/linear_solver/*.proto"
  "ortools/routing/*.proto"
  "ortools/sat/*.proto"
  "ortools/util/*.proto"
  )
if(USE_PDLP)
  file(GLOB_RECURSE pdlp_proto_dotnet_files RELATIVE ${PROJECT_SOURCE_DIR} "ortools/pdlp/*.proto")
  list(APPEND proto_dotnet_files ${pdlp_proto_dotnet_files})
endif()
list(REMOVE_ITEM proto_dotnet_files "ortools/constraint_solver/demon_profiler.proto")
list(REMOVE_ITEM proto_dotnet_files "ortools/constraint_solver/assignment.proto")
foreach(PROTO_FILE IN LISTS proto_dotnet_files)
  #message(STATUS "protoc proto(dotnet): ${PROTO_FILE}")
  get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(PROTO_DOTNET ${DOTNET_PROJECT_DIR}/${PROTO_DIR}/${PROTO_NAME}.pb.cs)
  #message(STATUS "protoc dotnet: ${PROTO_DOTNET}")
  add_custom_command(
    OUTPUT ${PROTO_DOTNET}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DOTNET_PROJECT_DIR}/${PROTO_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/dotnet/${PROTO_DIR}
    COMMAND ${PROTOC_PRG}
    "--proto_path=${PROJECT_SOURCE_DIR}"
    ${PROTO_DIRS}
    "--csharp_out=${DOTNET_PROJECT_DIR}/${PROTO_DIR}"
    "--csharp_opt=file_extension=.pb.cs"
    ${PROTO_FILE}
    DEPENDS ${PROTO_FILE} ${PROTOC_PRG}
    COMMENT "Generate C# protocol buffer for ${PROTO_FILE}"
    VERBATIM)
  list(APPEND PROTO_DOTNETS ${PROTO_DOTNET})
endforeach()
add_custom_target(Dotnet${PROJECT_NAME}_proto
  DEPENDS
    ${PROTO_DOTNETS}
    ${PROJECT_NAMESPACE}::ortools)

# Create the native library
add_library(google-ortools-native SHARED "")
set_target_properties(google-ortools-native PROPERTIES
  PREFIX ""
  POSITION_INDEPENDENT_CODE ON)
# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(google-ortools-native PROPERTIES INSTALL_RPATH "@loader_path")
  # Xcode fails to build if library doesn't contains at least one source file.
  if(XCODE)
    file(GENERATE
      OUTPUT ${PROJECT_BINARY_DIR}/google-ortools-native/version.cpp
      CONTENT "namespace {char* version = \"${PROJECT_VERSION}\";}")
    target_sources(google-ortools-native PRIVATE ${PROJECT_BINARY_DIR}/google-ortools-native/version.cpp)
  endif()
elseif(UNIX)
  set_target_properties(google-ortools-native PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

#################
##  .Net Test  ##
#################
# add_dotnet_test()
# CMake function to generate and build dotnet test.
# Parameters:
#  FILE_NAME: the .Net filename
#  COMPONENT_NAME: name of the ortools/ subdir where the test is located
#  note: automatically determined if located in ortools/<component>/dotnet/
# e.g.:
# add_dotnet_test(
#   FILE_NAME
#     ${PROJECT_SOURCE_DIR}/ortools/foo/dotnet/BarTests.cs
#   COMPONENT_NAME
#     foo
# )
function(add_dotnet_test)
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
    # test is located in ortools/<component_name>/dotnet/
    get_filename_component(WRAPPER_DIR ${TEST_FILE_NAME} DIRECTORY)
    get_filename_component(COMPONENT_DIR ${WRAPPER_DIR} DIRECTORY)
    get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)
  else()
    set(COMPONENT_NAME ${TEST_COMPONENT_NAME})
  endif()

  set(DOTNET_TEST_DIR ${PROJECT_BINARY_DIR}/dotnet/${COMPONENT_NAME}/${TEST_NAME})
  message(STATUS "build path: ${DOTNET_TEST_DIR}")

  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/dotnet/Test.csproj.in
    ${DOTNET_TEST_DIR}/${TEST_NAME}.csproj
    @ONLY)

  add_custom_command(
    OUTPUT ${DOTNET_TEST_DIR}/${TEST_NAME}.cs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DOTNET_TEST_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy
      ${TEST_FILE_NAME}
      ${DOTNET_TEST_DIR}/
    MAIN_DEPENDENCY ${TEST_FILE_NAME}
    VERBATIM
    WORKING_DIRECTORY ${DOTNET_TEST_DIR})

  add_custom_command(
    OUTPUT ${DOTNET_TEST_DIR}/timestamp
    COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
    ${DOTNET_EXECUTABLE} build --nologo -c Release ${TEST_NAME}.csproj
    COMMAND ${CMAKE_COMMAND} -E touch ${DOTNET_TEST_DIR}/timestamp
    DEPENDS
      ${DOTNET_TEST_DIR}/${TEST_NAME}.csproj
      ${DOTNET_TEST_DIR}/${TEST_NAME}.cs
      dotnet_package
    BYPRODUCTS
      ${DOTNET_TEST_DIR}/bin
      ${DOTNET_TEST_DIR}/obj
    VERBATIM
    COMMENT "Compiling .Net ${COMPONENT_NAME}/${TEST_NAME}.cs (${DOTNET_TEST_DIR}/timestamp)"
    WORKING_DIRECTORY ${DOTNET_TEST_DIR})

  add_custom_target(dotnet_${COMPONENT_NAME}_${TEST_NAME} ALL
    DEPENDS
      ${DOTNET_TEST_DIR}/timestamp
    WORKING_DIRECTORY ${DOTNET_TEST_DIR})

  if(BUILD_TESTING)
    if(USE_DOTNET_6)
      add_test(
        NAME dotnet_${COMPONENT_NAME}_${TEST_NAME}_net60
        COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
          ${DOTNET_EXECUTABLE} test --nologo --framework net6.0 -c Release
          WORKING_DIRECTORY ${DOTNET_TEST_DIR})
    endif()
    if(USE_DOTNET_7)
      add_test(
        NAME dotnet_${COMPONENT_NAME}_${TEST_NAME}_net70
        COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
          ${DOTNET_EXECUTABLE} test --nologo --framework net7.0 -c Release
          WORKING_DIRECTORY ${DOTNET_TEST_DIR})
    endif()
    if(USE_DOTNET_8)
      add_test(
        NAME dotnet_${COMPONENT_NAME}_${TEST_NAME}_net80
        COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
          ${DOTNET_EXECUTABLE} test --nologo --framework net8.0 -c Release
          WORKING_DIRECTORY ${DOTNET_TEST_DIR})
    endif()
    if(USE_DOTNET_9)
      add_test(
        NAME dotnet_${COMPONENT_NAME}_${TEST_NAME}_net90
        COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
          ${DOTNET_EXECUTABLE} test --nologo --framework net9.0 -c Release
          WORKING_DIRECTORY ${DOTNET_TEST_DIR})
    endif()
  endif()
  message(STATUS "Configuring test ${TEST_FILE_NAME} ...DONE")
endfunction()

#######################
##  DOTNET WRAPPERS  ##
#######################
list(APPEND CMAKE_SWIG_FLAGS "-I${PROJECT_SOURCE_DIR}")

# Swig wrap all libraries
foreach(SUBPROJECT IN ITEMS
 algorithms
 graph
 init
 linear_solver
 constraint_solver
 routing
 sat
 util)
  add_subdirectory(ortools/${SUBPROJECT}/csharp)
  target_link_libraries(google-ortools-native PRIVATE dotnet_${SUBPROJECT})
endforeach()

target_link_libraries(google-ortools-native PRIVATE dotnet_model_builder)

file(COPY ${PROJECT_SOURCE_DIR}/tools/doc/orLogo.png DESTINATION ${PROJECT_BINARY_DIR}/dotnet)
set(DOTNET_LOGO_DIR "${PROJECT_BINARY_DIR}/dotnet")

configure_file(
  ${PROJECT_SOURCE_DIR}/ortools/dotnet/README.dotnet.md
  ${PROJECT_BINARY_DIR}/dotnet/README.md
  COPYONLY)
set(DOTNET_README_DIR "${PROJECT_BINARY_DIR}/dotnet")

configure_file(
  ${PROJECT_SOURCE_DIR}/ortools/dotnet/Directory.Build.props.in
  ${PROJECT_BINARY_DIR}/dotnet/Directory.Build.props)

############################
##  .Net SNK file  ##
############################
# Build or retrieve .snk file
if(DEFINED ENV{DOTNET_SNK})
  add_custom_command(
    OUTPUT ${PROJECT_BINARY_DIR}/dotnet/or-tools.snk
    COMMAND ${CMAKE_COMMAND} -E copy $ENV{DOTNET_SNK} ${PROJECT_BINARY_DIR}/dotnet/or-tools.snk
    COMMENT "Copy or-tools.snk from ENV:DOTNET_SNK"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    VERBATIM
    )
else()
  set(DOTNET_SNK_PROJECT CreateSigningKey)
  set(DOTNET_SNK_PROJECT_DIR ${PROJECT_BINARY_DIR}/dotnet/${DOTNET_SNK_PROJECT})
  add_custom_command(
    OUTPUT ${PROJECT_BINARY_DIR}/dotnet/or-tools.snk
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${PROJECT_SOURCE_DIR}/ortools/dotnet/${DOTNET_SNK_PROJECT}
      ${DOTNET_SNK_PROJECT}
    COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME ${DOTNET_EXECUTABLE} run
    --project ${DOTNET_SNK_PROJECT}/${DOTNET_SNK_PROJECT}.csproj
    /or-tools.snk
    BYPRODUCTS
      ${DOTNET_SNK_PROJECT_DIR}/bin
      ${DOTNET_SNK_PROJECT_DIR}/obj
    COMMENT "Generate or-tools.snk using CreateSigningKey project"
    WORKING_DIRECTORY dotnet
    VERBATIM
    )
endif()

file(MAKE_DIRECTORY ${DOTNET_PACKAGES_DIR})
############################
##  .Net Runtime Package  ##
############################
# *.csproj.in contains:
# CMake variable(s) (@PROJECT_NAME@) that configure_file() can manage and
# generator expression ($<TARGET_FILE:...>) that file(GENERATE) can manage.
set(is_windows "$<PLATFORM_ID:Windows>")
set(is_not_windows "$<NOT:$<PLATFORM_ID:Windows>>")

set(need_unix_zlib_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_ZLIB}>>")
set(need_windows_zlib_lib "$<AND:${is_windows},$<BOOL:${BUILD_ZLIB}>>")

set(need_unix_bzip2_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_BZip2}>>")
set(need_windows_bzip2_lib "$<AND:${is_windows},$<BOOL:${BUILD_BZip2}>>")

set(need_unix_absl_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_absl}>>")
set(need_windows_absl_lib "$<AND:${is_windows},$<BOOL:${BUILD_absl}>>")

set(need_unix_re2_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_re2}>>")
set(need_windows_re2_lib "$<AND:${is_windows},$<BOOL:${BUILD_re2}>>")

set(need_unix_protobuf_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_Protobuf}>>")
set(need_windows_protobuf_lib "$<AND:${is_windows},$<BOOL:${BUILD_Protobuf}>>")

set(need_coinutils_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_CoinUtils}>>")
set(need_osi_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_Osi}>>")
set(need_clp_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_Clp}>>")
set(need_cgl_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_Cgl}>>")
set(need_cbc_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_Cbc}>>")

set(need_unix_highs_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_HIGHS}>>")
set(need_windows_highs_lib "$<AND:${is_windows},$<BOOL:${BUILD_HIGHS}>>")

set(need_unix_scip_lib "$<AND:${is_not_windows},$<BOOL:${BUILD_SCIP}>>")
set(need_windows_scip_lib "$<AND:${is_windows},$<BOOL:${BUILD_SCIP}>>")

set(is_ortools_shared "$<STREQUAL:$<TARGET_PROPERTY:ortools,TYPE>,SHARED_LIBRARY>")
set(need_unix_ortools_lib "$<AND:${is_not_windows},${is_ortools_shared}>")
set(need_windows_ortools_lib "$<AND:${is_windows},${is_ortools_shared}>")

configure_file(
  ${PROJECT_SOURCE_DIR}/ortools/dotnet/${DOTNET_PACKAGE}.runtime.csproj.in
  ${DOTNET_NATIVE_PROJECT_DIR}/${DOTNET_NATIVE_PROJECT}.csproj.in
  @ONLY)
file(GENERATE
  OUTPUT ${DOTNET_NATIVE_PROJECT_DIR}/$<CONFIG>/${DOTNET_NATIVE_PROJECT}.csproj.in
  INPUT ${DOTNET_NATIVE_PROJECT_DIR}/${DOTNET_NATIVE_PROJECT}.csproj.in)

add_custom_command(
  OUTPUT ${DOTNET_NATIVE_PROJECT_DIR}/${DOTNET_NATIVE_PROJECT}.csproj
  COMMAND ${CMAKE_COMMAND} -E copy ./$<CONFIG>/${DOTNET_NATIVE_PROJECT}.csproj.in ${DOTNET_NATIVE_PROJECT}.csproj
  DEPENDS
    ${DOTNET_NATIVE_PROJECT_DIR}/$<CONFIG>/${DOTNET_NATIVE_PROJECT}.csproj.in
  WORKING_DIRECTORY ${DOTNET_NATIVE_PROJECT_DIR})

add_custom_command(
  OUTPUT ${DOTNET_NATIVE_PROJECT_DIR}/timestamp
  COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
    ${DOTNET_EXECUTABLE} build --nologo -c Release -p:Platform=${DOTNET_PLATFORM} ${DOTNET_NATIVE_PROJECT}.csproj
  COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
    ${DOTNET_EXECUTABLE} pack --nologo -c Release -p:Platform=${DOTNET_PLATFORM} ${DOTNET_NATIVE_PROJECT}.csproj
  COMMAND ${CMAKE_COMMAND} -E touch ${DOTNET_NATIVE_PROJECT_DIR}/timestamp
  DEPENDS
    ${PROJECT_BINARY_DIR}/dotnet/Directory.Build.props
    ${DOTNET_NATIVE_PROJECT_DIR}/${DOTNET_NATIVE_PROJECT}.csproj
    ${PROJECT_BINARY_DIR}/dotnet/or-tools.snk
    Dotnet${PROJECT_NAME}_proto
    google-ortools-native
  BYPRODUCTS
    ${DOTNET_NATIVE_PROJECT_DIR}/bin
    ${DOTNET_NATIVE_PROJECT_DIR}/obj
  VERBATIM
  COMMENT "Generate .Net native package ${DOTNET_NATIVE_PROJECT} (${DOTNET_NATIVE_PROJECT_DIR}/timestamp)"
  WORKING_DIRECTORY ${DOTNET_NATIVE_PROJECT_DIR})

add_custom_target(dotnet_native_package
  DEPENDS
    ${DOTNET_NATIVE_PROJECT_DIR}/timestamp
  WORKING_DIRECTORY ${DOTNET_NATIVE_PROJECT_DIR})

####################
##  .Net Package  ##
####################
if(UNIVERSAL_DOTNET_PACKAGE)
  set(DOTNET_META_PLATFORM any)
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/dotnet/${DOTNET_PROJECT}-full.csproj.in
    ${DOTNET_PROJECT_DIR}/${DOTNET_PROJECT}.csproj.in
    @ONLY)
else()
  set(DOTNET_META_PLATFORM ${DOTNET_PLATFORM})
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/dotnet/${DOTNET_PROJECT}-local.csproj.in
    ${DOTNET_PROJECT_DIR}/${DOTNET_PROJECT}.csproj.in
    @ONLY)
endif()

add_custom_command(
  OUTPUT ${DOTNET_PROJECT_DIR}/${DOTNET_PROJECT}.csproj
  COMMAND ${CMAKE_COMMAND} -E copy ${DOTNET_PROJECT}.csproj.in ${DOTNET_PROJECT}.csproj
  DEPENDS
    ${DOTNET_PROJECT_DIR}/${DOTNET_PROJECT}.csproj.in
  WORKING_DIRECTORY ${DOTNET_PROJECT_DIR})

add_custom_command(
  OUTPUT ${DOTNET_PROJECT_DIR}/timestamp
  COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
    ${DOTNET_EXECUTABLE} build --nologo -c Release -p:Platform=${DOTNET_META_PLATFORM} ${DOTNET_PROJECT}.csproj
  COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
    ${DOTNET_EXECUTABLE} pack --nologo -c Release -p:Platform=${DOTNET_META_PLATFORM} ${DOTNET_PROJECT}.csproj
  COMMAND ${CMAKE_COMMAND} -E touch ${DOTNET_PROJECT_DIR}/timestamp
  DEPENDS
    ${PROJECT_BINARY_DIR}/dotnet/or-tools.snk
    ${DOTNET_PROJECT_DIR}/${DOTNET_PROJECT}.csproj
    Dotnet${PROJECT_NAME}_proto
    ${DOTNET_NATIVE_PROJECT_DIR}/timestamp
    dotnet_native_package
  BYPRODUCTS
    ${DOTNET_PROJECT_DIR}/bin
    ${DOTNET_PROJECT_DIR}/obj
  VERBATIM
  COMMENT "Generate .Net package ${DOTNET_PROJECT} (${DOTNET_PROJECT_DIR}/timestamp)"
  WORKING_DIRECTORY ${DOTNET_PROJECT_DIR})

add_custom_target(dotnet_package ALL
  DEPENDS
    ${DOTNET_PROJECT_DIR}/timestamp
  WORKING_DIRECTORY ${DOTNET_PROJECT_DIR})

###############
## Doc rules ##
###############
if(BUILD_DOTNET_DOC)
  # add a target to generate API documentation with Doxygen
  find_package(Doxygen REQUIRED)
  if(DOXYGEN_FOUND)
    configure_file(${PROJECT_SOURCE_DIR}/ortools/dotnet/Doxyfile.in ${PROJECT_BINARY_DIR}/dotnet/Doxyfile @ONLY)
    file(DOWNLOAD
      https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/v2.3.4/doxygen-awesome.css
      ${PROJECT_BINARY_DIR}/dotnet/doxygen-awesome.css
      SHOW_PROGRESS
    )
    add_custom_target(${PROJECT_NAME}_dotnet_doc ALL
      #COMMAND ${CMAKE_COMMAND} -E rm -rf ${PROJECT_BINARY_DIR}/docs/dotnet
      COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/docs/dotnet
      COMMAND ${DOXYGEN_EXECUTABLE} -q ${PROJECT_BINARY_DIR}/dotnet/Doxyfile
      DEPENDS
        dotnet_package
        ${PROJECT_BINARY_DIR}/dotnet/Doxyfile
        ${PROJECT_BINARY_DIR}/dotnet/doxygen-awesome.css
        ${PROJECT_SOURCE_DIR}/ortools/doxygen/header.html
        ${PROJECT_SOURCE_DIR}/ortools/doxygen/DoxygenLayout.xml
        ${PROJECT_SOURCE_DIR}/ortools/dotnet/stylesheet.css
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      COMMENT "Generating .Net API documentation with Doxygen"
      VERBATIM)
  else()
    message(WARNING "cmd `doxygen` not found, .Net doc generation is disable!")
  endif()
endif()

###################
##  .Net Sample  ##
###################
# add_dotnet_sample()
# CMake function to generate and build dotnet sample.
# Parameters:
#  FILE_NAME: the .Net filename
#  COMPONENT_NAME: name of the ortools/ subdir where the test is located
#  note: automatically determined if located in ortools/<component>/samples/
# e.g.:
# add_dotnet_sample(
#   FILE_NAME
#     ${PROJECT_SOURCE_DIR}/ortools/foo/sample/Bar.cs
#   COMPONENT_NAME
#     foo
# )
function(add_dotnet_sample)
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

  set(DOTNET_SAMPLE_DIR ${PROJECT_BINARY_DIR}/dotnet/${COMPONENT_NAME}/${SAMPLE_NAME})
  message(STATUS "build path: ${DOTNET_SAMPLE_DIR}")

  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/dotnet/Sample.csproj.in
    ${DOTNET_SAMPLE_DIR}/${SAMPLE_NAME}.csproj
    @ONLY)

  add_custom_command(
    OUTPUT ${DOTNET_SAMPLE_DIR}/${SAMPLE_NAME}.cs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DOTNET_SAMPLE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${SAMPLE_FILE_NAME} ${DOTNET_SAMPLE_DIR}/
      MAIN_DEPENDENCY ${SAMPLE_FILE_NAME}
    VERBATIM
    WORKING_DIRECTORY ${DOTNET_SAMPLE_DIR})

  add_custom_command(
    OUTPUT ${DOTNET_SAMPLE_DIR}/timestamp
    COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
      ${DOTNET_EXECUTABLE} build --nologo -c Release
    COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
      ${DOTNET_EXECUTABLE} pack --nologo -c Release
    COMMAND ${CMAKE_COMMAND} -E touch ${DOTNET_SAMPLE_DIR}/timestamp
    DEPENDS
      ${DOTNET_SAMPLE_DIR}/${SAMPLE_NAME}.csproj
      ${DOTNET_SAMPLE_DIR}/${SAMPLE_NAME}.cs
      dotnet_package
    BYPRODUCTS
      ${DOTNET_SAMPLE_DIR}/bin
      ${DOTNET_SAMPLE_DIR}/obj
    COMMENT "Compiling .Net ${COMPONENT_NAME}/${SAMPLE_NAME}.cs (${DOTNET_SAMPLE_DIR}/timestamp)"
    WORKING_DIRECTORY ${DOTNET_SAMPLE_DIR})

  add_custom_target(dotnet_${COMPONENT_NAME}_${SAMPLE_NAME} ALL
    DEPENDS
      ${DOTNET_SAMPLE_DIR}/timestamp
    WORKING_DIRECTORY ${DOTNET_SAMPLE_DIR})

  if(BUILD_TESTING)
    if(USE_DOTNET_CORE_31)
      add_test(
        NAME dotnet_${COMPONENT_NAME}_${SAMPLE_NAME}_netcoreapp31
        COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
          ${DOTNET_EXECUTABLE} run --framework netcoreapp3.1 -c Release
        WORKING_DIRECTORY ${DOTNET_SAMPLE_DIR})
    endif()
    if(USE_DOTNET_6)
      add_test(
        NAME dotnet_${COMPONENT_NAME}_${SAMPLE_NAME}_net60
        COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
          ${DOTNET_EXECUTABLE} run --no-build --framework net6.0 -c Release
        WORKING_DIRECTORY ${DOTNET_SAMPLE_DIR})
    endif()
    if(USE_DOTNET_7)
      add_test(
        NAME dotnet_${COMPONENT_NAME}_${SAMPLE_NAME}_net70
        COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
          ${DOTNET_EXECUTABLE} run --no-build --framework net7.0 -c Release
        WORKING_DIRECTORY ${DOTNET_SAMPLE_DIR})
    endif()
    if(USE_DOTNET_8)
      add_test(
        NAME dotnet_${COMPONENT_NAME}_${SAMPLE_NAME}_net80
        COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
          ${DOTNET_EXECUTABLE} run --no-build --framework net8.0 -c Release
        WORKING_DIRECTORY ${DOTNET_SAMPLE_DIR})
    endif()
    if(USE_DOTNET_9)
      add_test(
        NAME dotnet_${COMPONENT_NAME}_${SAMPLE_NAME}_net90
        COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
          ${DOTNET_EXECUTABLE} run --no-build --framework net9.0 -c Release
        WORKING_DIRECTORY ${DOTNET_SAMPLE_DIR})
    endif()
  endif()
  message(STATUS "Configuring sample ${SAMPLE_FILE_NAME} ...DONE")
endfunction()

####################
##  .Net Example  ##
####################
# add_dotnet_example()
# CMake function to generate and build dotnet example.
# Parameters:
#  FILE_NAME: the .Net filename
#  COMPONENT_NAME: name of the example/ subdir where the test is located
#  note: automatically determined if located in examples/<component>/
# e.g.:
# add_dotnet_example(
#   FILE_NAME
#     ${PROJECT_SOURCE_DIR}/examples/foo/Bar.cs
#   COMPONENT_NAME
#     foo
# )
function(add_dotnet_example)
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
    # sample is located in examples/<component_name>/
    get_filename_component(COMPONENT_DIR ${EXAMPLE_FILE_NAME} DIRECTORY)
    get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)
  else()
    set(COMPONENT_NAME ${EXAMPLE_COMPONENT_NAME})
  endif()

  set(DOTNET_EXAMPLE_DIR ${PROJECT_BINARY_DIR}/dotnet/${COMPONENT_NAME}/${EXAMPLE_NAME})
  message(STATUS "build path: ${DOTNET_EXAMPLE_DIR}")

  set(SAMPLE_NAME ${EXAMPLE_NAME})
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/dotnet/Example.csproj.in
    ${DOTNET_EXAMPLE_DIR}/${EXAMPLE_NAME}.csproj
    @ONLY)

  add_custom_command(
    OUTPUT ${DOTNET_EXAMPLE_DIR}/${EXAMPLE_NAME}.cs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DOTNET_EXAMPLE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy
      ${EXAMPLE_FILE_NAME}
      ${DOTNET_EXAMPLE_DIR}/
    MAIN_DEPENDENCY ${EXAMPLE_FILE_NAME}
    VERBATIM
    WORKING_DIRECTORY ${DOTNET_EXAMPLE_DIR})

  add_custom_command(
    OUTPUT ${DOTNET_EXAMPLE_DIR}/timestamp
    COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
      ${DOTNET_EXECUTABLE} build --nologo -c Release ${EXAMPLE_NAME}.csproj
    COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
      ${DOTNET_EXECUTABLE} pack --nologo -c Release ${EXAMPLE_NAME}.csproj
    COMMAND ${CMAKE_COMMAND} -E touch ${DOTNET_EXAMPLE_DIR}/timestamp
    DEPENDS
      ${DOTNET_EXAMPLE_DIR}/${EXAMPLE_NAME}.csproj
      ${DOTNET_EXAMPLE_DIR}/${EXAMPLE_NAME}.cs
      dotnet_package
    BYPRODUCTS
      ${DOTNET_EXAMPLE_DIR}/bin
      ${DOTNET_EXAMPLE_DIR}/obj
    VERBATIM
    COMMENT "Compiling .Net ${COMPONENT_NAME}/${EXAMPLE_NAME}.cs (${DOTNET_EXAMPLE_DIR}/timestamp)"
    WORKING_DIRECTORY ${DOTNET_EXAMPLE_DIR})

  add_custom_target(dotnet_${COMPONENT_NAME}_${EXAMPLE_NAME} ALL
    DEPENDS
      ${DOTNET_EXAMPLE_DIR}/timestamp
    WORKING_DIRECTORY ${DOTNET_EXAMPLE_DIR})

  if(BUILD_TESTING)
    if(USE_DOTNET_CORE_31)
      add_test(
        NAME dotnet_${COMPONENT_NAME}_${EXAMPLE_NAME}_netcoreapp31
        COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
          ${DOTNET_EXECUTABLE} run --no-build --framework netcoreapp3.1 -c Release ${EXAMPLE_NAME}.csproj
        WORKING_DIRECTORY ${DOTNET_EXAMPLE_DIR})
    endif()
    if(USE_DOTNET_6)
      add_test(
        NAME dotnet_${COMPONENT_NAME}_${EXAMPLE_NAME}_net60
        COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
          ${DOTNET_EXECUTABLE} run --no-build --framework net6.0 -c Release ${EXAMPLE_NAME}.csproj
        WORKING_DIRECTORY ${DOTNET_EXAMPLE_DIR})
    endif()
    if(USE_DOTNET_7)
      add_test(
        NAME dotnet_${COMPONENT_NAME}_${EXAMPLE_NAME}_net70
        COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
          ${DOTNET_EXECUTABLE} run --no-build --framework net7.0 -c Release ${EXAMPLE_NAME}.csproj
        WORKING_DIRECTORY ${DOTNET_EXAMPLE_DIR})
    endif()
    if(USE_DOTNET_8)
      add_test(
        NAME dotnet_${COMPONENT_NAME}_${EXAMPLE_NAME}_net80
        COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
          ${DOTNET_EXECUTABLE} run --no-build --framework net8.0 -c Release ${EXAMPLE_NAME}.csproj
        WORKING_DIRECTORY ${DOTNET_EXAMPLE_DIR})
    endif()
    if(USE_DOTNET_9)
      add_test(
        NAME dotnet_${COMPONENT_NAME}_${EXAMPLE_NAME}_net90
        COMMAND ${CMAKE_COMMAND} -E env --unset=TARGETNAME
          ${DOTNET_EXECUTABLE} run --no-build --framework net9.0 -c Release ${EXAMPLE_NAME}.csproj
        WORKING_DIRECTORY ${DOTNET_EXAMPLE_DIR})
    endif()
  endif()
  message(STATUS "Configuring example ${EXAMPLE_FILE_NAME} ...DONE")
endfunction()
