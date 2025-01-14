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

if(NOT BUILD_PYTHON_EXAMPLES)
  return()
endif()

file(GLOB PYTHON_SRCS "*.py")
# Remove too long examples
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/line_balancing_sat.py") # need input file
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/bus_driver_scheduling_sat.py") # too long
list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/cvrptw_plot.py") # depend on numpy

foreach(FILE_NAME IN LISTS PYTHON_SRCS)
  add_python_example(FILE_NAME ${FILE_NAME})
endforeach()
