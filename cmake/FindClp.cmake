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

#]=======================================================================]
find_package(PkgConfig REQUIRED)

pkg_check_modules(CLP REQUIRED clp IMPORTED_TARGET GLOBAL)
add_library(Coin::Clp ALIAS PkgConfig::CLP)
add_library(Coin::ClpSolver ALIAS PkgConfig::CLP)

pkg_check_modules(OSI_CLP REQUIRED osi-clp IMPORTED_TARGET GLOBAL)
add_library(Coin::OsiClp ALIAS PkgConfig::OSI_CLP)
