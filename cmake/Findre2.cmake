#[=======================================================================[.rst:
Findre2
--------

This module determines the re2 library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``re2::re2``,
if re2 has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

re2_FOUND          - True if re2 found.

#]=======================================================================]
include(FindPackageHandleStandardArgs)

if(NOT re2_NO_re2_CMAKE)
  # do a find package call to specifically look for the CMake version
  # of re2
  find_package(re2 QUIET NO_MODULE)

  # if we found the re2 cmake package then we are done, and
  # can print what we found and return.
  if(re2_FOUND)
    find_package_handle_standard_args(re2 CONFIG_MODE)
    return()
  endif()
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(re2 QUIET re2 IMPORTED_TARGET GLOBAL)
  if(re2_FOUND)
    add_library(re2::re2 ALIAS PkgConfig::re2)
  endif()
endif()
