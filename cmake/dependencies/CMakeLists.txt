# ##############################################################################
# SWIG (WIN32)
# ##############################################################################
if(WIN32
   AND (BUILD_PYTHON
        OR BUILD_JAVA
        OR BUILD_DOTNET))
  message(STATUS "Getting SWIG: ...")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/SWIG.CMakeLists.txt.in
                 ${CMAKE_CURRENT_BINARY_DIR}/SWIG/CMakeLists.txt @ONLY)

  execute_process(
    COMMAND ${CMAKE_COMMAND} -H. -Bproject_build -G "${CMAKE_GENERATOR}"
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/SWIG)
  if(result)
    message(FATAL_ERROR "CMake step for SWIG failed: ${result}")
  endif()

  execute_process(
    COMMAND ${CMAKE_COMMAND} --build project_build --config ${CMAKE_BUILD_TYPE}
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/SWIG)
  if(result)
    message(FATAL_ERROR "Build step for SWIG failed: ${result}")
  endif()
  message(STATUS "Getting SWIG: ...DONE")

  set(SWIG_EXECUTABLE
      ${CMAKE_CURRENT_BINARY_DIR}/SWIG/source/swig.exe
      CACHE INTERNAL "swig.exe location" FORCE)
endif()

include(FetchContent)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_TESTING OFF)

# ##############################################################################
# ZLIB
# ##############################################################################
if(BUILD_ZLIB)
  FetchContent_Declare(
    zlib
    GIT_REPOSITORY "https://github.com/madler/ZLIB.git"
    GIT_TAG "v1.2.11"
    PATCH_COMMAND git apply "${CMAKE_CURRENT_LIST_DIR}/../../patches/ZLIB.patch")
  FetchContent_MakeAvailable(zlib)
endif()

# ##############################################################################
# SCIP
# ##############################################################################
if(BUILD_SCIP)
  set(SHARED OFF)
  set(READLINE OFF)
  set(GMP OFF)
  set(PAPILO OFF)
  set(ZIMPL OFF)
  set(IPOPT OFF)
  set(LPS "none")
  #set(EXPRINT "none")
  #set(TPI "tny")
  FetchContent_Declare(
    scip
    URL "${CMAKE_CURRENT_LIST_DIR}/../../dependencies/archives/scip-7.0.1.tgz"
    PATCH_COMMAND patch -i "${CMAKE_CURRENT_LIST_DIR}/../../patches/scip-7.0.1.patch"
  )
  FetchContent_MakeAvailable(scip)
endif()

# ##############################################################################
# ABSEIL-CPP
# ##############################################################################
if(BUILD_absl)
  set(ABSL_ENABLE_INSTALL ON)
  FetchContent_Declare(
    absl
    GIT_REPOSITORY "https://github.com/abseil/abseil-cpp.git"
    GIT_TAG "20200225.2"
    PATCH_COMMAND git apply
    "${CMAKE_CURRENT_LIST_DIR}/../../patches/abseil-cpp-20200225.2.patch")
  FetchContent_MakeAvailable(absl)
endif()

# ##############################################################################
# GFLAGS
# ##############################################################################
if(BUILD_gflags)
  set(INSTALL_HEADERS ON)
  set(INSTALL_SHARED_LIBS OFF)
  set(INSTALL_SHARED_LIBS OFF)
  set(BUILD_STATIC_LIBS ON)
  set(INSTALL_STATIC_LIBS ON)
  FetchContent_Declare(
    gflags
    GIT_REPOSITORY "https://github.com/gflags/gflags.git"
    GIT_TAG "v2.2.2"
    PATCH_COMMAND git apply "${CMAKE_CURRENT_LIST_DIR}/../../patches/gflags-v2.2.2.patch")
  FetchContent_MakeAvailable(gflags)
endif()

# ##############################################################################
# GLOG
# ##############################################################################
if(BUILD_glog)
  FetchContent_Declare(
    glog
    GIT_REPOSITORY "https://github.com/google/glog.git"
    GIT_TAG "v0.4.0")
  set(WITH_GFLAGS OFF)
  FetchContent_MakeAvailable(glog)
endif()

# ##############################################################################
# Protobuf
# ##############################################################################
message(WARNING "fetching protobuf...")
if(BUILD_Protobuf)
  # FetchContent_Declare(SOURCE_SUBDIR) was introduced in 3.18
  if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.18")
    set(protobuf_BUILD_TESTS OFF)
    set(protobuf_MSVC_STATIC_RUNTIME OFF)
    FetchContent_Declare(
      protobuf
      GIT_REPOSITORY "https://github.com/protocolbuffers/protobuf.git"
      GIT_TAG "v3.12.2"
      SOURCE_SUBDIR cmake)
    FetchContent_MakeAvailable(protobuf)
  else()
    build_git_dependency(
      NAME Protobuf
      REPOSITORY "https://github.com/protocolbuffers/protobuf.git"
      TAG "v3.12.2"
      APPLY_PATCH "${CMAKE_CURRENT_LIST_DIR}/../../patches/protobuf-v3.12.2.patch"
      CMAKE_ARGS
      -Dprotobuf_MSVC_STATIC_RUNTIME:BOOL=OFF
      -Dprotobuf_BUILD_TESTS:BOOL=OFF
      "SOURCE_SUBDIR cmake"
      )
  endif()
endif()

# ##############################################################################
# Coinutils
# ##############################################################################
if(BUILD_CoinUtils)
  FetchContent_Declare(
    CoinUtils
    GIT_REPOSITORY "https://github.com/Mizux/CoinUtils.git"
    GIT_TAG "stable/2.11")
  FetchContent_MakeAvailable(CoinUtils)
endif()

# ##############################################################################
# Osi
# ##############################################################################
if(BUILD_Osi)
  FetchContent_Declare(
    Osi
    GIT_REPOSITORY "https://github.com/Mizux/Osi.git"
    GIT_TAG "stable/0.108")
  FetchContent_MakeAvailable(Osi)
endif()

# ##############################################################################
# Clp
# ##############################################################################
if(BUILD_Clp)
  FetchContent_Declare(
    Clp
    GIT_REPOSITORY "https://github.com/Mizux/Clp.git"
    GIT_TAG "stable/1.17.4")
  FetchContent_MakeAvailable(Clp)
endif()

# ##############################################################################
# Cgl
# ##############################################################################
if(BUILD_Cgl)
  FetchContent_Declare(
    Cgl
    GIT_REPOSITORY "https://github.com/Mizux/Cgl.git"
    GIT_TAG "stable/0.60")
  FetchContent_MakeAvailable(Cgl)
endif()

# ##############################################################################
# Cbc
# ##############################################################################
if(BUILD_Cbc)
  FetchContent_Declare(
    Cbc
    GIT_REPOSITORY "https://github.com/Mizux/Cbc.git"
    GIT_TAG "stable/2.10")
  FetchContent_MakeAvailable(Cbc)
endif()
