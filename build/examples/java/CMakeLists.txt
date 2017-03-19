file(MAKE_DIRECTORY ${CLASS_DIR})

file (GLOB_RECURSE java_files "${OR_TOOLS_ROOT_DIR}/*.java")
file (GLOB_RECURSE java_gen_files "${GEN_DIR}/*.java")
file (GLOB_RECURSE java_dependencies_files "${OR_TOOLS_ROOT_DIR}/dependencies/*.java")
list(REMOVE_ITEM java_files ${java_gen_files} ${java_dependencies_files})

foreach(fil ${java_files})
  EXTRACT_PACKAGE_FROM_JAVA_FILE(package sub_path ${fil})  
  get_filename_component(file_we ${fil} NAME_WE)
  set(class_file ${CLASS_DIR}/${sub_path}${file_we}.class)
  string(REPLACE "/" "_" _class_file ${class_file})

  add_custom_command(
    OUTPUT ${class_file}
    PRE_LINK
    COMMAND ${Java_JAVAC_EXECUTABLE} -d ${CLASS_DIR} -cp ${LIB_DIR}/com.google.ortools.jar${CPSEP}${LIB_DIR}/protobuf.jar ${fil}
    DEPENDS ortoolsJNI ${fil}
    COMMENT "generating ${class_file}"
  )
  add_custom_target(
    ${_class_file}
    DEPENDS ${class_file}
    VERBATIM
  )
endforeach()

add_custom_target(
  rjava
  COMMAND ${CMAKE_COMMAND} -Dtarget=rjava -Dex_file=$(EX) -Dargs=$(ARGS) -DOR_TOOLS_ROOT_DIR=${OR_TOOLS_ROOT_DIR} -DLIB_DIR=${LIB_DIR} -DCLASS_DIR=${CLASS_DIR} -P rjava.cmake
  DEPENDS java
)