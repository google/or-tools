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

set(WRAPPERS_NAME sat_python_wrappers)

add_library(${WRAPPERS_NAME} OBJECT wrappers.h wrappers.cc)
set_target_properties(${WRAPPERS_NAME} PROPERTIES
  POSITION_INDEPENDENT_CODE ON)
target_include_directories(${WRAPPERS_NAME} PUBLIC
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR})
target_link_libraries(${WRAPPERS_NAME} PUBLIC
  absl::memory
  absl::synchronization
  absl::str_format
  protobuf::libprotobuf)
add_library(${PROJECT_NAMESPACE}::${WRAPPERS_NAME} ALIAS ${WRAPPERS_NAME})  

# gen_cp_model_builder_pybind code generator.
add_executable(gen_cp_model_builder_pybind)
target_sources(gen_cp_model_builder_pybind PRIVATE "gen_cp_model_builder_pybind.cc")
target_include_directories(gen_cp_model_builder_pybind PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(gen_cp_model_builder_pybind PRIVATE cxx_std_17)
target_link_libraries(gen_cp_model_builder_pybind PRIVATE
  absl::flags_commandlineflag
  absl::flags_parse
  absl::flags_usage
  absl::die_if_null
  absl::str_format
  protobuf::libprotobuf
  ${PROJECT_NAMESPACE}::ortools_proto
  ${PROJECT_NAMESPACE}::${WRAPPERS_NAME})

include(GNUInstallDirs)
if(APPLE)
  set_target_properties(gen_cp_model_builder_pybind PROPERTIES INSTALL_RPATH
    "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
elseif(UNIX)
  cmake_path(RELATIVE_PATH CMAKE_INSTALL_FULL_LIBDIR
             BASE_DIRECTORY ${CMAKE_INSTALL_FULL_BINDIR}
             OUTPUT_VARIABLE libdir_relative_path)
  set_target_properties(gen_cp_model_builder_pybind PROPERTIES
                        INSTALL_RPATH "$ORIGIN/${libdir_relative_path}")
endif()

install(TARGETS gen_cp_model_builder_pybind)

add_custom_command(
     OUTPUT cp_model_builder_pybind.cc
     COMMAND gen_cp_model_builder_pybind > cp_model_builder_pybind.cc
     COMMENT "Generate C++ cp_model_builder_pybind.cc"
     VERBATIM)

# gen_sat_parameters_builder_pybind code generator.
add_executable(gen_sat_parameters_builder_pybind)
target_sources(gen_sat_parameters_builder_pybind PRIVATE "gen_sat_parameters_builder_pybind.cc")
target_include_directories(gen_sat_parameters_builder_pybind PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(gen_sat_parameters_builder_pybind PRIVATE cxx_std_17)
target_link_libraries(gen_sat_parameters_builder_pybind PRIVATE
  absl::flags_commandlineflag
  absl::flags_parse
  absl::flags_usage
  absl::die_if_null
  absl::str_format
  protobuf::libprotobuf
  ${PROJECT_NAMESPACE}::ortools_proto
  ${PROJECT_NAMESPACE}::${WRAPPERS_NAME})

include(GNUInstallDirs)
if(APPLE)
  set_target_properties(gen_sat_parameters_builder_pybind PROPERTIES INSTALL_RPATH
    "@loader_path/../${CMAKE_INSTALL_LIBDIR};@loader_path")
elseif(UNIX)
  cmake_path(RELATIVE_PATH CMAKE_INSTALL_FULL_LIBDIR
             BASE_DIRECTORY ${CMAKE_INSTALL_FULL_BINDIR}
             OUTPUT_VARIABLE libdir_relative_path)
  set_target_properties(gen_sat_parameters_builder_pybind PROPERTIES
                        INSTALL_RPATH "$ORIGIN/${libdir_relative_path}")
endif()

install(TARGETS gen_sat_parameters_builder_pybind)

add_custom_command(
     OUTPUT sat_parameters_builder_pybind.cc
     COMMAND gen_sat_parameters_builder_pybind > sat_parameters_builder_pybind.cc
     COMMENT "Generate C++ sat_parameters_builder_pybind.cc"
     VERBATIM)

# Generate both pybind extensins (cp_model_builder and sat_parameters_builder).
pybind11_add_module(cp_model_builder_pybind MODULE cp_model_builder_pybind.cc)
set_target_properties(cp_model_builder_pybind PROPERTIES
  LIBRARY_OUTPUT_NAME "cp_model_builder")

# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(cp_model_builder_pybind PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs")
elseif(UNIX)
  set_target_properties(cp_model_builder_pybind PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs")
endif()
target_link_libraries(cp_model_builder_pybind PRIVATE
  ${PROJECT_NAMESPACE}::ortools
  protobuf::libprotobuf)

target_include_directories(cp_model_builder_pybind PRIVATE ${protobuf_SOURCE_DIR})
add_library(${PROJECT_NAMESPACE}::cp_model_builder_pybind ALIAS cp_model_builder_pybind)

pybind11_add_module(sat_parameters_builder_pybind MODULE sat_parameters_builder_pybind.cc)
set_target_properties(sat_parameters_builder_pybind PROPERTIES
  LIBRARY_OUTPUT_NAME "sat_parameters_builder")

# note: macOS is APPLE and also UNIX !
if(APPLE)
  set_target_properties(sat_parameters_builder_pybind PROPERTIES
    SUFFIX ".so"
    INSTALL_RPATH "@loader_path;@loader_path/../../../${PYTHON_PROJECT}/.libs")
elseif(UNIX)
  set_target_properties(sat_parameters_builder_pybind PROPERTIES
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../../../${PYTHON_PROJECT}/.libs")
endif()
target_link_libraries(sat_parameters_builder_pybind PRIVATE
  ${PROJECT_NAMESPACE}::ortools
  protobuf::libprotobuf)

target_include_directories(sat_parameters_builder_pybind PRIVATE ${protobuf_SOURCE_DIR})
add_library(${PROJECT_NAMESPACE}::sat_parameters_builder_pybind ALIAS sat_parameters_builder_pybind)

pybind11_add_module(cp_model_helper_pybind11 MODULE cp_model_helper.cc)
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

