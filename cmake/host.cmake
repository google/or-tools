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

if(NOT CMAKE_CROSSCOMPILING)
  set(PROTOC_PRG protobuf::protoc)
  return()
endif()

message(STATUS "Subproject: HostTools...")

#configure_file(
# ${CMAKE_CURRENT_SOURCE_DIR}/host.CMakeLists.txt
#  ${CMAKE_CURRENT_BINARY_DIR}/host_tools/CMakeLists.txt)
#
#execute_process(
#  COMMAND ${CMAKE_COMMAND} -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -G "${CMAKE_GENERATOR}" .
#  RESULT_VARIABLE result
#  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/host_tools)
#if(result)
#  message(FATAL_ERROR "CMake step for host tools failed: ${result}")
#endif()
#execute_process(
#  COMMAND ${CMAKE_COMMAND} --build .
#  RESULT_VARIABLE result
#  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/host_tools)
#if(result)
#  message(FATAL_ERROR "Build step for host tools failed: ${result}")
#endif()

#file(COPY
# ${CMAKE_CURRENT_SOURCE_DIR}/cmake/host.CMakeLists.txt
#  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/host_tools)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/host.CMakeLists.txt
  ${CMAKE_CURRENT_BINARY_DIR}/host_tools/CMakeLists.txt
  COPYONLY)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/host_tools
  #COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/host.CMakeLists.txt CMakeLists.txt
  COMMAND ${CMAKE_COMMAND} -E remove_directory build
  COMMAND ${CMAKE_COMMAND} -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DCMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH=${CMAKE_CURRENT_BINARY_DIR}/host_tools/bin
  COMMAND ${CMAKE_COMMAND} --build build --config Release -v
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/host_tools
)

add_custom_target(host_tools
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/host_tools
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_executable(host_protoc IMPORTED GLOBAL)
set_target_properties(host_protoc PROPERTIES
  IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/host_tools/bin/protoc)

add_dependencies(host_protoc host_tools)
set(PROTOC_PRG host_protoc)

message(STATUS "Subproject: HostTools...DONE")
