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
FindCPLEX
--------

This module determines the CPLEX library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``CPLEX::CPLEX``, if
CPLEX has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

CPLEX_FOUND          - True if CPLEX found.

Hints
^^^^^

A user may set ``CPLEX_ROOT`` to a CPLEX installation root to tell this
module where to look.
#]=======================================================================]
set(CPLEX_FOUND FALSE)

if(CMAKE_C_COMPILER_LOADED)
  include (CheckIncludeFile)
  include (CheckCSourceCompiles)
elseif(CMAKE_CXX_COMPILER_LOADED)
  include (CheckIncludeFileCXX)
  include (CheckCXXSourceCompiles)
else()
  message(FATAL_ERROR "FindCPLEX only works if either C or CXX language is enabled")
endif()

if(NOT CPLEX_ROOT)
  if(DEFINED ENV{UNIX_CPLEX_DIR})
    set(CPLEX_ROOT $ENV{UNIX_CPLEX_DIR})
  elseif(DEFINED ENV{CPLEX_ROOT})
    set(CPLEX_ROOT $ENV{CPLEX_ROOT})
  endif()
endif()
message(STATUS "CPLEX_ROOT: ${CPLEX_ROOT}")
if(NOT CPLEX_ROOT)
  message(FATAL_ERROR "CPLEX_ROOT: not found")
else()
  set(CPLEX_FOUND TRUE)
endif()

# TODO(user) Bump support to CPLEX 20.1.0
# ref: https://www.ibm.com/docs/en/icos/20.1.0?topic=cplex-setting-up-gnulinuxmacos
# ref: https://www.ibm.com/docs/en/icos/20.1.0?topic=cplex-setting-up-windows

# ref: https://www.ibm.com/docs/en/icos/12.10.0?topic=cplex-setting-up-gnulinuxmacos
if(CPLEX_FOUND AND NOT TARGET CPLEX::CPLEX)
  add_library(CPLEX::CPLEX UNKNOWN IMPORTED)
  target_include_directories(CPLEX::CPLEX SYSTEM INTERFACE "${CPLEX_ROOT}/cplex/include")

  if(APPLE) # be aware that `UNIX` is `TRUE` on OS X, so this check must be first
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)")
      message(FATAL_ERROR "CPLEX do not support Apple M1, can't find a suitable static library")
    else()
      set_target_properties(CPLEX::CPLEX PROPERTIES
        IMPORTED_LOCATION "${CPLEX_ROOT}/cplex/lib/x86-64_osx/static_pic/libcplex.a")
    endif()
  elseif(UNIX)
    set_target_properties(CPLEX::CPLEX PROPERTIES
      IMPORTED_LOCATION "${CPLEX_ROOT}/cplex/lib/x86-64_linux/static_pic/libcplex.a")
  elseif(MSVC)
    set_target_properties(CPLEX::CPLEX PROPERTIES
      IMPORTED_LOCATION "${CPLEX_ROOT}/cplex/lib/x64_windows_msvc14/stat_mda/cplex12100.lib")
  else()
    message(FATAL_ERROR "CPLEX not supported for ${CMAKE_SYSTEM}")
  endif()
endif()
