#[=======================================================================[.rst:
FindEigen3
--------

This module determines the Eigen3 library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``Eigen3::Eigen``, if
Eigen3 has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

Eigen3_FOUND          - True if Eigen3 found.

#]=======================================================================]
include(FindPackageHandleStandardArgs)

find_package(Eigen3 QUIET NO_MODULE)
# if we found the Eigen3 cmake package then we are done, and
# can print what we found and return.
if(Eigen3_FOUND)
  find_package_handle_standard_args(Eigen3 CONFIG_MODE)
  return()
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(Eigen3 QUIET eigen3 IMPORTED_TARGET GLOBAL)
  if(Eigen3_FOUND)
    add_library(Eigen3::Eigen ALIAS PkgConfig::Eigen3)
  endif()
endif()
