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

pkg_check_modules(Cbc REQUIRED cbc IMPORTED_TARGET GLOBAL)
add_library(Coin::Cbc ALIAS PkgConfig::Cbc)
add_library(Coin::CbcSolver ALIAS PkgConfig::Cbc)

pkg_check_modules(OSI_CBC REQUIRED osi-cbc IMPORTED_TARGET GLOBAL)
add_library(Coin::OsiCbc ALIAS PkgConfig::OSI_CBC)
