if(NOT TARGET gflags::gflags)
  message(STATUS "Target gflags::gflags not found.")
  message(STATUS "Adding CMake Subproject: Gflags...")
  # Download and unpack gflags at configure time

  # Warning : using a $<JOIN:> expression do not work here ...
  set(CMAKE_MODULE_PATH_STR "")
  foreach(path IN LISTS CMAKE_MODULE_PATH)
    string(APPEND CMAKE_MODULE_PATH_STR "${path}$<SEMICOLON>")
  endforeach(path IN LISTS CMAKE_MODULE_PATH)

  set(CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/gflags-install
    -DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_STR}
    -DGFLAGS_INSTALL_HEADERS=ON
    -DGFLAGS_BUILD_SHARED_LIBS=OFF
    -DGFLAGS_INSTALL_SHARED_LIBS=OFF
    -DGFLAGS_BUILD_STATIC_LIBS=ON
    -DGFLAGS_INSTALL_STATIC_LIBS=ON)
    #-DGFLAGS_NAMESPACE=gflags
  #set(GFLAGS_IS_SUBPROJECT TRUE)

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

  set(PKG_PATH ${CMAKE_BINARY_DIR}/gflags-install/lib/cmake/gflags)
  if(EXISTS ${PKG_PATH}/gflagsConfig.cmake)
    set(PKG_CONFIG_FILE ${PKG_PATH}/gflagsConfig.cmake)
  elseif(EXISTS ${PKG_PATH}/gflags-config.cmake)
    set(PKG_CONFIG_FILE ${PKG_PATH}/gflags-config.cmake)
  else()
    message(FATAL_ERROR "Could not find gflags cmake config file that should be installed")
  endif()

  # Create our custom FindPackage.cmake
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/gflags/Findgflags.cmake.in
    ${CMAKE_BINARY_DIR}/cmake-module/gflags/Findgflags.cmake)

  # Prepend our file to the module path
  set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/cmake-module/gflags ${CMAKE_MODULE_PATH})

  message(STATUS "Adding CMake Subproject: Gflag...DONE")
endif()
