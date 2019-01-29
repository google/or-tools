if(NOT TARGET absl::base)
  message(STATUS "Target absl::base not found.")
  message(STATUS "Adding CMake Subproject: Abseil...")
  # Download and unpack abseil at configure time
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/abseil.CMakeLists.txt
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
