if(NOT BUILD_DOTNET)
  return()
endif()

if(NOT TARGET ortools::ortools)
  message(FATAL_ERROR ".Net: missing ortools TARGET")
endif()

find_package(SWIG)
include(UseSWIG)

#if(${SWIG_VERSION} VERSION_GREATER_EQUAL 4)
#  list(APPEND CMAKE_SWIG_FLAGS "-doxygen")
#endif()

if(UNIX AND NOT APPLE)
  list(APPEND CMAKE_SWIG_FLAGS "-DSWIGWORDSIZE64")
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
foreach(PROTO_FILE ${proto_dotnet_files})
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
    "--csharp_out=${PROJECT_BINARY_DIR}/dotnet/${PROTO_DIR}"
    "--csharp_opt=file_extension=.pb.cs"
    ${PROTO_FILE}
    DEPENDS ${PROTO_FILE} protobuf::protoc
    COMMENT "Running C++ protocol buffer compiler on ${PROTO_FILE}"
    VERBATIM)
  list(APPEND PROTO_DOTNETS ${PROTO_DOTNET})
endforeach()
add_custom_target(Dotnet${PROJECT_NAME}_proto DEPENDS ${PROTO_DOTNETS} ortools::ortools)

# Setup Dotnet
find_program (DOTNET_CLI NAMES dotnet)

# CMake will remove all '-D' prefix (i.e. -DUSE_FOO become USE_FOO)
#get_target_property(FLAGS ortools::ortools COMPILE_DEFINITIONS)
set(FLAGS -DUSE_BOP -DUSE_GLOP -DABSL_MUST_USE_RESULT)
if(USE_COINOR)
  list(APPEND FLAGS
    "-DUSE_CBC"
    "-DUSE_CLP"
    )
endif()
list(APPEND CMAKE_SWIG_FLAGS ${FLAGS} "-I${PROJECT_SOURCE_DIR}")

foreach(SUBPROJECT IN ITEMS algorithms graph linear_solver constraint_solver sat util)
  add_subdirectory(ortools/${SUBPROJECT}/csharp)
  list(APPEND dotnet_native_targets ${PROJECT_NAME}::dotnet_${SUBPROJECT})
endforeach()

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
    COMMAND
    ${DOTNET_CLI} run
    --project ${OR_TOOLS_DOTNET_SNK}/${OR_TOOLS_DOTNET_SNK}.csproj
    /or-tools.snk
    COMMENT "Generate or-tools.snk using CreateSigningKey project"
    WORKING_DIRECTORY dotnet
    VERBATIM
    )
endif()

if(APPLE)
  set(RUNTIME_IDENTIFIER osx-x64)
elseif(UNIX)
  set(RUNTIME_IDENTIFIER linux-x64)
elseif(WIN32)
  set(RUNTIME_IDENTIFIER win-x64)
else()
  message(FATAL_ERROR "Unsupported system !")
endif()
set(OR_TOOLS_DOTNET_NATIVE Google.OrTools.runtime.${RUNTIME_IDENTIFIER})

add_custom_target(dotnet_native
  DEPENDS
    dotnet/or-tools.snk
    ortools::ortools
    Dotnet${PROJECT_NAME}_proto
    ${PROJECT_BINARY_DIR}/dotnet/${OR_TOOLS_DOTNET_NATIVE}/${OR_TOOLS_DOTNET_NATIVE}.csproj
  COMMAND ${CMAKE_COMMAND} -E make_directory packages
  COMMAND ${DOTNET_CLI} build -c Release /p:Platform=x64 ${OR_TOOLS_DOTNET_NATIVE}/${OR_TOOLS_DOTNET_NATIVE}.csproj
  COMMAND ${DOTNET_CLI} pack -c Release ${OR_TOOLS_DOTNET_NATIVE}/${OR_TOOLS_DOTNET_NATIVE}.csproj
  WORKING_DIRECTORY dotnet
  )

configure_file(
  ortools/dotnet/${OR_TOOLS_DOTNET_NATIVE}/${OR_TOOLS_DOTNET_NATIVE}.csproj.in
  dotnet/${OR_TOOLS_DOTNET_NATIVE}/${OR_TOOLS_DOTNET_NATIVE}.csproj
  @ONLY)


# Main Target
set(OR_TOOLS_DOTNET Google.OrTools)

add_custom_target(dotnet_package ALL
  DEPENDS
    dotnet/or-tools.snk
    dotnet_native
    ${PROJECT_BINARY_DIR}/dotnet/${OR_TOOLS_DOTNET}/${OR_TOOLS_DOTNET}.csproj
  COMMAND ${DOTNET_CLI} build -c Release /p:Platform=x64 ${OR_TOOLS_DOTNET}/${OR_TOOLS_DOTNET}.csproj
  COMMAND ${DOTNET_CLI} pack -c Release ${OR_TOOLS_DOTNET}/${OR_TOOLS_DOTNET}.csproj
  BYPRODUCTS
    dotnet/packages
  WORKING_DIRECTORY dotnet
  )

configure_file(
  ortools/dotnet/${OR_TOOLS_DOTNET}/${OR_TOOLS_DOTNET}.csproj.in
  dotnet/${OR_TOOLS_DOTNET}/${OR_TOOLS_DOTNET}.csproj
  @ONLY)

# Test
if(BUILD_TESTING)
  #add_subdirectory(examples/dotnet)
endif()
