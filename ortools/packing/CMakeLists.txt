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

file(GLOB _SRCS "*.h" "*.cc")
list(FILTER _SRCS EXCLUDE REGEX "/vector_bin_packing_main\\.cc")

ortools_cxx_library(
  NAME
    ortools_packing
  TYPE
    SHARED
  SOURCES
    ${_SRCS}
    $<TARGET_OBJECTS:packing_proto>
  COMPILE_DEFINITIONS
    ${OR_TOOLS_COMPILE_DEFINITIONS}
  COMPILE_OPTIONS
    ${OR_TOOLS_COMPILE_OPTIONS}
  LINK_LIBRARIES
    ortools::ortools_core
    absl::flags
    absl::strings
)
if(MSVC AND BUILD_SHARED_LIBS)
  target_compile_definitions(ortools_packing PUBLIC "OR_PACKING_PROTO_DLL=__declspec(dllimport)")
 else()
  target_compile_definitions(ortools_packing PUBLIC "OR_PACKING_PROTO_DLL=")
endif()

# Vector Bin Packing
ortools_cxx_binary(
  NAME
    vector_bin_packing
  SOURCES
    "vector_bin_packing_main.cc"
  LINK_LIBRARIES
    ${PROJECT_NAMESPACE}::ortools
)

if(BUILD_CXX_TESTING)
  add_test(
    NAME cxx_packing_vector_bin_packing
    COMMAND vector_bin_packing --input ${CMAKE_CURRENT_SOURCE_DIR}/testdata/1D__bpp_scholl__bin2data.N2W2B1R0.vbp)
endif()

install(TARGETS vector_bin_packing)
