if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  set(CPSEP ;)
else()
  set(CPSEP :)
endif()

function(EXTRACT_PACKAGE_FROM_JAVA_FILE pack sub)
  if(NOT ARGN)
    message(SEND_ERROR "Error: EXTRACT_PACKAGE_FROM_JAVA_FILE() called without any java file")
    return()
  endif()
  set(${pack} "")
  set(${sub} "")

  file(READ ${ARGN} file_content)
  string(REGEX MATCH "package[^\n]*" package_line ${file_content}) #Match the line containing package
  string(REPLACE "package " "" package_line1 ${package_line}) #Remove the keyword package
  if("${package_line1}" STREQUAL "")
    set(${pack} "")
    set(${sub} "")
  else()  
    string(REPLACE ";" "" _package ${package_line1}) #Remove the semicolon
    string(REPLACE "." "/" _sub_path ${_package})
    set(${pack} ${_package}.)
    set(${sub} ${_sub_path}/)
  endif()
  set(${pack} ${${pack}} PARENT_SCOPE)
  set(${sub} ${${sub}} PARENT_SCOPE)
endfunction()