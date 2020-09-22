if(NOT BUILD_DOTNET)
  return()
endif()

if(NOT TARGET ortools::ortools)
  message(FATAL_ERROR ".Net: missing ortools TARGET")
endif()

# Will need swig
set(CMAKE_SWIG_FLAGS)
find_package(SWIG)
include(UseSWIG)

#if(${SWIG_VERSION} VERSION_GREATER_EQUAL 4)
#  list(APPEND CMAKE_SWIG_FLAGS "-doxygen")
#endif()

if(UNIX AND NOT APPLE)
  list(APPEND CMAKE_SWIG_FLAGS "-DSWIGWORDSIZE64")
endif()

# Setup Dotnet
find_program (DOTNET_EXECUTABLE NAMES dotnet)
if(NOT DOTNET_EXECUTABLE)
  message(FATAL_ERROR "Check for dotnet Program: not found")
else()
  message(STATUS "Found dotnet Program: ${DOTNET_EXECUTABLE}")
endif()

# Generate Protobuf .Net sources
set(PROTO_DOTNETS)
file(GLOB_RECURSE proto_dotnet_files RELATIVE ${PROJECT_SOURCE_DIR}
  "ortools/constraint_solver/*.proto"
  "ortools/linear_solver/*.proto"
  "ortools/sat/*.proto"
  "ortools/util/*.proto"
  )
list(REMOVE_ITEM proto_dotnet_files "ortools/constraint_solver/demon_profiler.proto")
list(REMOVE_ITEM proto_dotnet_files "ortools/constraint_solver/assignment.proto")
foreach(PROTO_FILE IN LISTS proto_dotnet_files)
  #message(STATUS "protoc proto(dotnet): ${PROTO_FILE}")
  get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(PROTO_DOTNET ${PROJECT_BINARY_DIR}/dotnet/${PROTO_DIR}/${PROTO_NAME}.pb.cs)
  #message(STATUS "protoc dotnet: ${PROTO_DOTNET}")
  add_custom_command(
    OUTPUT ${PROTO_DOTNET}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/dotnet/${PROTO_DIR}
    COMMAND protobuf::protoc
    "--proto_path=${PROJECT_SOURCE_DIR}"
    ${PROTO_DIRS}
    "--csharp_out=${PROJECT_BINARY_DIR}/dotnet/${PROTO_DIR}"
    "--csharp_opt=file_extension=.pb.cs"
    ${PROTO_FILE}
    DEPENDS ${PROTO_FILE} protobuf::protoc
    COMMENT "Generate C# protocol buffer for ${PROTO_FILE}"
    VERBATIM)
  list(APPEND PROTO_DOTNETS ${PROTO_DOTNET})
endforeach()
add_custom_target(Dotnet${PROJECT_NAME}_proto DEPENDS ${PROTO_DOTNETS} ortools::ortools)

# Create the native library
add_library(google-ortools-native SHARED "")
set_target_properties(google-ortools-native PROPERTIES
  PREFIX ""
  POSITION_INDEPENDENT_CODE ON)
# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(google-ortools-native PROPERTIES INSTALL_RPATH "@loader_path")
elseif(UNIX)
  set_target_properties(google-ortools-native PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

# CMake will remove all '-D' prefix (i.e. -DUSE_FOO become USE_FOO)
#get_target_property(FLAGS ortools::ortools COMPILE_DEFINITIONS)
set(FLAGS -DUSE_BOP -DUSE_GLOP -DABSL_MUST_USE_RESULT)
if(USE_SCIP)
  list(APPEND FLAGS "-DUSE_SCIP")
endif()
if(USE_COINOR)
  list(APPEND FLAGS "-DUSE_CBC" "-DUSE_CLP")
endif()
list(APPEND CMAKE_SWIG_FLAGS ${FLAGS} "-I${PROJECT_SOURCE_DIR}")

# Swig wrap all libraries
set(DOTNET_PROJECT Google.OrTools)
if(APPLE)
  set(RUNTIME_IDENTIFIER osx-x64)
elseif(UNIX)
  set(RUNTIME_IDENTIFIER linux-x64)
elseif(WIN32)
  set(RUNTIME_IDENTIFIER win-x64)
else()
  message(FATAL_ERROR "Unsupported system !")
endif()
set(DOTNET_NATIVE_PROJECT ${DOTNET_PROJECT}.runtime.${RUNTIME_IDENTIFIER})

foreach(SUBPROJECT IN ITEMS algorithms graph linear_solver constraint_solver sat util)
  add_subdirectory(ortools/${SUBPROJECT}/csharp)
  target_link_libraries(google-ortools-native PRIVATE dotnet_${SUBPROJECT})
endforeach()

############################
##  .Net Runtime Package  ##
############################
file(COPY tools/doc/orLogo.png DESTINATION dotnet)
set(DOTNET_PACKAGES_DIR "${PROJECT_BINARY_DIR}/dotnet/packages")
set(DOTNET_LOGO_DIR "${PROJECT_BINARY_DIR}/dotnet")
configure_file(ortools/dotnet/Directory.Build.props.in dotnet/Directory.Build.props)

# Build or retrieve .snk file
if(DEFINED ENV{DOTNET_SNK})
  add_custom_command(OUTPUT dotnet/or-tools.snk
    COMMAND ${CMAKE_COMMAND} -E copy $ENV{DOTNET_SNK} .
    COMMENT "Copy or-tools.snk from ENV:DOTNET_SNK"
    WORKING_DIRECTORY dotnet
    VERBATIM
    )
else()
  set(OR_TOOLS_DOTNET_SNK CreateSigningKey)
  add_custom_command(OUTPUT dotnet/or-tools.snk
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/ortools/dotnet/${OR_TOOLS_DOTNET_SNK} ${OR_TOOLS_DOTNET_SNK}
    COMMAND ${DOTNET_EXECUTABLE} run
    --project ${OR_TOOLS_DOTNET_SNK}/${OR_TOOLS_DOTNET_SNK}.csproj
    /or-tools.snk
    COMMENT "Generate or-tools.snk using CreateSigningKey project"
    WORKING_DIRECTORY dotnet
    VERBATIM
    )
endif()

file(GENERATE OUTPUT dotnet/$<CONFIG>/replace_runtime.cmake
  CONTENT
  "FILE(READ ${PROJECT_SOURCE_DIR}/ortools/dotnet/${DOTNET_NATIVE_PROJECT}/${DOTNET_NATIVE_PROJECT}.csproj.in input)
STRING(REPLACE \"@PROJECT_VERSION@\" \"${PROJECT_VERSION}\" input \"\${input}\")
STRING(REPLACE \"@ortools@\" \"$<$<NOT:$<PLATFORM_ID:Windows>>:$<TARGET_SONAME_FILE:${PROJECT_NAME}>>\" input \"\${input}\")
STRING(REPLACE \"@native@\" \"$<TARGET_FILE:google-ortools-native>\" input \"\${input}\")
FILE(WRITE ${DOTNET_NATIVE_PROJECT}/${DOTNET_NATIVE_PROJECT}.csproj \"\${input}\")"
)

add_custom_command(
  OUTPUT dotnet/${DOTNET_NATIVE_PROJECT}/${DOTNET_NATIVE_PROJECT}.csproj
  COMMAND ${CMAKE_COMMAND} -E make_directory ${DOTNET_NATIVE_PROJECT}
  COMMAND ${CMAKE_COMMAND} -P ./$<CONFIG>/replace_runtime.cmake
  WORKING_DIRECTORY dotnet
  )

if(WIN32)
add_custom_command(
  OUTPUT dotnet/${DOTNET_NATIVE_PROJECT}/${DOTNET_NATIVE_PROJECT}.targets
  COMMAND ${CMAKE_COMMAND} -E make_directory ${DOTNET_NATIVE_PROJECT}
  COMMAND ${CMAKE_COMMAND} -E copy
    ${PROJECT_SOURCE_DIR}/ortools/dotnet/${DOTNET_NATIVE_PROJECT}/${DOTNET_NATIVE_PROJECT}.targets
    ${DOTNET_NATIVE_PROJECT}/${DOTNET_NATIVE_PROJECT}.targets
  WORKING_DIRECTORY dotnet
  )
  set(DOTNET_TARGETS dotnet/${DOTNET_NATIVE_PROJECT}/${DOTNET_NATIVE_PROJECT}.targets)
endif()

add_custom_target(dotnet_native ALL
  DEPENDS
    dotnet/or-tools.snk
    Dotnet${PROJECT_NAME}_proto
    google-ortools-native
    dotnet/${DOTNET_NATIVE_PROJECT}/${DOTNET_NATIVE_PROJECT}.csproj
    ${DOTNET_TARGETS}
  COMMAND ${CMAKE_COMMAND} -E make_directory packages
  COMMAND ${DOTNET_EXECUTABLE} build -c Release /p:Platform=x64 ${DOTNET_NATIVE_PROJECT}/${DOTNET_NATIVE_PROJECT}.csproj
  COMMAND ${DOTNET_EXECUTABLE} pack -c Release ${DOTNET_NATIVE_PROJECT}/${DOTNET_NATIVE_PROJECT}.csproj
  BYPRODUCTS
    dotnet/${DOTNET_NATIVE_PROJECT}/bin
    dotnet/${DOTNET_NATIVE_PROJECT}/obj
  WORKING_DIRECTORY dotnet
  )

####################
##  .Net Package  ##
####################
file(GENERATE OUTPUT dotnet/$<CONFIG>/replace.cmake
  CONTENT
  "FILE(READ ${PROJECT_SOURCE_DIR}/ortools/dotnet/${DOTNET_PROJECT}/${DOTNET_PROJECT}.csproj.in input)
STRING(REPLACE \"@PROJECT_VERSION@\" \"${PROJECT_VERSION}\" input \"\${input}\")
STRING(REPLACE \"@PROJECT_SOURCE_DIR@\" \"${PROJECT_SOURCE_DIR}\" input \"\${input}\")
STRING(REPLACE \"@PROJECT_DOTNET_DIR@\" \"${PROJECT_BINARY_DIR}/dotnet\" input \"\${input}\")
STRING(REPLACE \"@DOTNET_PACKAGES_DIR@\" \"${PROJECT_BINARY_DIR}/dotnet/packages\" input \"\${input}\")
FILE(WRITE ${DOTNET_PROJECT}/${DOTNET_PROJECT}.csproj \"\${input}\")"
)

add_custom_command(
  OUTPUT dotnet/${DOTNET_PROJECT}/${DOTNET_PROJECT}.csproj
  COMMAND ${CMAKE_COMMAND} -E make_directory ${DOTNET_PROJECT}
  COMMAND ${CMAKE_COMMAND} -P ./$<CONFIG>/replace.cmake
  WORKING_DIRECTORY dotnet
  )

add_custom_target(dotnet_package ALL
  DEPENDS
    dotnet/or-tools.snk
    dotnet_native
    dotnet/${DOTNET_PROJECT}/${DOTNET_PROJECT}.csproj
  COMMAND ${DOTNET_EXECUTABLE} build -c Release /p:Platform=x64 ${DOTNET_PROJECT}/${DOTNET_PROJECT}.csproj
  COMMAND ${DOTNET_EXECUTABLE} pack -c Release ${DOTNET_PROJECT}/${DOTNET_PROJECT}.csproj
  BYPRODUCTS
    dotnet/${DOTNET_PROJECT}/bin
    dotnet/${DOTNET_PROJECT}/obj
    dotnet/packages
  WORKING_DIRECTORY dotnet
  )

# add_dotnet_sample()
# CMake function to generate and build dotnet sample.
# Parameters:
#  the dotnet filename
# e.g.:
# add_dotnet_sample(Foo.cs)
function(add_dotnet_sample FILE_NAME)
  message(STATUS "Building ${FILE_NAME}: ...")
  get_filename_component(SAMPLE_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(SAMPLE_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_DIR ${SAMPLE_DIR} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  set(SAMPLE_PATH ${PROJECT_BINARY_DIR}/dotnet/${COMPONENT_NAME}/${SAMPLE_NAME})
  file(MAKE_DIRECTORY ${SAMPLE_PATH})

  file(COPY ${FILE_NAME} DESTINATION ${SAMPLE_PATH})

  set(DOTNET_PACKAGES_DIR "${PROJECT_BINARY_DIR}/dotnet/packages")
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/dotnet/Sample.csproj.in
    ${SAMPLE_PATH}/${SAMPLE_NAME}.csproj
    @ONLY)

  add_custom_target(dotnet_sample_${SAMPLE_NAME} ALL
    DEPENDS ${SAMPLE_PATH}/${SAMPLE_NAME}.csproj
    COMMAND ${DOTNET_EXECUTABLE} build -c Release
    COMMAND ${DOTNET_EXECUTABLE} pack -c Release
    BYPRODUCTS
      ${SAMPLE_PATH}/bin
      ${SAMPLE_PATH}/obj
    WORKING_DIRECTORY ${SAMPLE_PATH})
  add_dependencies(dotnet_sample_${SAMPLE_NAME} dotnet_package)

  if(BUILD_TESTING)
    add_test(
      NAME dotnet_${COMPONENT_NAME}_${SAMPLE_NAME}
      COMMAND ${DOTNET_EXECUTABLE} run --no-build -c Release
      WORKING_DIRECTORY ${SAMPLE_PATH})
  endif()

  message(STATUS "Building ${FILE_NAME}: ...DONE")
endfunction()

# add_dotnet_example()
# CMake function to generate and build dotnet example.
# Parameters:
#  the dotnet filename
# e.g.:
# add_dotnet_example(Foo.cs)
function(add_dotnet_example FILE_NAME)
  message(STATUS "Building ${FILE_NAME}: ...")
  get_filename_component(EXAMPLE_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(COMPONENT_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  set(EXAMPLE_PATH ${PROJECT_BINARY_DIR}/dotnet/${COMPONENT_NAME}/${EXAMPLE_NAME})
  file(MAKE_DIRECTORY ${EXAMPLE_PATH})

  file(COPY ${FILE_NAME} DESTINATION ${EXAMPLE_PATH})

  set(DOTNET_PACKAGES_DIR "${PROJECT_BINARY_DIR}/dotnet/packages")
  set(SAMPLE_NAME ${EXAMPLE_NAME})
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/dotnet/Sample.csproj.in
    ${EXAMPLE_PATH}/${EXAMPLE_NAME}.csproj
    @ONLY)

  add_custom_target(dotnet_example_${EXAMPLE_NAME} ALL
    DEPENDS ${EXAMPLE_PATH}/${EXAMPLE_NAME}.csproj
    COMMAND ${DOTNET_EXECUTABLE} build -c Release
    COMMAND ${DOTNET_EXECUTABLE} pack -c Release
    BYPRODUCTS
      ${EXAMPLE_PATH}/bin
      ${EXAMPLE_PATH}/obj
    WORKING_DIRECTORY ${EXAMPLE_PATH})
  add_dependencies(dotnet_example_${EXAMPLE_NAME} dotnet_package)

  if(BUILD_TESTING)
    add_test(
      NAME dotnet_${COMPONENT_NAME}_${EXAMPLE_NAME}
      COMMAND ${DOTNET_EXECUTABLE} run --no-build -c Release
      WORKING_DIRECTORY ${EXAMPLE_PATH})
  endif()

  message(STATUS "Building ${FILE_NAME}: ...DONE")
endfunction()
