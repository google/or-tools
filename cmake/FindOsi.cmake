#[=======================================================================[.rst:
FindOsi
--------

This module determines the Osi library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``Coin::Osi``, if
Osi has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

Osi_FOUND          - True if Osi found.

#]=======================================================================]
include(FindPackageHandleStandardArgs)

if(NOT Osi_NO_Osi_CMAKE)
  # do a find package call to specifically look for the CMake version
  # of Osi
  find_package(Osi QUIET NO_MODULE)

  # if we found the Osi cmake package then we are done, and
  # can print what we found and return.
  if(Osi_FOUND)
    find_package_handle_standard_args(Osi CONFIG_MODE)
    return()
  endif()
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(Osi QUIET osi IMPORTED_TARGET GLOBAL)
  if(Osi_FOUND)
    add_library(Coin::Osi ALIAS PkgConfig::Osi)
  endif()
endif()
