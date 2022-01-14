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

pkg_check_modules(Clp REQUIRED clp IMPORTED_TARGET GLOBAL)
add_library(Coin::Clp ALIAS PkgConfig::Clp)
add_library(Coin::ClpSolver ALIAS PkgConfig::Clp)

pkg_check_modules(OSI_CLP REQUIRED osi-clp IMPORTED_TARGET GLOBAL)
add_library(Coin::OsiClp ALIAS PkgConfig::OSI_CLP)
