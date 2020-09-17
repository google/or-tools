if(NOT BUILD_JAVA_EXAMPLES)
	return()
endif()

file(GLOB JAVA_SRCS "*.java")
foreach(SAMPLE IN LISTS JAVA_SRCS)
  add_java_example(${SAMPLE})
endforeach()
