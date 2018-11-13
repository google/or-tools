# Setup SWIG
find_package(SWIG REQUIRED)
include(UseSWIG)

# Setup Python
find_package(PythonInterp REQUIRED)
find_package(PythonLibs REQUIRED)

set_property(SOURCE sat.i PROPERTY CPLUSPLUS ON)
list(APPEND CMAKE_SWIG_FLAGS "-module;pywrapsat")

# outdir for .py file
set(CMAKE_SWIG_OUTDIR ${CMAKE_CURRENT_BINARY_DIR}/..)
# outdir for .so file
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/..)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/..)
# Second, for multi-config builds (e.g. msvc)
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_CURRENT_BINARY_DIR}/..)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_CURRENT_BINARY_DIR}/..)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_CURRENT_BINARY_DIR}/..)
endforeach()

if(${CMAKE_VERSION} VERSION_GREATER 3.8.2)
  swig_add_library(pywrapsat TYPE SHARED LANGUAGE python SOURCES sat.i)
else()
  swig_add_module(pywrapsat python sat.i)
endif()

# ${SWIG_MODULE_<name>_REAL_NAME} will resolve to _<name>.so TARGET
target_include_directories(${SWIG_MODULE_pywrapsat_REAL_NAME} PRIVATE
  ${PYTHON_INCLUDE_DIRS})
target_link_libraries(${SWIG_MODULE_pywrapsat_REAL_NAME} PRIVATE
  ortools::ortools ${PYTHON_LIBRARIES})
if(UNIX)
  if(NOT APPLE)
    set_target_properties(${SWIG_MODULE_pywrapsat_REAL_NAME} PROPERTIES
      INSTALL_RPATH "$ORIGIN/../../:$ORIGIN")
  else()
    set_target_properties(${SWIG_MODULE_pywrapsat_REAL_NAME} PROPERTIES
      SUFFIX ".so"
      INSTALL_RPATH "@loader_path/../..;@loader_path")
    set_property(TARGET ${SWIG_MODULE_pywrapsat_REAL_NAME} APPEND PROPERTY
      LINK_FLAGS "-flat_namespace -undefined suppress")
  endif()
endif()
