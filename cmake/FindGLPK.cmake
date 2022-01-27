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

GLPK_FOUND          - True if GLPK found.

Hints
^^^^^

A user may set ``GLPK_ROOT`` to a GLPK installation root to tell this
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

if(NOT GLPK_ROOT)
  if (DEFINED ENV{GLPK_ROOT})
    set(GLPK_ROOT $ENV{GLPK_ROOT})
  else()
    find_path(GLPK_PATH libglpk.a libglpk.lib
      DOC "Path in which libglpk is found"
      PATHS
      "/usr/local/lib"
      "/usr/lib"
      "${CMAKE_BINARY_DIR}/dependencies/GLPK/source/w32"
      "${CMAKE_BINARY_DIR}/dependencies/GLPK/source/w64"
      "${CMAKE_BINARY_DIR}/dependencies/GLPK/source/src/.lib"
      )
    if(GLPK_PATH)
      set(GLPK_ROOT ${GLPK_PATH})
    endif()
  endif()
endif()

message(STATUS "GLPK_ROOT: ${GLPK_ROOT}")
if(NOT GLPK_ROOT)
  message(FATAL_ERROR "GLPK_ROOT: not found")
else()
  set(GLPK_FOUND TRUE)
endif()

if(GLPK_FOUND AND NOT TARGET GLPK::GLPK)
  add_library(GLPK::GLPK UNKNOWN IMPORTED)
  if (UNIX)
    set_property(TARGET GLPK::GLPK PROPERTY IMPORTED_LOCATION
      ${GLPK_ROOT}/libglpk.a
      )
  elseif(MSVC)
    set_property(TARGET GLPK::GLPK PROPERTY IMPORTED_LOCATION
      ${GLPK_ROOT}/libglpk.lib
      )
  else()
    message(FATAL_ERROR "OR-Tools with GLPK not supported for ${CMAKE_SYSTEM}")
  endif()
endif()
