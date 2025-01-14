# Copyright 2010-2025 Google LLC
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
include(FindPackageHandleStandardArgs)

if(NOT Osi_NO_Osi_CMAKE)
  # do a find package call to specifically look for the CMake version
  # of Osi
  find_package(Osi QUIET NO_MODULE)

  # if we found the Osi cmake package then we are done, and
  # can print what we found and return.
  if(Osi_FOUND)
    find_package_handle_standard_args(Osi CONFIG_MODE)
    return()
  endif()
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(Osi QUIET osi IMPORTED_TARGET GLOBAL)
  if(Osi_FOUND)
    add_library(Coin::Osi ALIAS PkgConfig::Osi)
  endif()
endif()
