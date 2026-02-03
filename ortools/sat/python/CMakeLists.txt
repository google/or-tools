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

# gen_proto_builder_pybind11 code generator.
add_executable(gen_proto_builder_pybind11)
target_sources(gen_proto_builder_pybind11 PRIVATE "gen_proto_builder_pybind11.cc")
target_include_directories(gen_proto_builder_pybind11 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(gen_proto_builder_pybind11 PRIVATE cxx_std_17)
target_link_libraries(gen_proto_builder_pybind11 PRIVATE
  absl::flags_commandlineflag
  absl::flags_parse
  absl::flags_usage
  absl::die_if_null
  absl::str_format
  protobuf::libprotobuf
  ${PROJECT_NAMESPACE}::ortools_proto
  ${PROJECT_NAMESPACE}::util_python_wrappers)

include(GNUInstallDirs)
if(APPLE)
  set_target_properties(gen_proto_builder_pybind11 PROPERTIES INSTALL_RPATH
    "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
elseif(UNIX)
  cmake_path(RELATIVE_PATH CMAKE_INSTALL_FULL_LIBDIR
             BASE_DIRECTORY ${CMAKE_INSTALL_FULL_BINDIR}
             OUTPUT_VARIABLE libdir_relative_path)
  set_target_properties(gen_proto_builder_pybind11 PROPERTIES
                        INSTALL_RPATH "$ORIGIN/${libdir_relative_path}")
endif()

install(TARGETS gen_proto_builder_pybind11)

add_custom_command(
     OUTPUT proto_builder_pybind11.h
     COMMAND gen_proto_builder_pybind11 > proto_builder_pybind11.h
     COMMENT "Generate C++ proto_builder_pybind11.h"
     VERBATIM)

pybind11_add_module(cp_model_helper_pybind11 MODULE cp_model_helper.cc proto_builder_pybind11.h)
set_target_properties(cp_model_helper_pybind11 PROPERTIES
  LIBRARY_OUTPUT_NAME "cp_model_helper")

# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(cp_model_helper_pybind11 PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs")
elseif(UNIX)
  set_target_properties(cp_model_helper_pybind11 PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs")
endif()
target_link_libraries(cp_model_helper_pybind11 PRIVATE
  ${PROJECT_NAMESPACE}::ortools
  protobuf::libprotobuf
  )

target_include_directories(cp_model_helper_pybind11 PRIVATE ${protobuf_SOURCE_DIR})
add_library(${PROJECT_NAMESPACE}::cp_model_helper_pybind11 ALIAS cp_model_helper_pybind11)

if(BUILD_TESTING)
  file(GLOB PYTHON_SRCS "*_test.py")
  foreach(FILE_NAME IN LISTS PYTHON_SRCS)
    add_python_test(FILE_NAME ${FILE_NAME})
  endforeach()
endif()
