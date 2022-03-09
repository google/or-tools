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
include(FindPackageHandleStandardArgs)

if(NOT CoinUtils_NO_CoinUtils_CMAKE)
  # do a find package call to specifically look for the CMake version
  # of CoinUtils
  find_package(CoinUtils QUIET NO_MODULE)

  # if we found the CoinUtils cmake package then we are done, and
  # can print what we found and return.
  if(CoinUtils_FOUND)
    find_package_handle_standard_args(CoinUtils CONFIG_MODE)
    return()
  endif()
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(CoinUtils QUIET coinutils IMPORTED_TARGET GLOBAL)
  if(CoinUtils_FOUND)
    add_library(Coin::CoinUtils ALIAS PkgConfig::CoinUtils)
  endif()
endif()
