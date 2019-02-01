# This file is just an orchestration
cmake_minimum_required(VERSION 3.8.2)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# Default Build Type to be Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel. (default: Release)"
    FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# Apple: Don't modify install_name when touching RPATH.
if(POLICY CMP0068)
  cmake_policy(SET CMP0068 NEW)
endif()

project(ortools-meta NONE)

include(CTest)
if(UNIX)
  # Needed to create python package from the build directory
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  option(BUILD_SHARED_LIBS "Build shared libraries(.so)." ON)
elseif(MSVC)
  # Windows only support static build.
  set(BUILD_SHARED_LIBS OFF)
endif()

# When incorporating ortools in a CMake Project, then only C++ library will be built.
# Consequently Python, Java and C# wrapper won't be built.
if("^${CMAKE_SOURCE_DIR}$" STREQUAL "^${PROJECT_SOURCE_DIR}$")
  set(ORTOOLS_IS_SUBPROJECT FALSE)
else()
  set(ORTOOLS_IS_SUBPROJECT TRUE)
endif()

include(CMakeDependentOption)
option(BUILD_DEPS "Force re-build of all dependencies" ON)
CMAKE_DEPENDENT_OPTION(BUILD_gflags "Install the gflags dependency Library" ON
  "BUILD_DEPS;" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_glog "Install the glog dependency Library" ON
  "BUILD_DEPS;" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_ZLIB "Install the ZLIB dependency Library" ON
  "BUILD_DEPS;" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_Protobuf "Install the Protobuf dependency Library" ON
  "BUILD_DEPS;" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_abseil-cpp "Install the abseil-cpp dependency Library" ON
  "BUILD_DEPS;" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_Cbc "Install the Cbc dependency Library" ON
  "BUILD_DEPS;" OFF)

option(BUILD_CXX "Build C++ library" ON)
CMAKE_DEPENDENT_OPTION(BUILD_PYTHON "Build Python Library" OFF
  "BUILD_CXX; NOT ORTOOLS_IS_SUBPROJECT" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_JAVA "Build Java Library" OFF
  "BUILD_CXX; NOT ORTOOLS_IS_SUBPROJECT" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_DOTNET "Build .NET Library" OFF
  "BUILD_CXX; NOT ORTOOLS_IS_SUBPROJECT" OFF)

message(STATUS "Build all dependencies: ${BUILD_DEPS}")
message(STATUS "Build CXX library: ${BUILD_CXX}")
message(STATUS "Build Python Binding: ${BUILD_PYTHON}")
message(STATUS "Build Java Binding: ${BUILD_JAVA}")
message(STATUS "Build .Net Binding: ${BUILD_DOTNET}")

# Add OR Tools Dependencies as CMake subproject if missing
if(NOT DEFINED Swig_MODULE_PATH AND BUILD_DEPS)
  set(Swig_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/Swig)
endif()
if(BUILD_gflags)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/gflags/ExternalProject_gflags.cmake)
endif()
if(BUILD_glog)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/glog/ExternalProject_glog.cmake)
endif()
if(BUILD_ZLIB)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/ZLIB/ExternalProject_ZLIB.cmake)
endif()
if(BUILD_Protobuf)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/Protobuf/ExternalProject_Protobuf.cmake)
endif()
if(BUILD_abseil-cpp)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/abseil-cpp/ExternalProject_abseil-cpp.cmake)
endif()
if(BUILD_Cbc)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/Cbc/ExternalProject_Cbc.cmake)
endif()

if(BUILD_CXX)
  include(cpp)
  if(BUILD_TESTING)
    add_subdirectory(examples/cpp)
  endif()
endif()

if(BUILD_PYTHON)
  include(python)
  if(BUILD_TESTING)
    add_subdirectory(examples/python)
    add_subdirectory(examples/notebook)
  endif()
endif()

if(BUILD_JAVA)
  include(java)
  if(BUILD_TESTING)
    add_subdirectory(examples/java)
  endif()
endif()

if(BUILD_DOTNET)
  include(dotnet)
  if(BUILD_TESTING)
    add_subdirectory(examples/dotnet)
  endif()
endif()
