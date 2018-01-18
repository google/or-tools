if (NOT BUILD_PYTHON)
  return()
endif()

install(DIRECTORY .
	DESTINATION ${CMAKE_INSTALL_DATADIR}/ortools/examples/data)
