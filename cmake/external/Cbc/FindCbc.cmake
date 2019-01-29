if(NOT TARGET Cbc::CbcSolver OR NOT TARGET Cbc::ClpSolver)
  message(STATUS "Target Cbc::CbcSolver or Cbc::ClpSolver not found.")
  message(STATUS "Adding CMake Subproject: Cbc...")
  # Download and unpack cbc at configure time
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/cbc.CMakeLists.txt
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
