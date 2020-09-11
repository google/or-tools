if (NOT BUILD_EXAMPLES)
	return()
endif()

if(BUILD_CXX_EXAMPLES)

endif()

if(BUILD_PYTHON)
  foreach(TEST IN ITEMS
      hidato_table
      integer_programming
      knapsack
      linear_programming
      pyflow_example
      tsp
      vrp
      vrpgs
      cvrp
      cvrptw)
    add_test(py${TEST}_venv ${VENV_BIN_DIR}/python ${CMAKE_CURRENT_SOURCE_DIR}/${TEST}.py)
    set_tests_properties(py${TEST}_venv PROPERTIES DEPENDS build_venv)
  endforeach()

  include(GNUInstallDirs)
  install(DIRECTORY .
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ortools/examples/python
    FILES_MATCHING PATTERN "*.py")
endif()

if(BUILD_JAVA_EXAMPLES)

endif()

if(BUILD_DOTNET_EXAMPLES)

endif()
