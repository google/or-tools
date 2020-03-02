#[=======================================================================[.rst:
Findglog
--------

This module determines the glog library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``glog::glog``, if
glog has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

glog_FOUND          - True if glog found.

#]=======================================================================]
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLOG REQUIRED libglog IMPORTED_TARGET GLOBAL)
add_library(glog::glog ALIAS PkgConfig::GLOG)
