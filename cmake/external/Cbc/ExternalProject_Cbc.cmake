if(NOT TARGET Cbc::CbcSolver OR NOT TARGET Cbc::ClpSolver)
  message(STATUS "Target Cbc::CbcSolver or Cbc::ClpSolver not found.")
  message(STATUS "Adding CMake Subproject: Cbc...")
  # Download and unpack cbc at configure time

  # Warning : using a $<JOIN:> expression do not work here ...
  set(CMAKE_MODULE_PATH_STR "")
  foreach(path IN LISTS CMAKE_MODULE_PATH)
    string(APPEND CMAKE_MODULE_PATH_STR "${path}$<SEMICOLON>")
  endforeach(path IN LISTS CMAKE_MODULE_PATH)

  set(CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/cbc-install
    -DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_STR})

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

  set(PKG_PATH ${CMAKE_BINARY_DIR}/cbc-install/lib/cmake/Cbc)
  if(EXISTS ${PKG_PATH}/CbcConfig.cmake)
    set(PKG_CONFIG_FILE ${PKG_PATH}/CbcConfig.cmake)
  elseif(EXISTS ${PKG_PATH}/Cbc-config.cmake)
    set(PKG_CONFIG_FILE ${PKG_PATH}/Cbc-config.cmake)
  else()
    message(FATAL_ERROR "Could not find Cbc cmake config file that should be installed")
  endif()

  # Create our custom FindPackage.cmake
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/Cbc/FindCbc.cmake.in
    ${CMAKE_BINARY_DIR}/cmake-module/Cbc/FindCbc.cmake)

  # Prepend our file to the module path
  set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/cmake-module/Cbc ${CMAKE_MODULE_PATH})

  message(STATUS "Adding CMake Subproject: Cbc...DONE")
endif()
