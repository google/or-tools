function(get_version_from_file VERSION_MAJOR VERSION_MINOR VERSION_PATCH)
  file(STRINGS "Version.txt" VERSION_STR)
  foreach(STR ${VERSION_STR})
    if(${STR} MATCHES "OR_TOOLS_MAJOR=(.*)")
      set(${VERSION_MAJOR} ${CMAKE_MATCH_1} PARENT_SCOPE)
    endif()
    if(${STR} MATCHES "OR_TOOLS_MINOR=(.*)")
      set(${VERSION_MINOR} ${CMAKE_MATCH_1} PARENT_SCOPE)
    endif()
  endforeach()
  set(${VERSION_PATCH} 9999 PARENT_SCOPE)
endfunction()

function(get_version_from_git VERSION_MAJOR VERSION_MINOR VERSION_PATCH)
  find_package(Git QUIET)
  if(NOT GIT_FOUND)
    message(STATUS "Did not find git package, get version from file...")
    get_version_from_file(MAJOR MINOR PATCH)
  else()
    execute_process(COMMAND
      ${GIT_EXECUTABLE} describe --tags
      RESULT_VARIABLE _OUTPUT_VAR
      OUTPUT_VARIABLE FULL
      ERROR_QUIET
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    if(NOT _OUTPUT_VAR)
      execute_process(COMMAND
        ${GIT_EXECUTABLE} rev-list HEAD --count
        RESULT_VARIABLE _OUTPUT_VAR
        OUTPUT_VARIABLE PATCH
        ERROR_QUIET
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
      STRING(STRIP PATCH ${PATCH})
      STRING(REGEX REPLACE "\n$" "" PATCH ${PATCH})
      STRING(REGEX REPLACE " " "" PATCH ${PATCH})
      STRING(REGEX REPLACE "^v([0-9]+)\\..*" "\\1" MAJOR "${FULL}")
      STRING(REGEX REPLACE "^v[0-9]+\\.([0-9]+).*" "\\1" MINOR "${FULL}")
    else()
      message(STATUS "Did not find any tag")
      get_version_from_file(MAJOR MINOR PATCH)
    endif()
  endif()
  set(${VERSION_MAJOR} ${MAJOR} PARENT_SCOPE)
  set(${VERSION_MINOR} ${MINOR} PARENT_SCOPE)
  set(${VERSION_PATCH} ${PATCH} PARENT_SCOPE)
endfunction()

function(set_version VERSION)
  get_filename_component(GIT_DIR ".git" ABSOLUTE)
  if(EXISTS ${GIT_DIR})
    get_version_from_git(MAJOR MINOR PATCH)
  else()
    get_version_from_file(MAJOR MINOR PATCH)
  endif()
  set(${VERSION} "${MAJOR}.${MINOR}.${PATCH}" PARENT_SCOPE)
endfunction()

function(check_target my_target)
  if(NOT TARGET ${my_target})
    message(FATAL_ERROR " Or-Tools: compiling Or-Tools requires a ${my_target}
    CMake target in your project, see CMake/README.md for more details")
  endif(NOT TARGET ${my_target})
endfunction()
