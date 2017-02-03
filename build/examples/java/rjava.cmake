include(${OR_TOOLS_ROOT_DIR}/build/utils.cmake)

CHECK_TARGET_FILE(${ex_file})

set(fil ${OR_TOOLS_ROOT_DIR}/${ex_file})

EXTRACT_PACKAGE_FROM_JAVA_FILE(package sub_path ${fil})

get_filename_component(file_we ${fil} NAME_WE)
set(class_file ${CLASS_DIR}/${sub_path}${file_we}.class)
string(REPLACE "/" "_" _class_file ${class_file})

execute_process(
  COMMAND make ${_class_file}
  WORKING_DIRECTORY ${OR_TOOLS_ROOT_DIR}/build
  ERROR_VARIABLE err1
)
if(NOT "${err1}" STREQUAL "")
  message(FATAL_ERROR "${err1}")
endif()

#TODO(Java_JAVAC_EXECUTABLE)
execute_process(
  COMMAND java -Djava.library.path=${LIB_DIR} -cp ${CLASS_DIR}${CPSEP}${LIB_DIR}/com.google.ortools.jar${CPSEP}${LIB_DIR}/protobuf.jar ${package}${file_we} ${args}
  WORKING_DIRECTORY ${OR_TOOLS_ROOT_DIR}/build
  ERROR_VARIABLE err2
)
if(NOT "${err2}" STREQUAL "")
  message(FATAL_ERROR "${err2}")
endif()
