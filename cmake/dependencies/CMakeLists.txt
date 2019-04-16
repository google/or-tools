####################
##  SWIG (WIN32)  ##
####################
if(WIN32 AND (BUILD_PYTHON OR BUILD_JAVA OR BUILD_DOTNET))
  message(STATUS "Getting SWIG: ...")
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/SWIG.CMakeLists.txt.in
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

  set(SWIG_EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/SWIG/source/swig.exe
    CACHE INTERNAL "swig.exe location" FORCE)
endif()

############
##  ZLIB  ##
############
if(BUILD_ZLIB)
  build_git_dependency(
    NAME
      ZLIB
    REPOSITORY
      "https://github.com/madler/ZLIB.git"
    TAG
      "v1.2.11"
    APPLY_PATCH
      "${CMAKE_SOURCE_DIR}/patches/ZLIB.patch"
 )
endif()

##################
##  ABSEIL-CPP  ##
##################
if(BUILD_absl)
  build_git_dependency(
    NAME
      abseil-cpp
    REPOSITORY
      "https://github.com/abseil/abseil-cpp.git"
    TAG
      "master"
 )
endif()

##############
##  GFLAGS  ##
##############
if(BUILD_gflags)
  build_git_dependency(
    NAME
      gflags
    REPOSITORY
      "https://github.com/gflags/gflags.git"
    TAG
      "v2.2.2"
    CMAKE_ARGS
      -DINSTALL_SHARED_LIBS:BOOL=OFF
      -DBUILD_STATIC_LIBS:BOOL=ON
      -DINSTALL_STATIC_LIBS:BOOL=ON
  )
endif()

############
##  GLOG  ##
############
if(BUILD_glog)
  build_git_dependency(
    NAME
      glog
    REPOSITORY
      "https://github.com/google/glog.git"
    TAG
      "v0.4.0"
    CMAKE_ARGS
      -DWITH_GFLAGS:BOOL=OFF
  )
endif()

################
##  Protobuf  ##
################
if(BUILD_Protobuf)
  build_git_dependency(
    NAME
      Protobuf
    REPOSITORY
      "https://github.com/protocolbuffers/protobuf.git"
    TAG
      "v3.7.1"
    CMAKE_ARGS
      "SOURCE_SUBDIR cmake"
  )
endif()

#################
##  Coinutils  ##
#################
if(BUILD_CoinUtils)
  build_git_dependency(
    NAME
      CoinUtils
    REPOSITORY
      "https://github.com/Mizux/CoinUtils.git"
    TAG
      "stable/2.11"
  )
endif()

###########
##  Osi  ##
###########
if(BUILD_Osi)
  build_git_dependency(
    NAME
      Osi
    REPOSITORY
      "https://github.com/Mizux/Osi.git"
    TAG
      "stable/0.108"
  )
endif()

###########
##  Clp  ##
###########
if(BUILD_Clp)
  build_git_dependency(
    NAME
      Clp
    REPOSITORY
      "https://github.com/Mizux/Clp.git"
    TAG
      "stable/1.17"
  )
endif()

###########
##  Cgl  ##
###########
if(BUILD_Cgl)
  build_git_dependency(
    NAME
      Cgl
    REPOSITORY
      "https://github.com/Mizux/Cgl.git"
    TAG
      "stable/0.60"
  )
endif()

###########
##  Cbc  ##
###########
if(BUILD_Cbc)
  build_git_dependency(
    NAME
      Cbc
    REPOSITORY
      "https://github.com/Mizux/Cbc.git"
    TAG
      "stable/2.10"
  )
endif()
