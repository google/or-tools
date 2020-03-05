if(NOT BUILD_PYTHON)
  return()
endif()

# Use latest UseSWIG module
cmake_minimum_required(VERSION 3.14)

if(NOT TARGET ortools::ortools)
  message(FATAL_ERROR "Python: missing ortools TARGET")
endif()

# Will need swig
find_package(SWIG REQUIRED)
include(UseSWIG)

if(${SWIG_VERSION} VERSION_GREATER_EQUAL 4)
  list(APPEND CMAKE_SWIG_FLAGS "-doxygen")
endif()

if(UNIX AND NOT APPLE)
  list(APPEND CMAKE_SWIG_FLAGS "-DSWIGWORDSIZE64")
endif()

# Setup Python
# prefer Python 3.8 over 3.7 over ...
# user can overwrite it e.g.:
# cmake -S. -Bbuild -DBUILD_PYTHON=ON -DPython_ADDITIONAL_VERSIONS="3.9"
set(Python_ADDITIONAL_VERSIONS "3.8;3.7;3.6;3.5" CACHE STRING "Python to use for binding")
find_package(PythonInterp REQUIRED)
message(STATUS "Found Python: ${PYTHON_EXECUTABLE} (found version \"${PYTHON_VERSION_STRING}\")")

if(${PYTHON_VERSION_STRING} VERSION_GREATER_EQUAL 3)
  list(APPEND CMAKE_SWIG_FLAGS "-py3;-DPY3")
endif()

# Find Python Library
# Force PythonLibs to find the same version than the python interpreter.
set(Python_ADDITIONAL_VERSIONS "${PYTHON_VERSION_STRING}")
find_package(PythonLibs REQUIRED)
message(STATUS "Found Python Include: ${PYTHON_INCLUDE_DIRS} (found version \"${PYTHONLIBS_VERSION_STRING}\")")

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

# To use a cmake generator expression (aka $<>), it must be processed at build time
# i.e. inside a add_custom_command()
# This command will depend on TARGET(s) in cmake generator expression
add_custom_command(OUTPUT python/setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "from setuptools import dist, find_packages, setup" > setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "class BinaryDistribution(dist.Distribution):" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  def is_pure(self):" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "    return False" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  def has_ext_modules(self):" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "    return True" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "from setuptools.command.install import install" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "class InstallPlatlib(install):" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "    def finalize_options(self):" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "        install.finalize_options(self)" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "        self.install_lib=self.install_platlib" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "setup(" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  name='ortools'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  license='Apache 2.0'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  version='${PROJECT_VERSION}'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  author='Google Inc'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  author_email='lperron@google.com'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  description='Google OR-Tools python libraries and modules'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  long_description='read(README.txt)'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  keywords=('operations research' +" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  ', constraint programming' +" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  ', linear programming' +" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  ', flow algoritms' +" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  ', python')," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  url='https://developers.google.com/optimization/'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  download_url='https://github.com/google/or-tools/releases'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  distclass=BinaryDistribution," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  cmdclass={'install': InstallPlatlib}," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  packages=find_packages()," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  package_data={" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'ortools':[$<$<NOT:$<PLATFORM_ID:Windows>>:'.libs/*', '../$<TARGET_SONAME_FILE_NAME:ortools>'>]," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'ortools.constraint_solver':['$<TARGET_FILE_NAME:pywrapcp>', '*.pyi']," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'ortools.linear_solver':['$<TARGET_FILE_NAME:pywraplp>', '*.pyi']," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'ortools.sat':['$<TARGET_FILE_NAME:pywrapsat>', '*.pyi']," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'ortools.graph':['$<TARGET_FILE_NAME:pywrapgraph>']," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'ortools.algorithms':['$<TARGET_FILE_NAME:pywrapknapsack_solver>']," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'ortools.data':['$<TARGET_FILE_NAME:pywraprcpsp>', '*.pyi']," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'ortools.util':['$<TARGET_FILE_NAME:sorted_interval_list>', '*.pyi']," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  }," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  include_package_data=True," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  install_requires=[" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'protobuf >= ${Protobuf_VERSION}'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'six >= 1.10'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  ]," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  classifiers=[" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'Development Status :: 5 - Production/Stable'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'Intended Audience :: Developers'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'License :: OSI Approved :: Apache Software License'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'Operating System :: POSIX :: Linux'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'Operating System :: MacOS :: MacOS X'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'Operating System :: Microsoft :: Windows'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'Programming Language :: Python :: 2'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'Programming Language :: Python :: 2.7'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'Programming Language :: Python :: 3'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'Programming Language :: Python :: 3.5'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'Programming Language :: Python :: 3.6'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'Topic :: Office/Business :: Scheduling'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'Topic :: Scientific/Engineering'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'Topic :: Scientific/Engineering :: Mathematics'," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  'Topic :: Software Development :: Libraries :: Python Modules'" >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo "  ]," >> setup.py
  COMMAND ${CMAKE_COMMAND} -E echo ")" >> setup.py
  COMMENT "Generate setup.py at build time (to use generator expression)"
  WORKING_DIRECTORY python
  VERBATIM)

# Find if python module MODULE_NAME is available,
# if not install it to the Python user install directory.
function(search_python_module MODULE_NAME)
  execute_process(
    COMMAND ${PYTHON_EXECUTABLE} -c "import ${MODULE_NAME}; print(${MODULE_NAME}.__version__)"
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
      COMMAND ${PYTHON_EXECUTABLE} -m pip install --user ${MODULE_NAME}
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
  endif()
endfunction()

# Look for python module wheel
search_python_module(setuptools)
search_python_module(wheel)

# Main Target
add_custom_target(python_package ALL
  DEPENDS
    ortools::ortools
    Py${PROJECT_NAME}_proto
    python/setup.py
  COMMAND ${CMAKE_COMMAND} -E remove_directory dist
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_NAME}/.libs
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywrapknapsack_solver> ${PROJECT_NAME}/algorithms
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywrapgraph> ${PROJECT_NAME}/graph
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywrapcp> ${PROJECT_NAME}/constraint_solver
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywraplp> ${PROJECT_NAME}/linear_solver
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywrapsat> ${PROJECT_NAME}/sat
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:pywraprcpsp> ${PROJECT_NAME}/data
  #COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ortools> ${PROJECT_NAME}/.libs
  COMMAND ${CMAKE_COMMAND} -E copy $<$<NOT:$<PLATFORM_ID:Windows>>:$<TARGET_SONAME_FILE:ortools>> ${PROJECT_NAME}/.libs
  #COMMAND ${PYTHON_EXECUTABLE} setup.py bdist_egg bdist_wheel
  COMMAND ${PYTHON_EXECUTABLE} setup.py bdist_wheel
  BYPRODUCTS
    python/${PROJECT_NAME}
    python/build
    python/dist
    python/${PROJECT_NAME}.egg-info
  WORKING_DIRECTORY python
  )

# Test
if(BUILD_TESTING)
  # Look for python module virtualenv
  search_python_module(virtualenv)
  # Testing using a vitual environment
  set(VENV_DIR ${CMAKE_BINARY_DIR}/venv)
  if(WIN32)
    set(VENV_PYTHON_EXECUTABLE "${VENV_DIR}\\Scripts\\python.exe")
  else()
    set(VENV_PYTHON_EXECUTABLE ${VENV_DIR}/bin/python)
  endif()
  # make a virtualenv to install our python package in it
  add_custom_command(TARGET python_package POST_BUILD
    COMMAND ${PYTHON_EXECUTABLE} -m virtualenv -p ${PYTHON_EXECUTABLE} ${VENV_DIR}
    # Must not call it in a folder containing the setup.py otherwise pip call it
    # (i.e. "python setup.py bdist") while we want to consume the wheel package
    COMMAND ${VENV_PYTHON_EXECUTABLE} -m pip install --find-links=${CMAKE_CURRENT_BINARY_DIR}/python/dist ${PROJECT_NAME}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/test.py.in ${VENV_DIR}/test.py
    BYPRODUCTS ${VENV_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} VERBATIM)
  # run the tests within the virtualenv
  add_test(pytest_venv ${VENV_PYTHON_EXECUTABLE} ${VENV_DIR}/test.py)

  #add_subdirectory(examples/python)
  #add_subdirectory(examples/notebook)
endif()
