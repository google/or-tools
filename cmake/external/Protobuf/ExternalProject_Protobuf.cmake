if(NOT TARGET protobuf::libprotobuf OR NOT TARGET protobuf::protoc)
  message(STATUS "Target protobuf::libprotobuf or protobuf::protoc not found.")
  message(STATUS "Adding CMake Subproject: Protobuf...")
  # Download and unpack protobuf at configure time

  # Warning : using a $<JOIN:> expression do not work here ...
  set(CMAKE_MODULE_PATH_STR "")
  foreach(path IN LISTS CMAKE_MODULE_PATH)
    string(APPEND CMAKE_MODULE_PATH_STR "${path}$<SEMICOLON>")
  endforeach(path IN LISTS CMAKE_MODULE_PATH)

  set(CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/protobuf-install
    -DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_STR}
    -Dprotobuf_BUILD_TESTS=OFF)

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

  set(PKG_PATH ${CMAKE_BINARY_DIR}/protobuf-install/lib/cmake/Protobuf)
  if(EXISTS ${PKG_PATH}/ProtobufConfig.cmake)
    set(PKG_CONFIG_FILE ${PKG_PATH}/ProtobufConfig.cmake)
  elseif(EXISTS ${PKG_PATH}/Protobuf-config.cmake)
    set(PKG_CONFIG_FILE ${PKG_PATH}/Protobuf-config.cmake)
  else()
    message(FATAL_ERROR "Could not find Protobuf cmake config file that should be installed")
  endif()

  # Create our custom FindPackage.cmake
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/Protobuf/FindProtobuf.cmake.in
    ${CMAKE_BINARY_DIR}/cmake-module/Protobuf/FindProtobuf.cmake)

  # Prepend our file to the module path
  set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/cmake-module/Protobuf ${CMAKE_MODULE_PATH})

  message(STATUS "Adding CMake Subproject: Protobuf...DONE")
endif()
