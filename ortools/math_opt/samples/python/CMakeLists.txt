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

if(BUILD_PYTHON_SAMPLES)
  file(GLOB PYTHON_SRCS "*.py")
  list(FILTER PYTHON_SRCS EXCLUDE REGEX "remote_solve.py$") # need ortools.math_opt.python.ipc
  list(FILTER PYTHON_SRCS EXCLUDE REGEX "tsp.py$") # need gurobi
  list(FILTER PYTHON_SRCS EXCLUDE REGEX "hierarchical_objectives.py$") # need gurobi
  list(FILTER PYTHON_SRCS EXCLUDE REGEX "smallest_circle.py$") # need gurobi
  list(FILTER PYTHON_SRCS EXCLUDE REGEX "facility_lp_benders.py$") # too long
  list(FILTER PYTHON_SRCS EXCLUDE REGEX "facility_location.py$") # timeout

  if(NOT USE_SCIP)
    list(FILTER PYTHON_SRCS EXCLUDE REGEX "basic_example.py$")
    list(FILTER PYTHON_SRCS EXCLUDE REGEX "integer_programming.py$")
    list(FILTER PYTHON_SRCS EXCLUDE REGEX "time_indexed_scheduling.py$")
  endif()

  foreach(SAMPLE IN LISTS PYTHON_SRCS)
    add_python_sample(
      FILE_NAME ${SAMPLE}
      COMPONENT_NAME math_opt)
  endforeach()
endif()
