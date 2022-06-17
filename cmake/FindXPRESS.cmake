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
FindXPRESS
--------

This module determines the XPRESS library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``XPRESS::XPRESS``, if
XPRESS has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

XPRESS_FOUND          - True if XPRESS found.

Hints
^^^^^

A user may set ``XPRESS_ROOT`` to a XPRESS installation root to tell this
module where to look.
#]=======================================================================]
set(XPRESS_FOUND FALSE)

if(CMAKE_C_COMPILER_LOADED)
  include (CheckIncludeFile)
  include (CheckCSourceCompiles)
elseif(CMAKE_CXX_COMPILER_LOADED)
  include (CheckIncludeFileCXX)
  include (CheckCXXSourceCompiles)
else()
  message(FATAL_ERROR "FindXPRESS only works if either C or CXX language is enabled")
endif()

if(NOT XPRESS_ROOT)
  set(XPRESS_ROOT $ENV{XPRESS_ROOT})
endif()
message(STATUS "XPRESS_ROOT: ${XPRESS_ROOT}")
if(NOT XPRESS_ROOT)
  message(FATAL_ERROR "XPRESS_ROOT: not found")
else()
  set(XPRESS_FOUND TRUE)
endif()

if(XPRESS_FOUND AND NOT TARGET XPRESS::XPRESS)
  add_library(XPRESS::XPRESS UNKNOWN IMPORTED)

  if(UNIX)
    set_target_properties(XPRESS::XPRESS PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${XPRESS_ROOT}/include")
  endif()

  if(APPLE)
    set_target_properties(XPRESS::XPRESS PROPERTIES
      #INSTALL_RPATH_USE_LINK_PATH TRUE
      #BUILD_WITH_INSTALL_RPATH TRUE
      #INTERFACE_LINK_DIRECTORIES "${XPRESS_ROOT}/lib"
      #INSTALL_RPATH "${XPRESS_ROOT}/lib;${INSTALL_RPATH}"
      IMPORTED_LOCATION "${XPRESS_ROOT}/lib/libxprs.dylib")
  elseif(UNIX)
    set_target_properties(XPRESS::XPRESS PROPERTIES
      INTERFACE_LINK_DIRECTORIES "${XPRESS_ROOT}/lib"
      IMPORTED_LOCATION ${XPRESS_ROOT}/lib/libxprs.so)
  elseif(MSVC)
    set_target_properties(XPRESS::XPRESS PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${XPRESS_ROOT}\\include"
      IMPORTED_LOCATION "${XPRESS_ROOT}\\lib\\xprs.lib")
  endif()
endif()
