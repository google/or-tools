if(NOT BUILD_PYTHON)
  return()
endif()

# Use latest UseSWIG module (3.14) and Python3 module (3.18)
cmake_minimum_required(VERSION 3.18)

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

# Find Python 3
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
list(APPEND CMAKE_SWIG_FLAGS "-py3" "-DPY3")

# Find if python module MODULE_NAME is available,
# if not install it to the Python 3 user install directory.
function(search_python_module MODULE_NAME)
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import ${MODULE_NAME}; print(${MODULE_NAME}.__version__)"
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
      COMMAND ${Python3_EXECUTABLE} -m pip install --user ${MODULE_NAME}
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
  endif()
endfunction()

# Find if python builtin module MODULE_NAME is available.
function(search_python_internal_module MODULE_NAME)
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import ${MODULE_NAME}"
    RESULT_VARIABLE _RESULT
    OUTPUT_VARIABLE MODULE_VERSION
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  if(${_RESULT} STREQUAL "0")
    message(STATUS "Found python internal module: ${MODULE_NAME}")
  else()
    message(FATAL_ERROR "Can't find python internal module \"${MODULE_NAME}\", please install it using your system package manger")
  endif()
endfunction()

# Generate Protobuf py sources with mypy support
search_python_module(mypy-protobuf)
set(PROTO_PYS)
file(GLOB_RECURSE proto_py_files RELATIVE ${PROJECT_SOURCE_DIR}
  "ortools/constraint_solver/*.proto"
  "ortools/linear_solver/*.proto"
  "ortools/packing/*.proto"
  "ortools/sat/*.proto"
  "ortools/util/*.proto"
  "ortools/scheduling/*.proto"
  )
list(REMOVE_ITEM proto_py_files "ortools/constraint_solver/demon_profiler.proto")
foreach(PROTO_FILE IN LISTS proto_py_files)
  #message(STATUS "protoc proto(py): ${PROTO_FILE}")
  get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  set(PROTO_PY ${PROJECT_BINARY_DIR}/python/${PROTO_DIR}/${PROTO_NAME}_pb2.py)
  #message(STATUS "protoc py: ${PROTO_PY}")
  add_custom_command(
    OUTPUT ${PROTO_PY}
    COMMAND ${PROTOC_PRG}
    "--proto_path=${PROJECT_SOURCE_DIR}"
    ${PROTO_DIRS}
    "--python_out=${PROJECT_BINARY_DIR}/python"
    "--mypy_out=${PROJECT_BINARY_DIR}/python"
    ${PROTO_FILE}
    DEPENDS ${PROTO_FILE} ${PROTOC_PRG}
    COMMENT "Generate Python 3 protocol buffer for ${PROTO_FILE}"
    VERBATIM)
  list(APPEND PROTO_PYS ${PROTO_PY})
endforeach()
add_custom_target(Py${PROJECT_NAME}_proto DEPENDS ${PROTO_PYS} ortools::ortools)

# CMake will remove all '-D' prefix (i.e. -DUSE_FOO become USE_FOO)
#get_target_property(FLAGS ortools::ortools COMPILE_DEFINITIONS)
set(FLAGS -DUSE_BOP -DUSE_GLOP -DABSL_MUST_USE_RESULT)
if(USE_SCIP)
  list(APPEND FLAGS "-DUSE_SCIP")
endif()
if(USE_COINOR)
  list(APPEND FLAGS "-DUSE_CBC" "-DUSE_CLP")
endif()
list(APPEND CMAKE_SWIG_FLAGS ${FLAGS} "-I${PROJECT_SOURCE_DIR}")

foreach(SUBPROJECT IN ITEMS init algorithms graph linear_solver constraint_solver sat scheduling util)
  add_subdirectory(ortools/${SUBPROJECT}/python)
endforeach()

#######################
## Python Packaging  ##
#######################
set(PYTHON_PROJECT ${PROJECT_NAME})
message(STATUS "Python project: ${PYTHON_PROJECT}")
set(PYTHON_PATH ${PROJECT_BINARY_DIR}/python/${PYTHON_PROJECT})
message(STATUS "Python project build path: ${PYTHON_PATH}")

#file(MAKE_DIRECTORY python/${PROJECT_NAME})
file(COPY ortools/__init__.py DESTINATION ${PYTHON_PATH})
file(COPY ortools/__init__.py DESTINATION ${PYTHON_PATH}/init)
file(COPY ortools/__init__.py DESTINATION ${PYTHON_PATH}/algorithms)
file(COPY ortools/__init__.py DESTINATION ${PYTHON_PATH}/graph)
file(COPY ortools/__init__.py DESTINATION ${PYTHON_PATH}/linear_solver)
file(COPY ortools/__init__.py DESTINATION ${PYTHON_PATH}/constraint_solver)
file(COPY ortools/__init__.py DESTINATION ${PYTHON_PATH}/packing)
file(COPY ortools/__init__.py DESTINATION ${PYTHON_PATH}/sat)
file(COPY ortools/__init__.py DESTINATION ${PYTHON_PATH}/sat/python)
file(COPY ortools/__init__.py DESTINATION ${PYTHON_PATH}/scheduling)
file(COPY ortools/__init__.py DESTINATION ${PYTHON_PATH}/util)

file(COPY
    ortools/linear_solver/linear_solver_natural_api.py
    DESTINATION ${PYTHON_PATH}/linear_solver)
file(COPY
    ortools/sat/python/cp_model.py
    ortools/sat/python/cp_model_helper.py
    ortools/sat/python/visualization.py
  DESTINATION ${PYTHON_PATH}/sat/python)

# setup.py.in contains cmake variable e.g. @PROJECT_NAME@ and
# generator expression e.g. $<TARGET_FILE_NAME:pyFoo>
configure_file(
  ${PROJECT_SOURCE_DIR}/ortools/python/setup.py.in
  ${PROJECT_BINARY_DIR}/python/setup.py.in
  @ONLY)
file(GENERATE
  OUTPUT ${PROJECT_BINARY_DIR}/python/$<CONFIG>/setup.py
  INPUT ${PROJECT_BINARY_DIR}/python/setup.py.in)

configure_file(
  ${PROJECT_SOURCE_DIR}/tools/README.pypi.txt
  ${PROJECT_BINARY_DIR}/python/README.txt
  COPYONLY)

# Look for python module wheel
search_python_module(setuptools)
search_python_module(wheel)

# Main Target
add_custom_target(python_package ALL
  COMMAND ${CMAKE_COMMAND} -E copy ./$<CONFIG>/setup.py setup.py
  COMMAND ${CMAKE_COMMAND} -E remove_directory dist
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PYTHON_PROJECT}/.libs
  # Don't need to copy static lib on windows.
  COMMAND ${CMAKE_COMMAND} -E $<IF:$<STREQUAL:$<TARGET_PROPERTY:${PYTHON_PROJECT},TYPE>,SHARED_LIBRARY>,copy,true>
  $<$<STREQUAL:$<TARGET_PROPERTY:${PYTHON_PROJECT},TYPE>,SHARED_LIBRARY>:$<TARGET_SONAME_FILE:${PYTHON_PROJECT}>>
  ${PYTHON_PROJECT}/.libs
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywrapinit> ${PYTHON_PROJECT}/init
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywrapknapsack_solver> ${PYTHON_PROJECT}/algorithms
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywrapgraph> ${PYTHON_PROJECT}/graph
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywrapcp> ${PYTHON_PROJECT}/constraint_solver
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywraplp> ${PYTHON_PROJECT}/linear_solver
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywrapsat> ${PYTHON_PROJECT}/sat
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywraprcpsp> ${PYTHON_PROJECT}/scheduling
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sorted_interval_list> ${PYTHON_PROJECT}/util
  #COMMAND ${Python3_EXECUTABLE} setup.py bdist_egg bdist_wheel
  COMMAND ${Python3_EXECUTABLE} setup.py bdist_wheel
  BYPRODUCTS
    python/${PYTHON_PROJECT}
    python/build
    python/dist
    python/${PYTHON_PROJECT}.egg-info
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/python
  COMMAND_EXPAND_LISTS
  )
add_dependencies(python_package ortools::ortools Py${PROJECT_NAME}_proto)

# Install rules
configure_file(
  ${PROJECT_SOURCE_DIR}/cmake/python-install.cmake.in
  ${PROJECT_BINARY_DIR}/python/python-install.cmake
  @ONLY)
install(SCRIPT ${PROJECT_BINARY_DIR}/python/python-install.cmake)

# Test
if(BUILD_TESTING)
  # Look for python module venv
  search_python_module(absl-py)
  search_python_internal_module(venv)
  # Testing using a vitual environment
  set(VENV_EXECUTABLE ${Python3_EXECUTABLE} -m venv)
  set(VENV_DIR ${PROJECT_BINARY_DIR}/python/venv)
  if(WIN32)
    set(VENV_Python3_EXECUTABLE "${VENV_DIR}\\Scripts\\python.exe")
  else()
    set(VENV_Python3_EXECUTABLE ${VENV_DIR}/bin/python)
  endif()
  # make a virtualenv to install our python package in it
  add_custom_command(TARGET python_package POST_BUILD
    COMMAND ${VENV_EXECUTABLE} ${VENV_DIR}
    # Must not call it in a folder containing the setup.py otherwise pip call it
    # (i.e. "python setup.py bdist") while we want to consume the wheel package
    COMMAND ${VENV_Python3_EXECUTABLE} -m pip install --find-links=${CMAKE_CURRENT_BINARY_DIR}/python/dist ${PROJECT_NAME}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/test.py.in ${VENV_DIR}/test.py
    BYPRODUCTS ${VENV_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} VERBATIM)
  # run the tests within the virtualenv
  add_test(NAME pytest_venv
    COMMAND ${VENV_Python3_EXECUTABLE} ${VENV_DIR}/test.py)
endif()

# add_python_sample()
# CMake function to generate and build python sample.
# Parameters:
#  the python filename
# e.g.:
# add_python_sample(foo.py)
function(add_python_sample FILE_NAME)
  message(STATUS "Configuring sample ${FILE_NAME} ...")
  get_filename_component(SAMPLE_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(SAMPLE_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_DIR ${SAMPLE_DIR} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  if(BUILD_TESTING)
    add_test(
      NAME python_${COMPONENT_NAME}_${SAMPLE_NAME}
      COMMAND ${VENV_Python3_EXECUTABLE} ${FILE_NAME}
      WORKING_DIRECTORY ${VENV_DIR})
  endif()
  message(STATUS "Configuring sample ${FILE_NAME} done")
endfunction()

# add_python_example()
# CMake function to generate and build python example.
# Parameters:
#  the python filename
# e.g.:
# add_python_example(foo.py)
function(add_python_example FILE_NAME)
  message(STATUS "Configuring example ${FILE_NAME} ...")
  get_filename_component(EXAMPLE_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(COMPONENT_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  if(BUILD_TESTING)
    add_test(
      NAME python_${COMPONENT_NAME}_${EXAMPLE_NAME}
      COMMAND ${VENV_Python3_EXECUTABLE} ${FILE_NAME}
      WORKING_DIRECTORY ${VENV_DIR})
  endif()
  message(STATUS "Configuring example ${FILE_NAME} done")
endfunction()

# add_python_test()
# CMake function to generate and build python test.
# Parameters:
#  the python filename
# e.g.:
# add_python_test(foo.py)
function(add_python_test FILE_NAME)
  message(STATUS "Configuring test ${FILE_NAME} ...")
  get_filename_component(TEST_NAME ${FILE_NAME} NAME_WE)
  get_filename_component(COMPONENT_DIR ${FILE_NAME} DIRECTORY)
  get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)

  if(BUILD_TESTING)
    add_test(
      NAME python_${COMPONENT_NAME}_${TEST_NAME}
      COMMAND ${VENV_Python3_EXECUTABLE} ${FILE_NAME}
      WORKING_DIRECTORY ${VENV_DIR})
  endif()
  message(STATUS "Configuring test ${FILE_NAME} done")
endfunction()
