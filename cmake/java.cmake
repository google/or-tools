if(NOT BUILD_JAVA)
  return()
endif()

find_package(SWIG REQUIRED)
find_package(JAVA REQUIRED)
find_package(JNI REQUIRED)

if(NOT TARGET ortools::ortools)
  message(FATAL_ERROR "Java: missing ortools TARGET")
endif()

if(BUILD_TESTING)
  add_subdirectory(examples/java)
endif()
