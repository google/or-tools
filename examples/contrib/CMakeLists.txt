if(NOT BUILD_EXAMPLES)
	return()
endif()

if(BUILD_CXX_EXAMPLES)

endif()

if(BUILD_PYTHON_EXAMPLES)
  file(GLOB PYTHON_SRCS "*.py")
  # Remove too long examples, or not working
  # ISSUE: Seems to works using Makefile based build
  # ???: not tested even in Makefile based build (i.e. unmaintained)
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/bacp.py") # need input
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/diet1.py") # ISSUE
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/magic_sequence_sat.py") # ISSUE
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/nontransitive_dice.py") # too long
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/project_scheduling_sat.py") # ???
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/rostering_with_travel.py") # ISSUE
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/scheduling_with_transitions_sat.py") # ???
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/school_scheduling_sat.py") # too long
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/secret_santa.py") # too long
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/sports_schedule_sat.py") # ???
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/stable_marriage.py") # ISSUE
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/stable_marriage_sat.py") # ???
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/steel.py") # ISSUE
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/steel_lns.py") # ISSUE
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/wedding_optimal_chart.py") # ISSUE
  # Not working everywhere since it rely on /usr/share/dict/words
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/word_square.py")
  foreach(FILE_NAME IN LISTS PYTHON_SRCS)
    message(STATUS "Add python example: ${FILE_NAME}")
    get_filename_component(NAME ${FILE_NAME} NAME_WE)
    add_test(python_${NAME} ${VENV_Python_EXECUTABLE} ${FILE_NAME})
  endforeach()

  install(DIRECTORY .
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ortools/examples/python
    FILES_MATCHING PATTERN "*.py")
endif()

if(BUILD_JAVA_EXAMPLES)
  file(GLOB JAVA_SRCS "*.java")
  foreach(SAMPLE IN LISTS JAVA_SRCS)
    add_java_example(${SAMPLE})
  endforeach()
endif()

if(BUILD_DOTNET_EXAMPLES)

endif()
