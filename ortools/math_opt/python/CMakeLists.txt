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

if(BUILD_TESTING)
  file(GLOB PYTHON_SRCS "*_test.py")
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/solve_gurobi_test.py") # need gurobi
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/solve_test.py") # need OSQP
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/callback_test") # segfault
  list(FILTER PYTHON_SRCS EXCLUDE REGEX ".*/mathopt_test") # import test fail
  foreach(FILE_NAME IN LISTS PYTHON_SRCS)
    add_python_test(FILE_NAME ${FILE_NAME})
  endforeach()
endif()
