#[=======================================================================[.rst:
FindGLPK
--------

This module determines the GLPK library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``GLPK::GLPK``, if
GLPK has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

``GLPK_FOUND``
  True if ``GLPK`` found.

``GLPK_INCLUDE_DIRS``
  Where to find glpk.h, etc.

``GLPK_LIBRARIES``
  List of libraries for using ``glpk``.

Hints
^^^^^

A user may set ``GLPK_ROOT`` environment to a GLPK installation root to tell this
module where to look.
#]=======================================================================]
# first specifically look for the CMake version of GLPK
find_package(GLPK QUIET NO_MODULE)
# if we found the GLPK cmake package then we are done.
if(GLPK_FOUND)
  return()
endif()

set(GLPK_FOUND FALSE)
if(CMAKE_C_COMPILER_LOADED)
  include (CheckIncludeFile)
  include (CheckCSourceCompiles)
elseif(CMAKE_CXX_COMPILER_LOADED)
  include (CheckIncludeFileCXX)
  include (CheckCXXSourceCompiles)
else()
  message(FATAL_ERROR "FindGLPK only works if either C or CXX language is enabled")
endif()

if(DEFINED ENV{GLPK_ROOT})
  message(STATUS "GLPK_ROOT: ${GLPK_ROOT}")
endif()

if(APPLE)
  find_path(GLPK_INCLUDE_DIRS glpk.h
    HINTS
      ENV GLPK_ROOT
      /Library/Frameworks/Glpk.framework/Headers
    )
  find_library(GLPK_LIBRARIES glpk
    HINTS
      ENV GLPK_ROOT
      /Library/Frameworks/Glpk.framework/Libraries
    )
  set(GLPK_LIBRARIES "-framework glpk" CACHE STRING "Glpk library for OSX")
else()
  find_path(GLPK_INCLUDE_DIRS glpk.h
    HINTS
      ENV GLPK_ROOT
    )
  find_library(GLPK_LIBRARIES glpk
    HINTS
      ENV GLPK_ROOT
    )
endif()

if(GLPK_INCLUDE_DIRS AND GLPK_LIBRARIES)
  if(NOT TARGET GLPK::GLPK)
    add_library(GLPK::GLPK UNKNOWN IMPORTED)
    set_property(TARGET GLPK::GLPK PROPERTY IMPORTED_LOCATION ${GLPK_LIBRARIES})
    target_include_directories(GLPK::GLPK INTERFACE ${GLPK_INCLUDE_DIRS})
  endif()
endif()

find_package_handle_standard_args(GLPK DEFAULT_MSG GLPK_LIBRARIES GLPK_INCLUDE_DIRS)
