if(NOT TARGET ZLIB::ZLIB)
  message(STATUS "Target ZLIB::ZLIB not found.")
  message(STATUS "Adding CMake Subproject: ZLIB...")
  # Download and unpack zlib at configure time
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/zlib.CMakeLists.txt
    ${CMAKE_BINARY_DIR}/zlib-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/zlib-download)
  if(result)
    message(FATAL_ERROR "CMake step for zlib failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/zlib-download)
  if(result)
    message(FATAL_ERROR "Build step for zlib failed: ${result}")
  endif()

  add_subdirectory(
    ${CMAKE_BINARY_DIR}/zlib-src
    ${CMAKE_BINARY_DIR}/zlib-build)
  message(STATUS "Adding CMake Subproject: ZLIB...DONE")
endif()
