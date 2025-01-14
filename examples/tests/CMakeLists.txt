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

if(NOT BUILD_EXAMPLES)
  return()
endif()

if(BUILD_CXX_EXAMPLES)
  file(GLOB CXX_SRCS "*.cc")
  foreach(_FULL_FILE_NAME IN LISTS CXX_SRCS)
    get_filename_component(_NAME ${_FULL_FILE_NAME} NAME_WE)
    get_filename_component(_FILE_NAME ${_FULL_FILE_NAME} NAME)
    ortools_cxx_test(
      NAME
        tests_${_NAME}
      SOURCES
        ${_FULL_FILE_NAME}
    )
  endforeach()
endif()

if(BUILD_PYTHON_EXAMPLES)
  file(GLOB PYTHON_SRCS "*.py")
  foreach(FILE_NAME IN LISTS PYTHON_SRCS)
    add_python_example(FILE_NAME ${FILE_NAME})
  endforeach()
endif()

if(BUILD_JAVA_EXAMPLES)
  file(GLOB JAVA_SRCS "*.java")
  foreach(FILE_NAME IN LISTS JAVA_SRCS)
    add_java_example(FILE_NAME ${FILE_NAME})
  endforeach()
endif()

if(BUILD_DOTNET_EXAMPLES)
  file(GLOB DOTNET_SRCS "*.cs")
  foreach(FILE_NAME IN LISTS DOTNET_SRCS)
    add_dotnet_example(FILE_NAME ${FILE_NAME})
  endforeach()
endif()
