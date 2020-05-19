if(NOT BUILD_PYTHON)
  return()
endif()

# Use latest UseSWIG module
cmake_minimum_required(VERSION 3.14)

if(NOT TARGET ortools::ortools)
  message(FATAL_ERROR "Python: missing ortools TARGET")
endif()

# Will need swig
set(CMAKE_SWIG_FLAGS)
find_package(SWIG REQUIRED)
include(UseSWIG)

if(${SWIG_VERSION} VERSION_GREATER_EQUAL 4)
  list(APPEND CMAKE_SWIG_FLAGS "-doxygen")
endif()

if(UNIX AND NOT APPLE)
  list(APPEND CMAKE_SWIG_FLAGS "-DSWIGWORDSIZE64")
endif()

# Find Python
find_package(Python REQUIRED COMPONENTS Interpreter Development)

if(Python_VERSION VERSION_GREATER_EQUAL 3)
  list(APPEND CMAKE_SWIG_FLAGS "-py3")
endif()

# Generate Protobuf py sources
set(PROTO_PYS)
file(GLOB_RECURSE proto_py_files RELATIVE ${PROJECT_SOURCE_DIR}
  "ortools/constraint_solver/*.proto"
  "ortools/linear_solver/*.proto"
  "ortools/sat/*.proto"
  "ortools/util/*.proto"
  )
list(REMOVE_ITEM proto_py_files "ortools/constraint_solver/demon_profiler.proto")
foreach(PROTO_FILE ${proto_py_files})
  #message(STATUS "protoc proto(py): ${PROTO_FILE}")
  get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(PROTO_PY ${PROJECT_BINARY_DIR}/python/${PROTO_DIR}/${PROTO_NAME}_pb2.py)
  #message(STATUS "protoc py: ${PROTO_PY}")
  add_custom_command(
    OUTPUT ${PROTO_PY}
    COMMAND protobuf::protoc
    "--proto_path=${PROJECT_SOURCE_DIR}"
    "--python_out=${PROJECT_BINARY_DIR}/python"
    ${PROTO_FILE}
    DEPENDS ${PROTO_FILE} protobuf::protoc
    COMMENT "Generate Python protocol buffer for ${PROTO_FILE}"
    VERBATIM)
  list(APPEND PROTO_PYS ${PROTO_PY})
endforeach()
add_custom_target(Py${PROJECT_NAME}_proto DEPENDS ${PROTO_PYS} ortools::ortools)

# CMake will remove all '-D' prefix (i.e. -DUSE_FOO become USE_FOO)
#get_target_property(FLAGS ortools::ortools COMPILE_DEFINITIONS)
set(FLAGS -DUSE_BOP -DUSE_GLOP -DABSL_MUST_USE_RESULT)
if(USE_COINOR)
  list(APPEND FLAGS
    "-DUSE_CBC"
    "-DUSE_CLP"
    )
endif()
list(APPEND CMAKE_SWIG_FLAGS ${FLAGS} "-I${PROJECT_SOURCE_DIR}")

foreach(SUBPROJECT IN ITEMS algorithms graph linear_solver constraint_solver sat data util)
  add_subdirectory(ortools/${SUBPROJECT}/python)
endforeach()

#######################
## Python Packaging  ##
#######################
#file(MAKE_DIRECTORY python/${PROJECT_NAME})
file(COPY ortools/__init__.py DESTINATION python/${PROJECT_NAME})
file(COPY ortools/__init__.py DESTINATION python/${PROJECT_NAME}/util)
file(COPY ortools/__init__.py DESTINATION python/${PROJECT_NAME}/constraint_solver)
file(COPY ortools/__init__.py DESTINATION python/${PROJECT_NAME}/linear_solver)
file(COPY ortools/__init__.py DESTINATION python/${PROJECT_NAME}/sat)
file(COPY ortools/__init__.py DESTINATION python/${PROJECT_NAME}/sat/python)
file(COPY ortools/__init__.py DESTINATION python/${PROJECT_NAME}/graph)
file(COPY ortools/__init__.py DESTINATION python/${PROJECT_NAME}/algorithms)
file(COPY ortools/__init__.py DESTINATION python/${PROJECT_NAME}/data)

file(COPY ortools/linear_solver/linear_solver_natural_api.py
  DESTINATION python/ortools/linear_solver)
file(COPY ortools/sat/python/cp_model.py
  DESTINATION python/ortools/sat/python)
file(COPY ortools/sat/python/visualization.py
  DESTINATION python/ortools/sat/python)

# setup.py.in contains cmake variable e.g. @PROJECT_NAME@ and
# generator expression e.g. $<TARGET_FILE_NAME:pyFoo>
configure_file(
	ortools/python/setup.py.in
	${CMAKE_CURRENT_BINARY_DIR}/python/setup.py.in
	@ONLY)
file(GENERATE
	OUTPUT python/$<CONFIG>/setup.py
	INPUT ${CMAKE_CURRENT_BINARY_DIR}/python/setup.py.in)

# Find if python module MODULE_NAME is available,
# if not install it to the Python user install directory.
function(search_python_module MODULE_NAME)
  execute_process(
    COMMAND ${Python_EXECUTABLE} -c "import ${MODULE_NAME}; print(${MODULE_NAME}.__version__)"
    RESULT_VARIABLE _RESULT
    OUTPUT_VARIABLE MODULE_VERSION
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  if(${_RESULT} STREQUAL "0")
    message(STATUS "Found python module: ${MODULE_NAME} (found version \"${MODULE_VERSION}\")")
  else()
    message(WARNING "Can't find python module \"${MODULE_NAME}\", install it using pip...")
    execute_process(
      COMMAND ${Python_EXECUTABLE} -m pip install --user ${MODULE_NAME}
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
  endif()
endfunction()

# Look for python module wheel
search_python_module(setuptools)
search_python_module(wheel)

# Main Target
add_custom_target(python_package ALL
	COMMAND ${CMAKE_COMMAND} -E copy $<CONFIG>/setup.py setup.py
  COMMAND ${CMAKE_COMMAND} -E remove_directory dist
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_NAME}/.libs
  #COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ortools> ${PROJECT_NAME}/.libs
  COMMAND ${CMAKE_COMMAND} -E copy $<$<NOT:$<PLATFORM_ID:Windows>>:$<TARGET_SONAME_FILE:ortools>> ${PROJECT_NAME}/.libs
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywrapknapsack_solver> ${PROJECT_NAME}/algorithms
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywrapgraph> ${PROJECT_NAME}/graph
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywrapcp> ${PROJECT_NAME}/constraint_solver
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywraplp> ${PROJECT_NAME}/linear_solver
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywrapsat> ${PROJECT_NAME}/sat
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywraprcpsp> ${PROJECT_NAME}/data
  #COMMAND ${PYTHON_EXECUTABLE} setup.py bdist_egg bdist_wheel
  COMMAND ${PYTHON_EXECUTABLE} setup.py bdist_wheel
  BYPRODUCTS
    python/${PROJECT_NAME}
    python/build
    python/dist
    python/${PROJECT_NAME}.egg-info
  WORKING_DIRECTORY python
  )
add_dependencies(python_package ortools::ortools Py${PROJECT_NAME}_proto)

# Test
if(BUILD_TESTING)
  # Look for python module virtualenv
  search_python_module(virtualenv)
  # Testing using a vitual environment
  set(VENV_EXECUTABLE ${Python_EXECUTABLE} -m virtualenv)
  set(VENV_DIR ${CMAKE_BINARY_DIR}/venv)
  if(WIN32)
    set(VENV_Python_EXECUTABLE "${VENV_DIR}\\Scripts\\python.exe")
  else()
    set(VENV_Python_EXECUTABLE ${VENV_DIR}/bin/python)
  endif()
  # make a virtualenv to install our python package in it
  add_custom_command(TARGET python_package POST_BUILD
		COMMAND ${VENV_EXECUTABLE} -p ${Python_EXECUTABLE} ${VENV_DIR}
    # Must not call it in a folder containing the setup.py otherwise pip call it
    # (i.e. "python setup.py bdist") while we want to consume the wheel package
    COMMAND ${VENV_Python_EXECUTABLE} -m pip install --find-links=${CMAKE_CURRENT_BINARY_DIR}/python/dist ${PROJECT_NAME}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/test.py.in ${VENV_DIR}/test.py
    BYPRODUCTS ${VENV_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} VERBATIM)
  # run the tests within the virtualenv
	add_test(NAME pytest_venv
		COMMAND ${VENV_Python_EXECUTABLE} ${VENV_DIR}/test.py)

  #add_subdirectory(examples/python)
  #add_subdirectory(examples/notebook)
endif()
