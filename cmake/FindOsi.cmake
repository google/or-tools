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
find_package(PkgConfig REQUIRED)

pkg_check_modules(OSI REQUIRED osi IMPORTED_TARGET GLOBAL)
add_library(Coin::Osi ALIAS PkgConfig::OSI)
