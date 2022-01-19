#[=======================================================================[.rst:
FindClp
--------

This module determines the Clp library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``Coin::Clp`` and ``Coin::OsiClp``, if
Clp has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

Clp_FOUND          - True if Clp found.
OsiClp_FOUND       - True if Osi-Clp found.

#]=======================================================================]
include(FindPackageHandleStandardArgs)

if(NOT Clp_NO_Clp_CMAKE)
  # do a find package call to specifically look for the CMake version
  # of Clp
  find_package(Clp QUIET NO_MODULE)

  # if we found the Clp cmake package then we are done, and
  # can print what we found and return.
  if(Clp_FOUND)
    find_package_handle_standard_args(Clp CONFIG_MODE)
    return()
  endif()
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(Clp QUIET cbc IMPORTED_TARGET GLOBAL)
  if(Clp_FOUND)
    add_library(Coin::Clp ALIAS PkgConfig::Clp)
    add_library(Coin::ClpSolver ALIAS PkgConfig::Clp)
  endif()

  pkg_check_modules(OsiClp QUIET osi-cbc IMPORTED_TARGET GLOBAL)
  if(OsiClp_FOUND)
    add_library(Coin::OsiClp ALIAS PkgConfig::OsiClp)
  endif()
endif()
