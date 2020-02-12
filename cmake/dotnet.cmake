if(NOT BUILD_DOTNET)
  return()
endif()

if(NOT TARGET ortools::ortools)
  message(FATAL_ERROR "Java: missing ortools TARGET")
endif()

find_package(SWIG)
include(UseSWIG)

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
  set(PROTO_DOTNET ${PROJECT_BINARY_DIR}/${PROTO_DIR}/${PROTO_NAME}.pb.cs)
  #message(STATUS "protoc dotnet: ${PROTO_DOTNET}")
  add_custom_command(
    OUTPUT ${PROTO_DOTNET}
    COMMAND protobuf::protoc
    "--proto_path=${PROJECT_SOURCE_DIR}"
    "--csharp_out=${PROJECT_BINARY_DIR}"
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

foreach(SUBPROJECT constraint_solver linear_solver sat graph algorithms data)
  #add_subdirectory(ortools/${SUBPROJECT}/csharp)
endforeach()

# Main Target
add_custom_target(dotnet_package ALL
  DEPENDS ${PROJECT_NAME}.csproj
  COMMAND ${CMAKE_COMMAND} -E remove_directory packages
  COMMAND ${DOTNET_CLI} package ${PROJECT_NAME}.csproj
  )

# Test
if(BUILD_TESTING)
  add_subdirectory(examples/dotnet)
endif()
