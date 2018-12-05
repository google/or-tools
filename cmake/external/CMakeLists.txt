list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Force dependencies to be built as static
set(BUILD_SHARED_BCKP ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)

# Disable test rules for dependencies
set(BUILD_TESTING OFF)

# Build Dependencies
#file(READ ${PROJECT_SOURCE_DIR}/Dependencies.txt _Dependency_file)
#foreach(DEPENDENCY Protobuf gflags glog)
#    string(REGEX REPLACE ".*${DEPENDENCY} = ([0-9.]+).*" "\\1" ${DEPENDENCY}_VERSION ${_Dependency_file})
#endforeach()

####################
##  SWIG (WIN32)  ##
####################
if(WIN32 AND (BUILD_PYTHON OR BUILD_JAVA OR BUILD_CSHARP))
  message(STATUS "Adding CMake Subproject: Swig...")
  # Download and unpack swig at configure time
  configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/swig.CMakeLists.txt
    ${CMAKE_BINARY_DIR}/swig-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/swig-download )
  if(result)
    message(FATAL_ERROR "CMake step for swig failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/swig-download )
  if(result)
    message(FATAL_ERROR "Build step for swig failed: ${result}")
  endif()

  set(SWIG_EXECUTABLE ${CMAKE_BINARY_DIR}/swig/swig.exe
    CACHE INTERNAL "swig.exe location" FORCE)
  message(STATUS "Adding CMake Subproject: Swig...DONE")
endif()

##############
##  GFLAGS  ##
##############
if(NOT TARGET gflags::gflags)
  message(STATUS "Target gflags::gflags not found.")
  message(STATUS "Adding CMake Subproject: Gflags...")
  # Download and unpack gflags at configure time
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/gflags.CMakeLists.txt
    ${CMAKE_BINARY_DIR}/gflags-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/gflags-download)
  if(result)
    message(FATAL_ERROR "CMake step for gflags failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/gflags-download)
  if(result)
    message(FATAL_ERROR "Build step for gflags failed: ${result}")
  endif()

  set(GFLAGS_NAMESPACE "gflags")
  set(GFLAGS_INSTALL_HEADERS ON)
  set(GFLAGS_BUILD_SHARED_LIBS OFF)
  set(GFLAGS_INSTALL_SHARED_LIBS OFF)
  set(GFLAGS_BUILD_STATIC_LIBS ON)
  set(GFLAGS_INSTALL_STATIC_LIBS ON)
  #set(GFLAGS_IS_SUBPROJECT TRUE)
  add_subdirectory(
    ${CMAKE_BINARY_DIR}/gflags-src
    ${CMAKE_BINARY_DIR}/gflags-build)
  message(STATUS "Adding CMake Subproject: Gflag...DONE")
endif()

############
##  GLOG  ##
############
if(NOT TARGET glog::glog)
  message(STATUS "Target glog::glog not found.")
  message(STATUS "Adding CMake Subproject: Glog...")
  # Download and unpack glog at configure time
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/glog.CMakeLists.txt
    ${CMAKE_BINARY_DIR}/glog-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/glog-download)
  if(result)
    message(FATAL_ERROR "CMake step for glog failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/glog-download)
  if(result)
    message(FATAL_ERROR "Build step for glog failed: ${result}")
  endif()

  set(gflags_NAMESPACE "gflags" CACHE INTERNAL "Namespace for gflags")
  add_subdirectory(
    ${CMAKE_BINARY_DIR}/glog-src
    ${CMAKE_BINARY_DIR}/glog-build)
  message(STATUS "Adding CMake Subproject: Glog...DONE")
endif()

############
##  ZLIB  ##
############
if(NOT TARGET ZLIB::ZLIB)
  message(STATUS "Target ZLIB::ZLIB not found.")
  message(STATUS "Adding CMake Subproject: ZLIB...")
  # Download and unpack zlib at configure time
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/zlib.CMakeLists.txt
    ${CMAKE_BINARY_DIR}/zlib-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/zlib-download)
  if(result)
    message(FATAL_ERROR "CMake step for zlib failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/zlib-download)
  if(result)
    message(FATAL_ERROR "Build step for zlib failed: ${result}")
  endif()

  add_subdirectory(
    ${CMAKE_BINARY_DIR}/zlib-src
    ${CMAKE_BINARY_DIR}/zlib-build)
  message(STATUS "Adding CMake Subproject: ZLIB...DONE")
endif()

################
##  PROTOBUF  ##
################
if(NOT TARGET protobuf::libprotobuf OR NOT TARGET protobuf::protoc)
  message(STATUS "Target protobuf::libprotobuf or protobuf::protoc not found.")
  message(STATUS "Adding CMake Subproject: Protobuf...")
  # Download and unpack protobuf at configure time
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/protobuf.CMakeLists.txt
    ${CMAKE_BINARY_DIR}/protobuf-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/protobuf-download )
  if(result)
    message(FATAL_ERROR "CMake step for protobuf failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/protobuf-download )
  if(result)
    message(FATAL_ERROR "Build step for protobuf failed: ${result}")
  endif()

  set(protobuf_BUILD_TESTS OFF CACHE INTERNAL "Disable Protobuf tests")
  add_subdirectory(
    ${CMAKE_BINARY_DIR}/protobuf-src/cmake
    ${CMAKE_BINARY_DIR}/protobuf-build)
  message(STATUS "Adding CMake Subproject: Protobuf...DONE")
endif()

##############
##  ABSEIL  ##
##############
if(NOT TARGET absl::base)
  message(STATUS "Target absl::base not found.")
  message(STATUS "Adding CMake Subproject: Abseil...")
  # Download and unpack abseil at configure time
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/abseil.CMakeLists.txt
    ${CMAKE_BINARY_DIR}/abseil-cpp-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/abseil-cpp-download)
  if(result)
    message(FATAL_ERROR "CMake step for abseil failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/abseil-cpp-download)
  if(result)
    message(FATAL_ERROR "Build step for abseil failed: ${result}")
  endif()

  add_subdirectory(
    ${CMAKE_BINARY_DIR}/abseil-cpp-src
    ${CMAKE_BINARY_DIR}/abseil-cpp-build)
  message(STATUS "Adding CMake Subproject: Abseil...DONE")
endif()

###########
##  CBC  ##
###########
if(NOT TARGET Cbc::CbcSolver OR NOT TARGET Cbc::ClpSolver)
  message(STATUS "Target Cbc::CbcSolver or Cbc::ClpSolver not found.")
  message(STATUS "Adding CMake Subproject: Cbc...")
  # Download and unpack cbc at configure time
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cbc.CMakeLists.txt
    ${CMAKE_BINARY_DIR}/cbc-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/cbc-download)
  if(result)
    message(FATAL_ERROR "CMake step for cbc failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/cbc-download)
  if(result)
    message(FATAL_ERROR "Build step for cbc failed: ${result}")
  endif()

  add_subdirectory(
    ${CMAKE_BINARY_DIR}/cbc-src
    ${CMAKE_BINARY_DIR}/cbc-build)
  message(STATUS "Adding CMake Subproject: Cbc...DONE")
endif()

# Reapply previous state
set(BUILD_SHARED_LIBS ${BUILD_SHARED_BCKP})
