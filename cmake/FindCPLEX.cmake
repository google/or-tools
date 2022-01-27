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

# TODO(mizux) Bump support to CPLEX 20.1.0
# ref: https://www.ibm.com/docs/en/icos/20.1.0?topic=cplex-setting-up-gnulinuxmacos
# ref: https://www.ibm.com/docs/en/icos/20.1.0?topic=cplex-setting-up-windows

# ref: https://www.ibm.com/docs/en/icos/12.10.0?topic=cplex-setting-up-gnulinuxmacos
if(CPLEX_FOUND AND NOT TARGET CPLEX::CPLEX)
  add_library(CPLEX::CPLEX UNKNOWN IMPORTED)
  target_include_directories(CPLEX::CPLEX SYSTEM INTERFACE "${CPLEX_ROOT}/cplex/include")

  if(APPLE) # be aware that `UNIX` is `TRUE` on OS X, so this check must be first
    # FIXME(mizux) need actual macOS library path
    # target_link_libraries(CPLEX::CPLEX INTERFACE "${CPLEX_ROOT}/cplex/lib/libcplex.lib")
    message(FATAL_ERROR "CPLEX not yet supported for CMake builds on ${CMAKE_SYSTEM}")
  elseif(UNIX)
    target_link_libraries(CPLEX::CPLEX INTERFACE "${CPLEX_ROOT}/cplex/lib/x86-64_linux/static_pic/libcplex.a")
  elseif(MSVC)
    target_link_libraries(CPLEX::CPLEX INTERFACE "${CPLEX_ROOT}/cplex/lib/x64_windows_msvc14/stat_mda/cplex12100.lib")
  else()
    message(FATAL_ERROR "CPLEX not supported for ${CMAKE_SYSTEM}")
  endif()
endif()
