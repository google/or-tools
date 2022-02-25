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
"""

from ortools.linear_solver import linear_solver_pb2
from ortools.model_builder.python import pywrap_model_builder_helper

MPModelExportOptions = pywrap_model_builder_helper.MPModelExportOptions


def BuildModel(variable_lower_bounds, variable_upper_bounds,
               objective_coefficients, constraint_lower_bounds,
               constraint_upper_bounds,
               constraint_matrix) -> linear_solver_pb2.MPModelProto:
    """Returns a MPModelProto representing the given linear programming problem.

  Consider the linear programming problem:
    minimize: c'x
    subject to: con_lb <= A * x <= con_ub
    var_lb <= x <= var_ub.

  Args:
    variable_lower_bounds: The vector var_lb. Entries may be -infinity.
    variable_upper_bounds: The vector var_ub. Entries may be infinity.
    objective_coefficients: The vector c.
    constraint_lower_bounds: The vector con_lb. Entries may be -infinity.
    constraint_upper_bounds: The vector con_ub. Entries may be infinity.
    constraint_matrix: The matrix A (Numpy Sparse matrix.)

  Returns:
    A MPModelProto encoding the linear programming problem. Modify the returned
    model to set additional properties like maximization or integrality.

  Raises:
    ValueError: Inconsistent dimensions.
  """
    return linear_solver_pb2.MPModelProto.FromString(
        pywrap_model_builder_helper.BuildModel(variable_lower_bounds,
                                               variable_upper_bounds,
                                               objective_coefficients,
                                               constraint_lower_bounds,
                                               constraint_upper_bounds,
                                               constraint_matrix))


def ExportModelProtoToMpsString(
    input_model: linear_solver_pb2.MPModelProto,
    options: MPModelExportOptions = MPModelExportOptions()
) -> str:
    return pywrap_model_builder_helper.ExportModelProtoToMpsString(
        input_model.SerializeToString(), options)


def ExportModelProtoToLpString(
    input_model: linear_solver_pb2.MPModelProto,
    options: MPModelExportOptions = MPModelExportOptions()
) -> str:
    return pywrap_model_builder_helper.ExportModelProtoToLpString(
        input_model.SerializeToString(), options)


def ImportFromMpsString(mps_string: str) -> linear_solver_pb2.MPModelProto:
    model_string = pywrap_model_builder_helper.ImportFromMpsString(mps_string)
    return linear_solver_pb2.MPModelProto.FromString(model_string)


def ImportFromMpsFile(mps_file: str) -> linear_solver_pb2.MPModelProto:
    model_string = pywrap_model_builder_helper.ImportFromMpsFile(mps_file)
    return linear_solver_pb2.MPModelProto.FromString(model_string)


def ImportFromLpString(mps_string: str) -> linear_solver_pb2.MPModelProto:
    model_string = pywrap_model_builder_helper.ImportFromLpString(mps_string)
    return linear_solver_pb2.MPModelProto.FromString(model_string)


def ImportFromLpFile(mps_file: str) -> linear_solver_pb2.MPModelProto:
    model_string = pywrap_model_builder_helper.ImportFromLpFile(mps_file)
    return linear_solver_pb2.MPModelProto.FromString(model_string)


class ModelSolverHelper(pywrap_model_builder_helper.ModelSolverHelper):

    def Solve(
        self, request: linear_solver_pb2.MPModelRequest
    ) -> linear_solver_pb2.MPSolutionResponse:
        return linear_solver_pb2.MPSolutionResponse.FromString(super().Solve(
            request.SerializeToString()))
