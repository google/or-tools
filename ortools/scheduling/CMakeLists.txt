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

list(APPEND _SRCS
  course_scheduling.cc
  course_scheduling.h
  jobshop_scheduling_parser.cc
  jobshop_scheduling_parser.h
  rcpsp_parser.cc
  rcpsp_parser.h
)

ortools_cxx_library(
  NAME
    ortools_scheduling
  TYPE
    SHARED
  SOURCES
    ${_SRCS}
    $<TARGET_OBJECTS:scheduling_proto>
  COMPILE_DEFINITIONS
    ${OR_TOOLS_COMPILE_DEFINITIONS}
  COMPILE_OPTIONS
    ${OR_TOOLS_COMPILE_OPTIONS}
  LINK_LIBRARIES
    ortools::ortools_core
    absl::strings
    protobuf::libprotobuf
)
if(MSVC AND BUILD_SHARED_LIBS)
  target_compile_definitions(ortools_scheduling PUBLIC "OR_SCHEDULING_PROTO_DLL=__declspec(dllimport)")
 else()
  target_compile_definitions(ortools_scheduling PUBLIC "OR_SCHEDULING_PROTO_DLL=")
endif()

