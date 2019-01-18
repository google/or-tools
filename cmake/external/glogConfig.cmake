if(NOT TARGET glog::glog)
  message(STATUS "Target glog::glog not found.")
  message(STATUS "Adding CMake Subproject: Glog...")
  # Download and unpack glog at configure time
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/glog.CMakeLists.txt
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
