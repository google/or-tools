if(NOT TARGET ZLIB::ZLIB)
  message(STATUS "Target ZLIB::ZLIB not found.")
  message(STATUS "Adding CMake project: ZLIB...")
  # Download and unpack zlib at configure time

  set(CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/zlib-install
    -DCMAKE_MODULE_PATH=$<JOIN:${CMAKE_MODULE_PATH},$<SEMICOLON>>)

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

  set(PKG_PATH ${CMAKE_BINARY_DIR}/zlib-install/lib/cmake/ZLIB)
  if(EXISTS ${PKG_PATH}/ZLIBConfig.cmake)
    set(PKG_CONFIG_FILE ${PKG_PATH}/ZLIBConfig.cmake)
  elseif(EXISTS ${PKG_PATH}/ZLIB-config.cmake)
    set(PKG_CONFIG_FILE ${PKG_PATH}/ZLIB-config.cmake)
  else()
    message(FATAL_ERROR "Could not find ZLIB cmake config file that should be installed")
  endif()

  # Create our custom FindPackage.cmake
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/ZLIB/FindZLIB.cmake.in
    ${CMAKE_BINARY_DIR}/cmake-module/ZLIB/FindZLIB.cmake)

  # Prepend our file to the module path
  set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/cmake-module/ZLIB ${CMAKE_MODULE_PATH})

  message(STATUS "Adding CMake project: ZLIB...DONE")
endif()
