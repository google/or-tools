#[=======================================================================[.rst:
FindCoinUtils
--------

This module determines the CoinUtils library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``Coin::CoinUtils``, if
CoinUtils has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

CoinUtils_FOUND          - True if CoinUtils found.

#]=======================================================================]
find_package(PkgConfig REQUIRED)

pkg_check_modules(COINUTILS REQUIRED coinutils IMPORTED_TARGET GLOBAL)
add_library(Coin::CoinUtils ALIAS PkgConfig::COINUTILS)
