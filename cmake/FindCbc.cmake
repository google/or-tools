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
OsiCbc_FOUND       - True if Osi-Cbc found.

#]=======================================================================]
include(FindPackageHandleStandardArgs)

if(NOT Cbc_NO_Cbc_CMAKE)
  # do a find package call to specifically look for the CMake version
  # of Cbc
  find_package(Cbc QUIET NO_MODULE)

  # if we found the Cbc cmake package then we are done, and
  # can print what we found and return.
  if(Cbc_FOUND)
    find_package_handle_standard_args(Cbc CONFIG_MODE)
    return()
  endif()
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(Cbc QUIET cbc IMPORTED_TARGET GLOBAL)
  if(Cbc_FOUND)
    add_library(Coin::Cbc ALIAS PkgConfig::Cbc)
    add_library(Coin::CbcSolver ALIAS PkgConfig::Cbc)
  endif()

  pkg_check_modules(OsiCbc QUIET osi-cbc IMPORTED_TARGET GLOBAL)
  if(OsiCbc_FOUND)
    add_library(Coin::OsiCbc ALIAS PkgConfig::OsiCbc)
  endif()
endif()
