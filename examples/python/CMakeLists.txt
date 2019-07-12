if (NOT BUILD_PYTHON)
	return()
endif()

project(ortools_examples)

if (APPLE)
	set(CMAKE_INSTALL_RPATH
		"@loader_path/../..;@loader_path/../lib;@loader_path")
else()
	set(CMAKE_INSTALL_RPATH "$ORIGIN/../../:$ORIGIN/../lib:$ORIGIN/")
endif()

get_filename_component(PARENT_SOURCE_DIR ${PROJECT_SOURCE_DIR} DIRECTORY)

# Look for python module virtualenv
search_python_module(virtualenv)
# Testing using a vitual environment
set(VENV_EXECUTABLE ${PYTHON_EXECUTABLE} -m virtualenv)
set(VENV_DIR ${CMAKE_BINARY_DIR}/venv)
if(WIN32)
set(VENV_BIN_DIR "${VENV_DIR}\\Scripts")
else()
set(VENV_BIN_DIR ${VENV_DIR}/bin)
endif()
# run the tests within the virtualenv


foreach(TEST
    linear_programming
    )
  add_test(NAME py_${TEST} COMMAND ${VENV_BIN_DIR}/python ${PROJECT_SOURCE_DIR}/${TEST}.py)
endforeach()
