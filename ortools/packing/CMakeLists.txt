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

set(NAME ${PROJECT_NAME}_packing)

# Will be merge in libortools.so
#add_library(${NAME} STATIC ${_SRCS})
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  )
target_include_directories(${NAME} PRIVATE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR})
target_link_libraries(${NAME} PRIVATE
  absl::flags
  absl::strings
  ${PROJECT_NAMESPACE}::ortools_proto)
#add_library(${PROJECT_NAMESPACE}::packing ALIAS ${NAME})

# Vector Bin Packing
add_executable(vector_bin_packing)
target_sources(vector_bin_packing PRIVATE "vector_bin_packing_main.cc")
target_include_directories(vector_bin_packing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(vector_bin_packing PRIVATE cxx_std_17)
target_link_libraries(vector_bin_packing PRIVATE ${PROJECT_NAMESPACE}::ortools)

include(GNUInstallDirs)
if(APPLE)
  set_target_properties(vector_bin_packing PROPERTIES INSTALL_RPATH
    "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
elseif(UNIX)
  cmake_path(RELATIVE_PATH CMAKE_INSTALL_FULL_LIBDIR
             BASE_DIRECTORY ${CMAKE_INSTALL_FULL_BINDIR}
             OUTPUT_VARIABLE libdir_relative_path)
  set_target_properties(vector_bin_packing PROPERTIES
                        INSTALL_RPATH "$ORIGIN/${libdir_relative_path}")
endif()

if(BUILD_TESTING)
  add_test(
    NAME cxx_packing_vector_bin_packing
    COMMAND vector_bin_packing --input ${CMAKE_CURRENT_SOURCE_DIR}/testdata/1D__bpp_scholl__bin2data.N2W2B1R0.vbp)
endif()

install(TARGETS vector_bin_packing)
