# Copyright 2010-2022 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
OsiClp_FOUND       - True if Osi-Clp found.

#]=======================================================================]
include(FindPackageHandleStandardArgs)

if(NOT Clp_NO_Clp_CMAKE)
  # do a find package call to specifically look for the CMake version
  # of Clp
  find_package(Clp QUIET NO_MODULE)

  # if we found the Clp cmake package then we are done, and
  # can print what we found and return.
  if(Clp_FOUND)
    find_package_handle_standard_args(Clp CONFIG_MODE)
    return()
  endif()
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(Clp QUIET cbc IMPORTED_TARGET GLOBAL)
  if(Clp_FOUND)
    add_library(Coin::Clp ALIAS PkgConfig::Clp)
    add_library(Coin::ClpSolver ALIAS PkgConfig::Clp)
  endif()

  pkg_check_modules(OsiClp QUIET osi-cbc IMPORTED_TARGET GLOBAL)
  if(OsiClp_FOUND)
    add_library(Coin::OsiClp ALIAS PkgConfig::OsiClp)
  endif()
endif()
