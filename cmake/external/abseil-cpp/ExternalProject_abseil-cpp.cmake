if(NOT TARGET absl::base)
  message(STATUS "Target absl::base not found.")
  message(STATUS "Adding CMake Subproject: Abseil...")
  # Download and unpack abseil at configure time

  # Warning : using a $<JOIN:> expression do not work here ...
  set(CMAKE_MODULE_PATH_STR "")
  foreach(path IN LISTS CMAKE_MODULE_PATH)
    string(APPEND CMAKE_MODULE_PATH_STR "${path}$<SEMICOLON>")
  endforeach(path IN LISTS CMAKE_MODULE_PATH)

  set(CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/abseil-cpp-install
    -DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_STR})

  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/abseil-cpp.CMakeLists.txt
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

  set(PKG_PATH ${CMAKE_BINARY_DIR}/abseil-cpp-install/lib/cmake/absl)

  # TODO: remove this when abseil-cpp do the install (see https://github.com/abseil/abseil-cpp/pull/182/files)
  file(INSTALL ${CMAKE_SOURCE_DIR}/patches/absl-config.cmake DESTINATION ${PKG_PATH})

  if(EXISTS ${PKG_PATH}/abslConfig.cmake)
    set(PKG_CONFIG_FILE ${PKG_PATH}/abslConfig.cmake)
  elseif(EXISTS ${PKG_PATH}/absl-config.cmake)
    set(PKG_CONFIG_FILE ${PKG_PATH}/absl-config.cmake)
  else()
    message(FATAL_ERROR "Could not find abseil-cpp cmake config file that should be installed")
  endif()

  # Create our custom FindPackage.cmake
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/abseil-cpp/Findabseil-cpp.cmake.in
    ${CMAKE_BINARY_DIR}/cmake-module/abseil-cpp/Findabseil-cpp.cmake)

  # Prepend our file to the module path
  set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/cmake-module/abseil-cpp ${CMAKE_MODULE_PATH})

  message(STATUS "Adding CMake Subproject: Abseil...DONE")
endif()
