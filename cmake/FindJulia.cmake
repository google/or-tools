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
FindJulia
---------

This module determines the Julia interpreter of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``Julia::Interpreter``, if
Julia has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

Julia_FOUND          - True if Julia found.
Julia_BIN            - The path to the Julia executable.
Julia_VERSION        - The version of the Julia executable if found.

Hints
^^^^^

A user may set ``JULIA_BINDIR`` to a folder containing the Julia binary
to tell this module where to look.
#]=======================================================================]

include(FindPackageHandleStandardArgs)

if(DEFINED ENV{JULIA_BINDIR})
  message(STATUS "JULIA_BINDIR: $ENV{JULIA_BINDIR}")
endif()

set(Julia_FOUND FALSE)

if(DEFINED ENV{JULIA_BINDIR})
  find_program(Julia_BIN julia PATHS $ENV{JULIA_BINDIR} DOC "Julia executable")
else()
  find_program(Julia_BIN julia DOC "Julia executable")
endif()
message(STATUS "Julia_BIN: ${Julia_BIN}")

if(Julia_BIN)
  execute_process(
    COMMAND "${Julia_BIN}" --startup-file=no --version
    OUTPUT_VARIABLE Julia_VERSION
  )
  message(STATUS "Julia_VERSION: ${Julia_VERSION}")
  set(Julia_FOUND TRUE)
endif()

