if(NOT TARGET protobuf::libprotobuf OR NOT TARGET protobuf::protoc)
  message(STATUS "Target protobuf::libprotobuf or protobuf::protoc not found.")
  message(STATUS "Adding CMake Subproject: Protobuf...")
  # Download and unpack protobuf at configure time
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/protobuf.CMakeLists.txt
    ${CMAKE_BINARY_DIR}/protobuf-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/protobuf-download )
  if(result)
    message(FATAL_ERROR "CMake step for protobuf failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/protobuf-download )
  if(result)
    message(FATAL_ERROR "Build step for protobuf failed: ${result}")
  endif()

  set(protobuf_BUILD_TESTS OFF CACHE INTERNAL "Disable Protobuf tests")
  add_subdirectory(
    ${CMAKE_BINARY_DIR}/protobuf-src/cmake
    ${CMAKE_BINARY_DIR}/protobuf-build)
  message(STATUS "Adding CMake Subproject: Protobuf...DONE")
endif()
