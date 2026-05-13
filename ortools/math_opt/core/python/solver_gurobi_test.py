#!/usr/bin/env python3
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

import datetime
import math

from absl.testing import absltest
from pybind11_abseil.status import StatusNotOk
from ortools.math_opt import infeasible_subsystem_pb2
from ortools.math_opt import model_pb2
from ortools.math_opt import parameters_pb2
from ortools.math_opt import result_pb2
from ortools.math_opt.core.python import solver
from ortools.math_opt.python.testing import compare_proto


# The model is:
#        x + z <= 4    (c)
#   3 <= x + z         (d)
#   x, y, z in [0, 1]
# The IIS is x upper bound, z upper bound, (d) lower bound
def _simple_infeasible_model() -> model_pb2.ModelProto:
    model = model_pb2.ModelProto()
    model.variables.ids[:] = [0, 1, 2]
    model.variables.lower_bounds[:] = [0.0, 0.0, 0.0]
    model.variables.upper_bounds[:] = [1.0, 1.0, 1.0]
    model.variables.integers[:] = [False, False, False]
    model.linear_constraints.ids[:] = [0, 1]
    model.linear_constraints.lower_bounds[:] = [-math.inf, 3.0]
    model.linear_constraints.upper_bounds[:] = [4.0, math.inf]
    model.linear_constraint_matrix.row_ids[:] = [0, 0, 1, 1]
    model.linear_constraint_matrix.column_ids[:] = [0, 2, 0, 2]
    model.linear_constraint_matrix.coefficients[:] = [1.0, 1.0, 1.0, 1.0]
    return model


# The model is
#  2*x + 2*y + 2*z >= 3.0
#  x + y <= 1
#  y + z <= 1
#  x + z <= 1
#  x, y, z in {0, 1}
def _nontrivial_infeasible_model() -> model_pb2.ModelProto:
    model = model_pb2.ModelProto()
    model.variables.ids[:] = [0, 1, 2]
    model.variables.lower_bounds[:] = [0.0, 0.0, 0.0]
    model.variables.upper_bounds[:] = [1.0, 1.0, 1.0]
    model.variables.integers[:] = [True, True, True]
    model.linear_constraints.ids[:] = [0, 1, 2, 3]
    model.linear_constraints.lower_bounds[:] = [
        3.0,
        -math.inf,
        -math.inf,
        -math.inf,
    ]
    model.linear_constraints.upper_bounds[:] = [math.inf, 1.0, 1.0, 1.0]
    model.linear_constraint_matrix.row_ids[:] = [0, 0, 0, 1, 1, 2, 2, 3, 3]
    model.linear_constraint_matrix.column_ids[:] = [0, 1, 2, 0, 1, 1, 2, 0, 2]
    model.linear_constraint_matrix.coefficients[:] = [
        2.0,
        2.0,
        2.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
    ]
    return model


def _expected_iis_success() -> (
    infeasible_subsystem_pb2.ComputeInfeasibleSubsystemResultProto
):
    expected = infeasible_subsystem_pb2.ComputeInfeasibleSubsystemResultProto(
        is_minimal=True, feasibility=result_pb2.FEASIBILITY_STATUS_INFEASIBLE
    )
    expected.infeasible_subsystem.variable_bounds[0].upper = True
    expected.infeasible_subsystem.variable_bounds[2].upper = True
    expected.infeasible_subsystem.linear_constraints[1].lower = True
    return expected


class PybindComputeInfeasibleSubsystemTest(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):

    def test_compute_infeasible_subsystem_infeasible(self) -> None:
        iis_result = solver.compute_infeasible_subsystem(
            _simple_infeasible_model(),
            parameters_pb2.SOLVER_TYPE_GUROBI,
            parameters_pb2.SolverInitializerProto(),
            parameters_pb2.SolveParametersProto(),
            None,
            None,
        )
        self.assert_protos_equiv(iis_result, _expected_iis_success())

    def test_compute_infeasible_subsystem_infeasible_uninterrupted(self) -> None:
        interrupter = solver.SolveInterrupter()
        iis_result = solver.compute_infeasible_subsystem(
            _simple_infeasible_model(),
            parameters_pb2.SOLVER_TYPE_GUROBI,
            parameters_pb2.SolverInitializerProto(),
            parameters_pb2.SolveParametersProto(),
            None,
            interrupter,
        )
        self.assert_protos_equiv(iis_result, _expected_iis_success())

    def test_compute_infeasible_subsystem_interrupted(self) -> None:
        interrupter = solver.SolveInterrupter()
        interrupter.interrupt()
        iis_result = solver.compute_infeasible_subsystem(
            _nontrivial_infeasible_model(),
            parameters_pb2.SOLVER_TYPE_GUROBI,
            parameters_pb2.SolverInitializerProto(),
            parameters_pb2.SolveParametersProto(),
            None,
            interrupter,
        )
        expected = infeasible_subsystem_pb2.ComputeInfeasibleSubsystemResultProto(
            feasibility=result_pb2.FEASIBILITY_STATUS_UNDETERMINED
        )
        self.assert_protos_equiv(iis_result, expected)

    def test_compute_infeasible_subsystem_time_limit(self) -> None:
        params = parameters_pb2.SolveParametersProto()
        params.time_limit.FromTimedelta(datetime.timedelta(seconds=0.0))
        iis_result = solver.compute_infeasible_subsystem(
            _nontrivial_infeasible_model(),
            parameters_pb2.SOLVER_TYPE_GUROBI,
            parameters_pb2.SolverInitializerProto(),
            params,
            None,
            None,
        )
        expected = infeasible_subsystem_pb2.ComputeInfeasibleSubsystemResultProto(
            feasibility=result_pb2.FEASIBILITY_STATUS_UNDETERMINED
        )
        self.assert_protos_equiv(iis_result, expected)

    def test_compute_infeasible_subsystem_infeasible_message_cb(self) -> None:
        messages = []
        iis_result = solver.compute_infeasible_subsystem(
            _simple_infeasible_model(),
            parameters_pb2.SOLVER_TYPE_GUROBI,
            parameters_pb2.SolverInitializerProto(),
            parameters_pb2.SolveParametersProto(),
            messages.extend,
            None,
        )
        self.assert_protos_equiv(iis_result, _expected_iis_success())
        self.assertIn("IIS computed", "\n".join(messages))

    def test_compute_infeasible_subsystem_error_wrong_solver(self) -> None:
        with self.assertRaisesRegex(StatusNotOk, "SOLVER_TYPE_GLPK is not registered"):
            solver.compute_infeasible_subsystem(
                _simple_infeasible_model(),
                parameters_pb2.SOLVER_TYPE_GLPK,
                parameters_pb2.SolverInitializerProto(),
                parameters_pb2.SolveParametersProto(),
                None,
                None,
            )


if __name__ == "__main__":
    absltest.main()
