if(NOT BUILD_DOTNET)
  return()
endif()

if(NOT TARGET ${PROJECT_NAMESPACE}::ortools)
  message(FATAL_ERROR ".Net: missing ortools TARGET")
endif()

# Will need swig
set(CMAKE_SWIG_FLAGS)
find_package(SWIG REQUIRED)
include(UseSWIG)

#if(${SWIG_VERSION} VERSION_GREATER_EQUAL 4)
#  list(APPEND CMAKE_SWIG_FLAGS "-doxygen")
#endif()

if(UNIX AND NOT APPLE)
  list(APPEND CMAKE_SWIG_FLAGS "-DSWIGWORDSIZE64")
endif()

# Find dotnet cli
find_program(DOTNET_EXECUTABLE NAMES dotnet)
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
    COMMAND ${PROTOC_PRG}
    "--proto_path=${PROJECT_SOURCE_DIR}"
    ${PROTO_DIRS}
    "--csharp_out=${PROJECT_BINARY_DIR}/dotnet/${PROTO_DIR}"
    "--csharp_opt=file_extension=.pb.cs"
    ${PROTO_FILE}
    DEPENDS ${PROTO_FILE} ${PROTOC_PRG}
    COMMENT "Generate C# protocol buffer for ${PROTO_FILE}"
    VERBATIM)
  list(APPEND PROTO_DOTNETS ${PROTO_DOTNET})
endforeach()
add_custom_target(Dotnet${PROJECT_NAME}_proto DEPENDS ${PROTO_DOTNETS} ${PROJECT_NAMESPACE}::ortools)

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

# CMake will remove all '-D' prefix (i.e. -DUSE_FOO become USE_FOO)
#get_target_property(FLAGS ${PROJECT_NAMESPACE}::ortools COMPILE_DEFINITIONS)
set(FLAGS -DUSE_BOP -DUSE_GLOP -DABSL_MUST_USE_RESULT)
if(USE_COINOR)
  list(APPEND FLAGS "-DUSE_CBC" "-DUSE_CLP")
endif()
if(USE_GLPK)
  list(APPEND FLAGS "-DUSE_GLPK")
endif()
if(USE_SCIP)
  list(APPEND FLAGS "-DUSE_SCIP")
endif()
list(APPEND CMAKE_SWIG_FLAGS ${FLAGS} "-I${PROJECT_SOURCE_DIR}")

# Needed by dotnet/CMakeLists.txt
set(DOTNET_PACKAGE Google.OrTools)
set(DOTNET_PACKAGES_DIR "${PROJECT_BINARY_DIR}/dotnet/packages")
if(APPLE)
  set(RUNTIME_IDENTIFIER osx-x64)
elseif(UNIX)
  set(RUNTIME_IDENTIFIER linux-x64)
elseif(WIN32)
  set(RUNTIME_IDENTIFIER win-x64)
else()
  message(FATAL_ERROR "Unsupported system !")
endif()
set(DOTNET_NATIVE_PROJECT ${DOTNET_PACKAGE}.runtime.${RUNTIME_IDENTIFIER})
set(DOTNET_PROJECT ${DOTNET_PACKAGE})

# Swig wrap all libraries
foreach(SUBPROJECT IN ITEMS algorithms graph init linear_solver constraint_solver sat util)
  add_subdirectory(ortools/${SUBPROJECT}/csharp)
  target_link_libraries(google-ortools-native PRIVATE dotnet_${SUBPROJECT})
endforeach()

file(COPY tools/doc/orLogo.png DESTINATION dotnet)
set(DOTNET_LOGO_DIR "${PROJECT_BINARY_DIR}/dotnet")
configure_file(ortools/dotnet/Directory.Build.props.in dotnet/Directory.Build.props)

############################
##  .Net SNK file  ##
############################
# Build or retrieve .snk file
if(DEFINED ENV{DOTNET_SNK})
  add_custom_command(
    OUTPUT ${PROJECT_BINARY_DIR}/dotnet/or-tools.snk
    COMMAND ${CMAKE_COMMAND} -E copy $ENV{DOTNET_SNK} .
    COMMENT "Copy or-tools.snk from ENV:DOTNET_SNK"
    WORKING_DIRECTORY dotnet
    VERBATIM
    )
else()
  set(DOTNET_SNK_PROJECT CreateSigningKey)
  set(DOTNET_SNK_PROJECT_PATH ${PROJECT_BINARY_DIR}/dotnet/${DOTNET_SNK_PROJECT})
  add_custom_command(
    OUTPUT ${PROJECT_BINARY_DIR}/dotnet/or-tools.snk
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${PROJECT_SOURCE_DIR}/ortools/dotnet/${DOTNET_SNK_PROJECT}
      ${DOTNET_SNK_PROJECT}
    COMMAND ${DOTNET_EXECUTABLE} run
    --project ${DOTNET_SNK_PROJECT}/${DOTNET_SNK_PROJECT}.csproj
    /or-tools.snk
    BYPRODUCTS
      ${DOTNET_SNK_PROJECT_PATH}/bin
      ${DOTNET_SNK_PROJECT_PATH}/obj
    COMMENT "Generate or-tools.snk using CreateSigningKey project"
    WORKING_DIRECTORY dotnet
    VERBATIM
    )
endif()

############################
##  .Net Runtime Package  ##
############################
message(STATUS ".Net runtime project: ${DOTNET_NATIVE_PROJECT}")
set(DOTNET_NATIVE_PROJECT_PATH ${PROJECT_BINARY_DIR}/dotnet/${DOTNET_NATIVE_PROJECT})
message(STATUS ".Net runtime project build path: ${DOTNET_NATIVE_PROJECT_PATH}")

file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/dotnet/packages)

# *.csproj.in contains:
# CMake variable(s) (@PROJECT_NAME@) that configure_file() can manage and
# generator expression ($<TARGET_FILE:...>) that file(GENERATE) can manage.
configure_file(
  ${PROJECT_SOURCE_DIR}/ortools/dotnet/${DOTNET_PACKAGE}.runtime.csproj.in
  ${DOTNET_NATIVE_PROJECT_PATH}/${DOTNET_NATIVE_PROJECT}.csproj.in
  @ONLY)
file(GENERATE
  OUTPUT ${DOTNET_NATIVE_PROJECT_PATH}/$<CONFIG>/${DOTNET_NATIVE_PROJECT}.csproj.in
  INPUT ${DOTNET_NATIVE_PROJECT_PATH}/${DOTNET_NATIVE_PROJECT}.csproj.in)

add_custom_command(
  OUTPUT ${DOTNET_NATIVE_PROJECT_PATH}/${DOTNET_NATIVE_PROJECT}.csproj
  DEPENDS ${DOTNET_NATIVE_PROJECT_PATH}/$<CONFIG>/${DOTNET_NATIVE_PROJECT}.csproj.in
  COMMAND ${CMAKE_COMMAND} -E copy ./$<CONFIG>/${DOTNET_NATIVE_PROJECT}.csproj.in ${DOTNET_NATIVE_PROJECT}.csproj
  WORKING_DIRECTORY ${DOTNET_NATIVE_PROJECT_PATH}
)

add_custom_command(
  OUTPUT ${DOTNET_NATIVE_PROJECT_PATH}/timestamp
  COMMAND ${DOTNET_EXECUTABLE} build -c Release /p:Platform=x64 ${DOTNET_NATIVE_PROJECT}.csproj
  COMMAND ${DOTNET_EXECUTABLE} pack -c Release ${DOTNET_NATIVE_PROJECT}.csproj
  COMMAND ${CMAKE_COMMAND} -E touch ${DOTNET_NATIVE_PROJECT_PATH}/timestamp
  DEPENDS
    ${DOTNET_NATIVE_PROJECT_PATH}/${DOTNET_NATIVE_PROJECT}.csproj
    ${PROJECT_BINARY_DIR}/dotnet/or-tools.snk
    Dotnet${PROJECT_NAME}_proto
    google-ortools-native
  BYPRODUCTS
    ${DOTNET_NATIVE_PROJECT_PATH}/bin
    ${DOTNET_NATIVE_PROJECT_PATH}/obj
  COMMENT "Generate .Net native package ${DOTNET_NATIVE_PROJECT} (${DOTNET_NATIVE_PROJECT_PATH}/timestamp)"
  WORKING_DIRECTORY ${DOTNET_NATIVE_PROJECT_PATH}
)

add_custom_target(dotnet_native_package
  DEPENDS
    ${DOTNET_NATIVE_PROJECT_PATH}/timestamp
  WORKING_DIRECTORY dotnet)

####################
##  .Net Package  ##
####################
message(STATUS ".Net project: ${DOTNET_PROJECT}")
set(DOTNET_PROJECT_PATH ${PROJECT_BINARY_DIR}/dotnet/${DOTNET_PROJECT})
message(STATUS ".Net project build path: ${DOTNET_PROJECT_PATH}")

set(PROJECT_DOTNET_DIR ${PROJECT_BINARY_DIR}/dotnet)

configure_file(
  ${PROJECT_SOURCE_DIR}/ortools/dotnet/${DOTNET_PROJECT}.csproj.in
  ${DOTNET_PROJECT_PATH}/${DOTNET_PROJECT}.csproj.in
  @ONLY)

add_custom_command(
  OUTPUT ${DOTNET_PROJECT_PATH}/${DOTNET_PROJECT}.csproj
  DEPENDS ${DOTNET_PROJECT_PATH}/${DOTNET_PROJECT}.csproj.in
  COMMAND ${CMAKE_COMMAND} -E copy ${DOTNET_PROJECT}.csproj.in ${DOTNET_PROJECT}.csproj
  WORKING_DIRECTORY ${DOTNET_PROJECT_PATH}
)

add_custom_command(
  OUTPUT ${DOTNET_PROJECT_PATH}/timestamp
  COMMAND ${DOTNET_EXECUTABLE} build -c Release /p:Platform=x64 ${DOTNET_PROJECT}.csproj
  COMMAND ${DOTNET_EXECUTABLE} pack -c Release ${DOTNET_PROJECT}.csproj
  COMMAND ${CMAKE_COMMAND} -E touch ${DOTNET_PROJECT_PATH}/timestamp
  DEPENDS
    ${PROJECT_BINARY_DIR}/dotnet/or-tools.snk
    ${DOTNET_PROJECT_PATH}/${DOTNET_PROJECT}.csproj
    dotnet_native_package
  BYPRODUCTS
    dotnet/${DOTNET_PROJECT}/bin
    dotnet/${DOTNET_PROJECT}/obj
  COMMENT "Generate .Net package ${DOTNET_PROJECT} (${DOTNET_PROJECT_PATH}/timestamp)"
  WORKING_DIRECTORY ${DOTNET_PROJECT_PATH}
)

add_custom_target(dotnet_package ALL
  DEPENDS
    ${DOTNET_PROJECT_PATH}/timestamp
  WORKING_DIRECTORY dotnet)

#################
##  .Net Test  ##
#################
# add_dotnet_test()
# CMake function to generate and build dotnet test.
# Parameters:
#  the dotnet filename
# e.g.:
# add_dotnet_test(FooTests.cs)
function(add_dotnet_test FILE_NAME)
  message(STATUS "Configuring test ${FILE_NAME} ...")
  get_filename_component(TEST_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(COMPONENT_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  set(DOTNET_TEST_PATH ${PROJECT_BINARY_DIR}/dotnet/${COMPONENT_NAME}/${TEST_NAME})
  message(STATUS "build path: ${DOTNET_TEST_PATH}")
  file(MAKE_DIRECTORY ${DOTNET_TEST_PATH})

  add_custom_command(
    OUTPUT ${DOTNET_TEST_PATH}/${TEST_NAME}.cs
    COMMAND ${CMAKE_COMMAND} -E copy ${FILE_NAME} ${DOTNET_TEST_PATH}
    MAIN_DEPENDENCY ${FILE_NAME}
    VERBATIM
  )

  set(DOTNET_PACKAGES_DIR "${PROJECT_BINARY_DIR}/dotnet/packages")
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/dotnet/Test.csproj.in
    ${DOTNET_TEST_PATH}/${TEST_NAME}.csproj
    @ONLY)

  add_custom_command(
    OUTPUT ${DOTNET_TEST_PATH}/timestamp
    COMMAND ${DOTNET_EXECUTABLE} build -c Release
    COMMAND ${CMAKE_COMMAND} -E touch ${DOTNET_TEST_PATH}/timestamp
    DEPENDS
      ${DOTNET_TEST_PATH}/${TEST_NAME}.csproj
      ${DOTNET_TEST_PATH}/${TEST_NAME}.cs
      dotnet_package
    BYPRODUCTS
      ${DOTNET_TEST_PATH}/bin
      ${DOTNET_TEST_PATH}/obj
      COMMENT "Compiling .Net ${COMPONENT_NAME}/${TEST_NAME}.cs (${DOTNET_TEST_PATH}/timestamp)"
    WORKING_DIRECTORY ${DOTNET_TEST_PATH})

  add_custom_target(dotnet_${COMPONENT_NAME}_${TEST_NAME} ALL
    DEPENDS
      ${DOTNET_TEST_PATH}/timestamp
    WORKING_DIRECTORY dotnet)

  if(BUILD_TESTING)
    add_test(
      NAME dotnet_${COMPONENT_NAME}_${TEST_NAME}
      COMMAND ${DOTNET_EXECUTABLE} test --no-build -c Release
      WORKING_DIRECTORY ${DOTNET_TEST_PATH})
  endif()
  message(STATUS "Configuring test ${FILE_NAME} done")
endfunction()

###################
##  .Net Sample  ##
###################
# add_dotnet_sample()
# CMake function to generate and build dotnet sample.
# Parameters:
#  the dotnet filename
# e.g.:
# add_dotnet_sample(FooApp.cs)
function(add_dotnet_sample FILE_NAME)
  message(STATUS "Configuring sample ${FILE_NAME} ...")
  get_filename_component(SAMPLE_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(SAMPLE_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_DIR ${SAMPLE_DIR} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  set(DOTNET_SAMPLE_PATH ${PROJECT_BINARY_DIR}/dotnet/${COMPONENT_NAME}/${SAMPLE_NAME})
  message(STATUS "build path: ${DOTNET_SAMPLE_PATH}")
  file(MAKE_DIRECTORY ${DOTNET_SAMPLE_PATH})

  add_custom_command(
    OUTPUT ${DOTNET_SAMPLE_PATH}/${SAMPLE_NAME}.cs
    COMMAND ${CMAKE_COMMAND} -E copy ${FILE_NAME} ${DOTNET_SAMPLE_PATH}
    MAIN_DEPENDENCY ${FILE_NAME}
    VERBATIM
  )

  set(DOTNET_PACKAGES_DIR "${PROJECT_BINARY_DIR}/dotnet/packages")
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/dotnet/Sample.csproj.in
    ${DOTNET_SAMPLE_PATH}/${SAMPLE_NAME}.csproj
    @ONLY)

  add_custom_command(
    OUTPUT ${DOTNET_SAMPLE_PATH}/timestamp
    COMMAND ${DOTNET_EXECUTABLE} build -c Release
    COMMAND ${DOTNET_EXECUTABLE} pack -c Release
    COMMAND ${CMAKE_COMMAND} -E touch ${DOTNET_SAMPLE_PATH}/timestamp
    DEPENDS
      ${DOTNET_SAMPLE_PATH}/${SAMPLE_NAME}.csproj
      ${DOTNET_SAMPLE_PATH}/${SAMPLE_NAME}.cs
      dotnet_package
    BYPRODUCTS
      ${DOTNET_SAMPLE_PATH}/bin
      ${DOTNET_SAMPLE_PATH}/obj
    COMMENT "Compiling .Net ${COMPONENT_NAME}/${SAMPLE_NAME}.cs (${DOTNET_SAMPLE_PATH}/timestamp)"
    WORKING_DIRECTORY ${DOTNET_SAMPLE_PATH})

  add_custom_target(dotnet_${COMPONENT_NAME}_${SAMPLE_NAME} ALL
    DEPENDS
      ${DOTNET_SAMPLE_PATH}/timestamp
    WORKING_DIRECTORY dotnet)

  if(BUILD_TESTING)
    add_test(
      NAME dotnet_${COMPONENT_NAME}_${SAMPLE_NAME}
      COMMAND ${DOTNET_EXECUTABLE} run --no-build -c Release
      WORKING_DIRECTORY ${DOTNET_SAMPLE_PATH})
  endif()
  message(STATUS "Configuring sample ${FILE_NAME} done")
endfunction()

####################
##  .Net Example  ##
####################
# add_dotnet_example()
# CMake function to generate and build dotnet example.
# Parameters:
#  the dotnet filename
# e.g.:
# add_dotnet_example(Foo.cs)
function(add_dotnet_example FILE_NAME)
  message(STATUS "Configuring example ${FILE_NAME} ...")
  get_filename_component(EXAMPLE_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(COMPONENT_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  set(DOTNET_EXAMPLE_PATH ${PROJECT_BINARY_DIR}/dotnet/${COMPONENT_NAME}/${EXAMPLE_NAME})
  message(STATUS "build path: ${DOTNET_EXAMPLE_PATH}")
  file(MAKE_DIRECTORY ${DOTNET_EXAMPLE_PATH})

  add_custom_command(
    OUTPUT ${DOTNET_EXAMPLE_PATH}/${EXAMPLE_NAME}.cs
    COMMAND ${CMAKE_COMMAND} -E copy ${FILE_NAME} ${DOTNET_EXAMPLE_PATH}
    MAIN_DEPENDENCY ${FILE_NAME}
    VERBATIM
  )

  set(DOTNET_PACKAGES_DIR "${PROJECT_BINARY_DIR}/dotnet/packages")
  set(SAMPLE_NAME ${EXAMPLE_NAME})
  configure_file(
    ${PROJECT_SOURCE_DIR}/ortools/dotnet/Sample.csproj.in
    ${DOTNET_EXAMPLE_PATH}/${EXAMPLE_NAME}.csproj
    @ONLY)

  add_custom_command(
    OUTPUT ${DOTNET_EXAMPLE_PATH}/timestamp
    COMMAND ${DOTNET_EXECUTABLE} build -c Release
    COMMAND ${DOTNET_EXECUTABLE} pack -c Release
    COMMAND ${CMAKE_COMMAND} -E touch ${DOTNET_EXAMPLE_PATH}/timestamp
    DEPENDS
      ${DOTNET_EXAMPLE_PATH}/${EXAMPLE_NAME}.csproj
      ${DOTNET_EXAMPLE_PATH}/${EXAMPLE_NAME}.cs
      dotnet_package
    BYPRODUCTS
      ${DOTNET_EXAMPLE_PATH}/bin
      ${DOTNET_EXAMPLE_PATH}/obj
      COMMENT "Compiling .Net ${COMPONENT_NAME}/${EXAMPLE_NAME}.cs (${DOTNET_EXAMPLE_PATH}/timestamp)"
    WORKING_DIRECTORY ${DOTNET_EXAMPLE_PATH})

  add_custom_target(dotnet_${COMPONENT_NAME}_${EXAMPLE_NAME} ALL
    DEPENDS
      ${DOTNET_EXAMPLE_PATH}/timestamp
    WORKING_DIRECTORY dotnet)

  if(BUILD_TESTING)
    add_test(
      NAME dotnet_${COMPONENT_NAME}_${EXAMPLE_NAME}
      COMMAND ${DOTNET_EXECUTABLE} run --no-build -c Release
      WORKING_DIRECTORY ${DOTNET_EXAMPLE_PATH})
  endif()
  message(STATUS "Configuring example ${FILE_NAME} done")
endfunction()
