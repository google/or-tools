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

if(NOT BUILD_SAMPLES)
  return()
endif()

if(BUILD_CXX_SAMPLES)
  file(GLOB CXX_SRCS "*.cc")
  foreach(SAMPLE IN LISTS CXX_SRCS)
    add_cxx_sample(FILE_NAME ${SAMPLE})
  endforeach()
endif()

if(BUILD_PYTHON_SAMPLES)
  file(GLOB PYTHON_SRCS "*.py")
  foreach(SAMPLE IN LISTS PYTHON_SRCS)
    add_python_sample(FILE_NAME ${SAMPLE})
  endforeach()
endif()

if(BUILD_JAVA_SAMPLES)
  file(GLOB JAVA_SRCS "*.java")
  foreach(SAMPLE IN LISTS JAVA_SRCS)
    add_java_sample(FILE_NAME ${SAMPLE})
  endforeach()
endif()

if(BUILD_DOTNET_SAMPLES)
  file(GLOB DOTNET_SRCS "*.cs")
  foreach(SAMPLE IN LISTS DOTNET_SRCS)
    add_dotnet_sample(FILE_NAME ${SAMPLE})
  endforeach()
endif()
