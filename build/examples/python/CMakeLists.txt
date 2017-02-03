#TODO(make the python version configurable)
find_package( PythonInterp 2.7 REQUIRED )

set(PYTHONPATH ${SRC_DIR}:${PROTO_PYTHON_DIR})

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  set(SET_PYTHONPATH @set PYTHONPATH=${PYTHONPATH} &&)
else()
  set(SET_PYTHONPATH PYTHONPATH=${PYTHONPATH})
endif()

add_custom_target(
  rpy
  COMMAND ${CMAKE_COMMAND} -Dtarget=rpy -Dex_file=$(EX) -DOR_TOOLS_ROOT_DIR=${OR_TOOLS_ROOT_DIR} -P rpy.cmake
  COMMAND ${SET_PYTHONPATH} ${PYTHON_EXECUTABLE} ${OR_TOOLS_ROOT_DIR}/$(EX) $(ARGS)
  DEPENDS python
)