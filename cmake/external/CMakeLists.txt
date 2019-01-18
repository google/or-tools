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

############
##  ZLIB  ##
############

################
##  PROTOBUF  ##
################

##############
##  ABSEIL  ##
##############

###########
##  CBC  ##
###########

# Reapply previous state
set(BUILD_SHARED_LIBS ${BUILD_SHARED_BCKP})
