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
FindSCIP
--------

This module determines the SCIP library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``SCIP::libscip``, if
SCIP has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

SCIP_FOUND          - True if SCIP found.

Hints
^^^^^

A user may set ``SCIP_ROOT`` to a SCIP installation root to tell this
module where to look.
#]=======================================================================]
include(FindPackageHandleStandardArgs)

# first specifically look for the CMake version of SCIP
find_package(SCIP QUIET NO_MODULE)
# if we found the SCIP cmake package then we are done.
if(SCIP_FOUND)
  find_package_handle_standard_args(SCIP CONFIG_MODE)
  if(NOT TARGET SCIP::libscip)
    message(WARNING "SCIP::libscip not provided")
    add_library(SCIP::libscip ALIAS libscip)
  endif()
  return()
endif()

set(SCIP_FOUND FALSE)
if(CMAKE_C_COMPILER_LOADED)
  include (CheckIncludeFile)
  include (CheckCSourceCompiles)
elseif(CMAKE_CXX_COMPILER_LOADED)
  include (CheckIncludeFileCXX)
  include (CheckCXXSourceCompiles)
else()
  message(FATAL_ERROR "FindSCIP only works if either C or CXX language is enabled")
endif()

if(NOT SCIP_ROOT)
  set(SCIP_ROOT $ENV{SCIP_ROOT})
endif()
message(STATUS "SCIP_ROOT: ${SCIP_ROOT}")
if(NOT SCIP_ROOT)
  message(FATAL_ERROR "SCIP_ROOT: not found")
else()
  set(SCIP_FOUND TRUE)
endif()

if(SCIP_FOUND AND NOT TARGET SCIP::libscip)
  add_library(SCIP::libscip UNKNOWN IMPORTED)

  set_target_properties(SCIP::libscip PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${SCIP_ROOT}/include")

  if(APPLE)
    set(SCIP_ARCH darwin.x86_64.gnu.opt)
    set_property(TARGET SCIP::libscip PROPERTY IMPORTED_LOCATION
      -force_load
      ${SCIP_ROOT}/lib/libscip.a
      ${SCIP_ROOT}/lib/libscipopt.a
      ${SCIP_ROOT}/lib/libsoplex.a
      ${SCIP_ROOT}/lib/libsoplex.${SCIP_ARCH}.a
      )
  elseif(UNIX)
    set(SCIP_ARCH linux.x86_64.gnu.opt)
    set_property(TARGET SCIP::libscip PROPERTY IMPORTED_LOCATION
      ${SCIP_ROOT}/lib/libscip.a
      ${SCIP_ROOT}/lib/libscipopt.a
      ${SCIP_ROOT}/lib/libsoplex.a
      ${SCIP_ROOT}/lib/libsoplex.${SCIP_ARCH}.a
      )
  elseif(MSVC)
    set_property(TARGET SCIP::libscip PROPERTY IMPORTED_LOCATION
      ${SCIP_ROOT}/lib/scip.lib
      ${SCIP_ROOT}/lib/soplex.lib
      ignore:4006
      )
  else()
    message(FATAL_ERROR "OR-Tools with SCIP not supported for ${CMAKE_SYSTEM}")
  endif()
endif()
