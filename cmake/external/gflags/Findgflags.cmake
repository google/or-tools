if(NOT TARGET gflags::gflags)
  message(STATUS "Target gflags::gflags not found.")
  message(STATUS "Adding CMake Subproject: Gflags...")
  # Download and unpack gflags at configure time
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/gflags.CMakeLists.txt
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
