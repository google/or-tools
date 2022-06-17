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
"""helpers methods for the cp_model_builder module."""

import numbers
import numpy as np

from ortools.linear_solver import linear_solver_pb2
from ortools.model_builder.python import pywrap_model_builder_helper


# Used by CVXPY.
class ModelSolverHelper(pywrap_model_builder_helper.ModelSolverHelper):

    def Solve(
        self, request: linear_solver_pb2.MPModelRequest
    ) -> linear_solver_pb2.MPSolutionResponse:
        return linear_solver_pb2.MPSolutionResponse.FromString(
            super().solve_serialized_request(request.SerializeToString()))


def is_integral(x):
    """Checks if x has either a number.Integral or a np.integer type."""
    return isinstance(x, numbers.Integral) or isinstance(x, np.integer)


def is_a_number(x):
    """Checks if x has either a number.Number or a np.double type."""
    return isinstance(x, numbers.Number) or isinstance(
        x, np.double) or isinstance(x, np.integer)


def is_zero(x):
    """Checks if the x is 0 or 0.0."""
    return (is_integral(x) and int(x) == 0) or (is_a_number(x) and
                                                float(x) == 0.0)


def is_one(x):
    """Checks if x is 1 or 1.0."""
    return (is_integral(x) and int(x) == 1) or (is_a_number(x) and
                                                float(x) == 1.0)


def is_minus_one(x):
    """Checks if x is -1 or -1.0."""
    return (is_integral(x) and int(x) == -1) or (is_a_number(x) and
                                                 float(x) == -1.0)


def assert_is_a_number(x):
    """Asserts that x is a number and returns it."""
    if not is_a_number(x):
        raise TypeError('Not an number: %s' % x)
    elif is_integral(x):
        return int(x)
    else:
        return float(x)
