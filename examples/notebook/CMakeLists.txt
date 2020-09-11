if(NOT BUILD_PYTHON_EXAMPLES)
  return()
endif()

install(DIRECTORY .
  DESTINATION ${CMAKE_INSTALL_DATADIR}/ortools/examples/notebook
  FILES_MATCHING PATTERN "*.ipynb")
