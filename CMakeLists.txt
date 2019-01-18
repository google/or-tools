# This file is just an orchestration
cmake_minimum_required(VERSION 3.8.2)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# Default Build Type to be Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel. (default: Release)"
    FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# Use find_package everywhere, no-op if it's a subproject
macro(find_package)
  if(NOT TARGET ${ARGV0} AND NOT TARGET ${ARGV0}::${ARGV0})
    # Handle custom Protobuf, Zlib install path
    if (${ARGV0}_DIR AND "${ARGV0}" STREQUAL "Protobuf" OR "${ARGV0}" STREQUAL "ZLIB")
        if(EXISTS ${${ARGV0}_DIR}/${ARGV0}Config.cmake)
            include(${${ARGV0}_DIR}/${ARGV0}Config.cmake)
        elseif(EXISTS ${${ARGV0}_DIR}/${ARGV0}-config.cmake)
            include(${${ARGV0}_DIR}/${ARGV0}-config.cmake)
        endif()
    else()
        _find_package(${ARGV})
    endif()
  else()
    if(TARGET ${ARGV0})
      get_target_property(TGT_VER ${ARGV0} VERSION)
      set(TGT ${ARGV0})
    else()
      get_target_property(TGT_VER ${ARGV0}::${ARGV0} VERSION)
      set(TGT ${ARGV0}::${ARGV0})
    endif()
    message(STATUS "Found ${ARGV0}: CMake Target ${TGT} (found version \"${TGT_VER}\")")
    set(${ARGV0}_FOUND TRUE)
  endif()
endmacro()
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
if(NOT Swig_DIR AND BUILD_DEPS)
  set(Swig_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external CACHE PATH "Swig dependency path")
endif()
if(NOT gflags_DIR AND BUILD_DEPS)
  set(gflags_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external CACHE PATH "gflags dependency path")
endif()
if(NOT glog_DIR AND BUILD_DEPS)
  set(glog_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external CACHE PATH "glog dependency path")
endif()
if(NOT ZLIB_DIR AND BUILD_DEPS)
  set(ZLIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external CACHE PATH "ZLIB dependency path")
endif()
if(NOT Protobuf_DIR AND BUILD_DEPS)
  set(Protobuf_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external CACHE PATH "Protobuf dependency path")
endif()
if(NOT abseil_DIR AND BUILD_DEPS)
  set(abseil_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external CACHE PATH "abseil dependency path")
endif()
if(NOT Cbc_DIR AND BUILD_DEPS)
  set(Cbc_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external CACHE PATH "Cbc dependency path")
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
