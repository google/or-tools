include(CMakeParseArguments)

#Add a prefix and a suffix to a list of elements
function(PREFIX_SUFFIX)
    cmake_parse_arguments(
        PARSED # prefix of output variables
        ""
        "PREFIX;SUFFIX;RESULT" # list of names of mono-valued arguments
        "LIST" # list of names of multi-valued arguments (output variables are lists)
        ${ARGN} # arguments of the function to parse, here we take the all original ones
    )
    set(${PARSED_RESULT} "")
    foreach(val ${PARSED_LIST})
    	list(APPEND ${PARSED_RESULT} "${PARSED_PREFIX}${val}${PARSED_SUFFIX}")
    endforeach()
    set(${PARSED_RESULT} ${${PARSED_RESULT}} PARENT_SCOPE)
endfunction()

#Defines the Java classpath separator 
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  set(CPSEP ;)
else()
  set(CPSEP :)
endif()

#extracts the package, if exists, from a java file, otherwise it returns an empty string
function(EXTRACT_PACKAGE_FROM_JAVA_FILE pack sub)
  if(NOT ARGN)
    message(SEND_ERROR "EXTRACT_PACKAGE_FROM_JAVA_FILE() called without any java file!")
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

#For examples targets (rjava, rpy...), it checks if an example file was provided, and if it exists.
function(CHECK_TARGET_FILE)
  if("${ARGN}" STREQUAL "")
    message(FATAL_ERROR " No file was specified. Retry with the following command : make ${target} EX=path/to/example")
  endif()

  set(fil ${OR_TOOLS_ROOT_DIR}/${ARGN})
  if(NOT EXISTS ${fil})
    message(FATAL_ERROR "Error: ${ARGN} was not found")
      return()
  endif()
endfunction()