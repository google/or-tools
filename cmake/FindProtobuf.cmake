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
FindProtobuf
--------

This module determines the Protobuf library of the system.

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``protobuf::libprotobuf`` and
``protobuf::protoc``, if Protobuf has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

::

Protobuf_FOUND          - True if Protobuf found.

#]=======================================================================]
find_package(PkgConfig REQUIRED)

pkg_check_modules(PROTOBUF REQUIRED protobuf>=3.12 IMPORTED_TARGET GLOBAL)
add_library(protobuf::libprotobuf ALIAS PkgConfig::PROTOBUF)
set_target_properties(PkgConfig::PROTOBUF PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PROTOBUF_INCLUDEDIR}")

find_program(PROTOC_EXEC protoc REQUIRED)
add_executable(protobuf::protoc IMPORTED GLOBAL)
set_target_properties(protobuf::protoc PROPERTIES
  IMPORTED_LOCATION ${PROTOC_EXEC}
)
