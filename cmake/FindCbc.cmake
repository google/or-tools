#[=======================================================================[.rst:
FindCbc
--------

This module determines the Cbc library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``Coin::Cbc`` and ``Coin::OsiCbc``, if
Cbc has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

Cbc_FOUND          - True if Cbc found.

#]=======================================================================]
find_package(PkgConfig REQUIRED)

pkg_check_modules(CBC REQUIRED cbc IMPORTED_TARGET GLOBAL)
add_library(Coin::Cbc ALIAS PkgConfig::CBC)
add_library(Coin::CbcSolver ALIAS PkgConfig::CBC)

pkg_check_modules(OSI_CBC REQUIRED osi-cbc IMPORTED_TARGET GLOBAL)
add_library(Coin::OsiCbc ALIAS PkgConfig::OSI_CBC)
