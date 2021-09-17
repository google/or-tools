if(NOT BUILD_EXAMPLES)
  return()
endif()

if(BUILD_CXX_EXAMPLES)
  file(GLOB CXX_SRCS "*.cc")
  foreach(FILE_NAME IN LISTS CXX_SRCS)
    add_cxx_example(${FILE_NAME})
  endforeach()
endif()

if(BUILD_PYTHON_EXAMPLES)
  file(GLOB PYTHON_SRCS "*.py")
  # Remove too long examples, or not working
  # ISSUE: Seems to works using Makefile based build
  # ???: not tested even in Makefile based build (i.e. unmaintained)
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/bacp.py") # need input
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/nontransitive_dice.py") # too long
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/secret_santa.py") # too long
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/steel.py") # need input
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/steel_lns.py") # need input
  # Not working everywhere since it rely on /usr/share/dict/words
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/word_square.py")

  #list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/project_scheduling_sat.py") # ISSUE
  #list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/sports_schedule_sat.py") # ISSUE
  foreach(FILE_NAME IN LISTS PYTHON_SRCS)
    add_python_example(${FILE_NAME})
  endforeach()
endif()

if(BUILD_JAVA_EXAMPLES)
  file(GLOB JAVA_SRCS "*.java")
  foreach(FILE_NAME IN LISTS JAVA_SRCS)
    add_java_example(${FILE_NAME})
  endforeach()
endif()

if(BUILD_DOTNET_EXAMPLES)
  file(GLOB DOTNET_SRCS "*.cs")
  list(FILTER DOTNET_SRCS EXCLUDE REGEX ".*/coins_grid.cs") # too long
  list(FILTER DOTNET_SRCS EXCLUDE REGEX ".*/nontransitive_dice.cs") # too long
  list(FILTER DOTNET_SRCS EXCLUDE REGEX ".*/partition.cs") # too long
  # Not working everywhere since it rely on /usr/share/dict/words
  list(FILTER DOTNET_SRCS EXCLUDE REGEX ".*/word_square.cs")
  foreach(FILE_NAME IN LISTS DOTNET_SRCS)
    add_dotnet_example(${FILE_NAME})
  endforeach()
endif()
