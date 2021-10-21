#[=======================================================================[.rst:
FindCPLEX
--------

This module determines the CPLEX library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``CPLEX::CPLEX``, if
CPLEX has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

CPLEX_FOUND          - True if CPLEX found.

Hints
^^^^^

A user may set ``CPLEX_ROOT`` to a CPLEX installation root to tell this
module where to look.
#]=======================================================================]
set(CPLEX_FOUND FALSE)

if(CMAKE_C_COMPILER_LOADED)
  include (CheckIncludeFile)
  include (CheckCSourceCompiles)
elseif(CMAKE_CXX_COMPILER_LOADED)
  include (CheckIncludeFileCXX)
  include (CheckCXXSourceCompiles)
else()
  message(FATAL_ERROR "FindCPLEX only works if either C or CXX language is enabled")
endif()

if(NOT CPLEX_ROOT)
  if(DEFINED ENV{UNIX_CPLEX_DIR})
    set(CPLEX_ROOT $ENV{UNIX_CPLEX_DIR})
  elseif(DEFINED ENV{CPLEX_ROOT})
    set(CPLEX_ROOT $ENV{CPLEX_ROOT})
  endif()
endif()
message(STATUS "CPLEX_ROOT: ${CPLEX_ROOT}")
if(NOT CPLEX_ROOT)
  message(FATAL_ERROR "CPLEX_ROOT: not found")
else()
  set(CPLEX_FOUND TRUE)
endif()

if(CPLEX_FOUND AND NOT TARGET CPLEX::CPLEX)
  add_library(CPLEX::CPLEX UNKNOWN IMPORTED)

  set_target_properties(CPLEX::CPLEX PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CPLEX_INCLUDE_DIRS}")

  if(APPLE) # be aware that `UNIX` is `TRUE` on OS X, so this check must be first
    set_target_properties(CPLEX::CPLEX PROPERTIES
      IMPORTED_LOCATION "${CPLEX_ROOT}/cplex/lib/path/to/required.lib") # FIXME need actual Apple library path
  elseif(UNIX)
    set_target_properties(CPLEX::CPLEX PROPERTIES
      IMPORTED_LOCATION "${CPLEX_ROOT}/cplex/lib/x86-64_linux/static_pic/libcplex.a")
  elseif(MSVC)
    set_target_properties(CPLEX::CPLEX PROPERTIES
      IMPORTED_LOCATION "${CPLEX_ROOT}/cplex/lib/x64_windows_msvc14/stat_mda/cplex12100.lib")
  else()
    message(FATAL_ERROR "CPLEX not supported for ${CMAKE_SYSTEM}")
  endif()
endif()
