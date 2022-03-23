# Copyright 2010-2021 Google LLC
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
"""A Python wrapper for model_builder_helper.

TODO(user): Remove this file and rename pywrap_model_builder_helper to
  model_builder_helper when the cvxpy interface has been switched to the new
  API.
"""

from ortools.linear_solver import linear_solver_pb2
from ortools.model_builder.python import pywrap_model_builder_helper


# Used by CVXPY.
class ModelSolverHelper(pywrap_model_builder_helper.ModelSolverHelper):

    def Solve(
        self, request: linear_solver_pb2.MPModelRequest
    ) -> linear_solver_pb2.MPSolutionResponse:
        return linear_solver_pb2.MPSolutionResponse.FromString(
            super().SolveSerialized(request.SerializeToString()))
