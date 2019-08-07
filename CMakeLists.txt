# This file is just an orchestration
cmake_minimum_required(VERSION 3.5.2)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Apple: Don't modify install_name when touching RPATH.
if(POLICY CMP0068)
  cmake_policy(SET CMP0068 NEW)
endif()

if(POLICY CMP0078)
  cmake_policy(SET CMP0078 OLD)
endif()

if(POLICY CMP0086)
  cmake_policy(SET CMP0086 NEW)
endif()

include(utils)
set_version(VERSION)

project(ortools VERSION ${VERSION} LANGUAGES CXX)
message(STATUS "version: ${PROJECT_VERSION}")

# Default Build Type to be Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel. (default: Release)"
    FORCE)
endif()

if(UNIX)
  option(BUILD_SHARED_LIBS "Build shared libraries(.so)." ON)
elseif(MSVC)
  # Windows only support static build.
  set(BUILD_SHARED_LIBS OFF)
endif()

# By default only the ortools C++ library is built.
option(BUILD_CXX "Build C++ library" ON)
option(BUILD_PYTHON "Build Python Library" OFF)
option(BUILD_JAVA "Build Java Library" OFF)
option(BUILD_DOTNET "Build .NET Library" OFF)
option(USE_XPRESS "Build and use XPRESS interface" OFF)
option(USE_CPLEX "Build and use CPLEX interface" OFF)
message(STATUS "Build C++ library: ${BUILD_CXX}")
message(STATUS "Build Python: ${BUILD_PYTHON}")
message(STATUS "Build Java: ${BUILD_JAVA}")
message(STATUS "Build .Net: ${BUILD_DOTNET}")
message(STATUS "USE_XPRESS: ${USE_XPRESS}")
message(STATUS "USE_CPLEX: ${USE_CPLEX}")

#option(BUILD_DOC "Build doxygen" OFF)
#message(STATUS "Build doxygen: ${BUILD_DOC}")

# By default all dependencies are NOT built (i.e. BUILD_DEPS=OFF).
# IF building any wrapper THEN Force BUILD_DEPS=ON
# IF BUILD_DEPS=ON THEN Force all BUILD_*=ON
include(CMakeDependentOption)
CMAKE_DEPENDENT_OPTION(BUILD_DEPS "Force re-build of all dependencies" OFF
  "NOT BUILD_PYTHON; NOT BUILD_JAVA; NOT BUILD_DOTNET" ON)
CMAKE_DEPENDENT_OPTION(BUILD_ZLIB "Build the ZLIB dependency Library" OFF
  "NOT BUILD_DEPS" ON)
CMAKE_DEPENDENT_OPTION(BUILD_absl "Build the abseil-cpp dependency Library" OFF
  "NOT BUILD_DEPS" ON)
CMAKE_DEPENDENT_OPTION(BUILD_gflags "Build the gflags dependency Library" OFF
  "NOT BUILD_DEPS" ON)
CMAKE_DEPENDENT_OPTION(BUILD_glog "Build the glog dependency Library" OFF
  "NOT BUILD_DEPS" ON)
CMAKE_DEPENDENT_OPTION(BUILD_Protobuf "Build the Protobuf dependency Library" OFF
  "NOT BUILD_DEPS" ON)
CMAKE_DEPENDENT_OPTION(BUILD_CoinUtils "Build the CoinUtils dependency Library" OFF
  "NOT BUILD_DEPS" ON)
CMAKE_DEPENDENT_OPTION(BUILD_Osi "Build the Osi dependency Library" OFF
  "NOT BUILD_DEPS" ON)
CMAKE_DEPENDENT_OPTION(BUILD_Clp "Build the Clp dependency Library" OFF
  "NOT BUILD_DEPS" ON)
CMAKE_DEPENDENT_OPTION(BUILD_Cgl "Build the Cgl dependency Library" OFF
  "NOT BUILD_DEPS" ON)
CMAKE_DEPENDENT_OPTION(BUILD_Cbc "Build the Cbc dependency Library" OFF
  "NOT BUILD_DEPS" ON)

message(STATUS "Build all dependencies: ${BUILD_DEPS}")
message(STATUS "Build ZLIB: ${BUILD_ZLIB}")
message(STATUS "Build abseil-cpp: ${BUILD_absl}")
message(STATUS "Build gflags: ${BUILD_gflags}")
message(STATUS "Build glog: ${BUILD_glog}")
message(STATUS "Build protobuf: ${BUILD_Protobuf}")
message(STATUS "Build CoinUtils: ${BUILD_CoinUtils}")
message(STATUS "Build Osi: ${BUILD_Osi}")
message(STATUS "Build Clp: ${BUILD_Clp}")
message(STATUS "Build Cgl: ${BUILD_Cgl}")
message(STATUS "Build Cbc: ${BUILD_Cbc}")

if (USE_XPRESS)

	if (APPLE)
		message(FATAL_ERROR "XPRESS not yet supported on MACOS")
	endif()
	
	message(STATUS "XPRESS: configuring makefiles")
	if (NOT XPRESSDIR)
		set(XPRESSDIR $ENV{XPRESSDIR})
	endif(NOT XPRESSDIR)

	message(STATUS "XPRESSDIR: ${XPRESSDIR}")

	if (NOT XPRESSDIR)
		message(FATAL_ERROR "XPRESSDIR: not found")
	endif()

	add_compile_definitions(USE_XPRESS)
endif(USE_XPRESS)


if (USE_CPLEX)

	if (APPLE)
		message(FATAL_ERROR "CPLEX not yet supported on MACOS")
	elseif(UNIX)
		message(FATAL_ERROR "CPLEX not yet supported on LINUX")
	endif()
	
	message(STATUS "CPLEX: configuring makefiles")
	if (NOT CPLEXDIR)
		set(CPLEXDIR $ENV{CPLEXDIR})
	endif(NOT CPLEXDIR)

	message(STATUS "CPLEXDIR: ${CPLEXDIR}")

	if (NOT CPLEXDIR)
		message(FATAL_ERROR "CPLEXDIR: not found")
	endif()

	add_compile_definitions(USE_CPLEX)
endif(USE_CPLEX)

# Build Needed dependencies
add_subdirectory(cmake/dependencies dependencies)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/dependencies/install)

include(CTest)

include(cpp)

include(python)
include(java)
include(dotnet)

if (USE_XPRESS)
	if (APPLE)
		message(FATAL_ERROR "XPRESS not yet supported on MACOS")
		#target_link_libraries(ortools PUBLIC ${XPRESSDIR}/lib/libxprs.dylib)
	elseif(UNIX)
		target_link_libraries(ortools PUBLIC ${XPRESSDIR}/lib/libxprs.so)
	elseif(MSVC)
		target_link_libraries(ortools PUBLIC ${XPRESSDIR}/lib/xprs.lib)
	endif()
endif(USE_XPRESS)


if (USE_CPLEX)
	if (APPLE)
		message(FATAL_ERROR "CPLEX not yet supported on MACOS")
		#target_link_libraries(ortools PUBLIC ${CPLEXDIR}/lib/x64_windows_vs2015/stat_mda/cplex1270.dylib)
	elseif(UNIX)
		message(FATAL_ERROR "CPLEX not yet supported on LINUX")
		#target_link_libraries(ortools PUBLIC ${CPLEXDIR}/lib/x64_windows_vs2015/stat_mda/cplex1270.so)
	elseif(MSVC)
		target_link_libraries(ortools PUBLIC ${CPLEXDIR}/lib/x64_windows_vs2015/stat_mda/cplex1270.lib)
	endif()
endif(USE_CPLEX)
