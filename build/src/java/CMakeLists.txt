#[[find_package(Java REQUIRED)
if(JAVA_FOUND)
  message("java found, Ok! "${Java_JAVA_EXECUTABLE})
else()
  message("java not found!")
endif()
]]


include(ortools_protobuf.cmake)
include(ortools_SWIG.cmake)
include(UseJava)

find_package(Java REQUIRED COMPONENTS Runtime Development)

file(GLOB_RECURSE java_sources "${GEN_DIR}/*.java")
list(APPEND java_sources ${SRC_DIR}/com/google/ortools/constraintsolver/JavaDecisionBuilder.java)
list(APPEND java_sources ${java_gen_protobuf})
list(REMOVE_DUPLICATES java_sources) #Because protobuf and SWIG java files are generated under the same directory


set(CMAKE_JAVA_INCLUDE_PATH ${SRC_DIR} ${GEN_DIR})
add_jar(
    ortoolsJNI
    SOURCES ${java_sources}
    INCLUDE_JARS ${LIB_DIR}/protobuf.jar
    OUTPUT_DIR ${LIB_DIR}
    OUTPUT_NAME com.google.ortools
)

add_dependencies(ortoolsJNI jniortools)

add_custom_target(
  java
  DEPENDS ortoolsJNI
)
