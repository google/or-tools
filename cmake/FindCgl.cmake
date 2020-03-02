#[=======================================================================[.rst:
FindCgl
--------

This module determines the Cgl library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``Coin::Cgl``, if
Cgl has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

Cgl_FOUND          - True if Cgl found.

#]=======================================================================]
find_package(PkgConfig REQUIRED)

pkg_check_modules(CGL REQUIRED cgl IMPORTED_TARGET GLOBAL)
add_library(Coin::Cgl ALIAS PkgConfig::CGL)
