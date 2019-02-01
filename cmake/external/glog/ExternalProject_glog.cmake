if(NOT TARGET glog::glog)
  message(STATUS "Target glog::glog not found.")
  message(STATUS "Adding CMake Subproject: Glog...")

  # Warning : using a $<JOIN:> expression do not work here ...
  set(CMAKE_MODULE_PATH_STR "")
  foreach(path IN LISTS CMAKE_MODULE_PATH)
    string(APPEND CMAKE_MODULE_PATH_STR "${path}$<SEMICOLON>")
  endforeach(path IN LISTS CMAKE_MODULE_PATH)

  # Config the external project
  set(CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/glog-install
    -DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_STR}
    -DBUILD_TESTING=OFF
    -Dgflags_NAMESPACE="gflags")

  # Download and unpack glog at configure time
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/glog.CMakeLists.txt
    ${CMAKE_BINARY_DIR}/glog-download/CMakeLists.txt)

  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/glog-download)
  if(result)
    message(FATAL_ERROR "CMake step for glog failed: ${result}")
  endif()

  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/glog-download)
  if(result)
    message(FATAL_ERROR "Build step for glog failed: ${result}")
  endif()

  set(PKG_PATH ${CMAKE_BINARY_DIR}/glog-install/lib/cmake/glog)
  if(EXISTS ${PKG_PATH}/glogConfig.cmake)
    set(PKG_CONFIG_FILE ${PKG_PATH}/glogConfig.cmake)
  elseif(EXISTS ${PKG_PATH}/glog-config.cmake)
    set(PKG_CONFIG_FILE ${PKG_PATH}/glog-config.cmake)
  else()
    message(FATAL_ERROR "Could not find glog cmake config file that should be installed")
  endif()

  # Create our custom FindPackage.cmake
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/external/glog/Findglog.cmake.in
    ${CMAKE_BINARY_DIR}/cmake-module/glog/Findglog.cmake)

  # Prepend our file to the module path
  set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/cmake-module/glog ${CMAKE_MODULE_PATH})

  message(STATUS "Adding CMake Subproject: Glog...DONE")
endif()
