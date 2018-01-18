if (NOT BUILD_PYTHON)
  return()
endif()

foreach(TEST
		hidato_table
		integer_programming
		knapsack
		linear_programming
		pyflow_example
		tsp
		)
	add_test(py${TEST}_venv ${VENV_BIN_DIR}/python ${CMAKE_CURRENT_SOURCE_DIR}/${TEST}.py)
	set_tests_properties(py${TEST}_venv PROPERTIES DEPENDS build_venv)
endforeach()

include(GNUInstallDirs)
install(DIRECTORY .
	DESTINATION ${CMAKE_INSTALL_DATADIR}/ortools/examples/python
	FILES_MATCHING PATTERN "*.py")
