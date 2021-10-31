#[=======================================================================[.rst:
FindSCIP
--------

This module determines the SCIP library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``SCIP::SCIP``, if
SCIP has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

SCIP_FOUND          - True if SCIP found.

Hints
^^^^^

A user may set ``SCIP_ROOT`` to a SCIP installation root to tell this
module where to look.
#]=======================================================================]
set(SCIP_FOUND FALSE)

if(CMAKE_C_COMPILER_LOADED)
  include (CheckIncludeFile)
  include (CheckCSourceCompiles)
elseif(CMAKE_CXX_COMPILER_LOADED)
  include (CheckIncludeFileCXX)
  include (CheckCXXSourceCompiles)
else()
  message(FATAL_ERROR "FindSCIP only works if either C or CXX language is enabled")
endif()

if(NOT SCIP_ROOT)
  set(SCIP_ROOT $ENV{SCIP_ROOT})
endif()
message(STATUS "SCIP_ROOT: ${SCIP_ROOT}")
if(NOT SCIP_ROOT)
  message(FATAL_ERROR "SCIP_ROOT: not found")
else()
  set(SCIP_FOUND TRUE)
endif()

if(SCIP_FOUND AND NOT TARGET SCIP::SCIP)
  add_library(SCIP::SCIP UNKNOWN IMPORTED)

  set_target_properties(SCIP::SCIP PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${SCIP_ROOT}/include")

  if(APPLE)
    set(SCIP_ARCH darwin.x86_64.gnu.opt)
    set_property(TARGET SCIP::SCIP PROPERTY IMPORTED_LOCATION
      -force_load
      ${SCIP_ROOT}/lib/libscip.a
      ${SCIP_ROOT}/lib/libscipopt.a
      ${SCIP_ROOT}/lib/libsoplex.a
      ${SCIP_ROOT}/lib/libsoplex.${SCIP_ARCH}.a
      )
  elseif(UNIX)
    set(SCIP_ARCH linux.x86_64.gnu.opt)
    set_property(TARGET SCIP::SCIP PROPERTY IMPORTED_LOCATION
      ${SCIP_ROOT}/lib/libscip.a
      ${SCIP_ROOT}/lib/libscipopt.a
      ${SCIP_ROOT}/lib/libsoplex.a
      ${SCIP_ROOT}/lib/libsoplex.${SCIP_ARCH}.a
      )
  elseif(MSVC)
    set_property(TARGET SCIP::SCIP PROPERTY IMPORTED_LOCATION
      ${SCIP_ROOT}/lib/scip.lib
      ${SCIP_ROOT}/lib/soplex.lib
      ignore:4006
      )
  endif()
endif()
