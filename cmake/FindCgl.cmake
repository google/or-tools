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
include(FindPackageHandleStandardArgs)

if(NOT Cgl_NO_Cgl_CMAKE)
  # do a find package call to specifically look for the CMake version
  # of Cgl
  find_package(Cgl QUIET NO_MODULE)

  # if we found the Cgl cmake package then we are done, and
  # can print what we found and return.
  if(Cgl_FOUND)
    find_package_handle_standard_args(Cgl CONFIG_MODE)
    return()
  endif()
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(Cgl QUIET cgl IMPORTED_TARGET GLOBAL)
  if(Cgl_FOUND)
    add_library(Coin::Cgl ALIAS PkgConfig::Cgl)
  endif()
endif()
