
#[[find_package(Java REQUIRED)
if(JAVA_FOUND)
  message("java found, Ok! "${Java_JAVA_EXECUTABLE})
else()
  message("java not found!")
endif()

find_program(MONO_EXECUTABLE mono)
find_program(MCS_EXECUTABLE mcs)
if(MONO_EXECUTABLE AND MCS_EXECUTABLE)
  set(MONO_FOUND TRUE CACHE INTERNAL "")
  message("mono and mcs found, Ok! (" ${MONO_EXECUTABLE} ";" ${MCS_EXECUTABLE} ")")
endif()
if(NOT MONO_FOUND)
  message(FATAL_ERROR "Could not find mono or mcs!")
endif()
]]

