if(WIN32 AND (BUILD_PYTHON OR BUILD_JAVA OR BUILD_CSHARP))
  message(STATUS "Adding CMake Subproject: Swig...")
  # Download and unpack swig at configure time
  configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/swig.CMakeLists.txt
    ${CMAKE_BINARY_DIR}/swig-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/swig-download )
  if(result)
    message(FATAL_ERROR "CMake step for swig failed: ${result}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/swig-download )
  if(result)
    message(FATAL_ERROR "Build step for swig failed: ${result}")
  endif()

  set(SWIG_EXECUTABLE ${CMAKE_BINARY_DIR}/swig/swig.exe
    CACHE INTERNAL "swig.exe location" FORCE)
  message(STATUS "Adding CMake Subproject: Swig...DONE")
endif()
