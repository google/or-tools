if(NOT BUILD_EXAMPLES)
	return()
endif()

if(BUILD_CXX_EXAMPLES)

endif()

if(BUILD_PYTHON_EXAMPLES)
  foreach(TEST IN ITEMS
      assignment
      costas_array
      hidato
      knapsack_cp
      knapsack_mip
      )
    add_test(py${TEST}_venv ${VENV_BIN_DIR}/python ${CMAKE_CURRENT_SOURCE_DIR}/${TEST}.py)
    set_tests_properties(py${TEST}_venv PROPERTIES DEPENDS build_venv)
  endforeach()

  install(DIRECTORY .
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ortools/examples/python
    FILES_MATCHING PATTERN "*.py")
endif()

if(BUILD_JAVA_EXAMPLES)

endif()

if(BUILD_DOTNET_EXAMPLES)

endif()
