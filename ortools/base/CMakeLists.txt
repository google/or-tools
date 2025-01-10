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
list(FILTER _SRCS EXCLUDE REGEX ".*/.*_test.cc")
list(FILTER _SRCS EXCLUDE REGEX "/status_matchers\\.h")
set(NAME ${PROJECT_NAME}_base)

# Will be merge in libortools.so
#add_library(${NAME} STATIC ${_SRCS})
add_library(${NAME} OBJECT ${_SRCS})
set_target_properties(${NAME} PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  )
target_compile_definitions(${NAME} PRIVATE
  -DOR_TOOLS_MAJOR=${PROJECT_VERSION_MAJOR}
  -DOR_TOOLS_MINOR=${PROJECT_VERSION_MINOR}
  -DOR_TOOLS_PATCH=${PROJECT_VERSION_PATCH})
if(MSVC)
  target_compile_definitions(${NAME} PRIVATE -DOR_TOOLS_EXPORTS)
endif()
target_include_directories(${NAME} PRIVATE
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR})
target_link_libraries(${NAME} PRIVATE
  ZLIB::ZLIB
  absl::base
  absl::strings
  absl::str_format
  protobuf::libprotobuf
  ${PROJECT_NAMESPACE}::ortools_proto)
#add_library(${PROJECT_NAMESPACE}::base ALIAS ${NAME})
