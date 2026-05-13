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

from ortools.service.v1 import optimization_pb2
from ortools.service.v1.mathopt import model_pb2 as api_model_pb2
from ortools.service.v1.mathopt import parameters_pb2 as api_parameters_pb2
from ortools.service.v1.mathopt import result_pb2 as api_result_pb2
from ortools.service.v1.mathopt import solution_pb2 as api_solution_pb2
from ortools.service.v1.mathopt import solver_resources_pb2 as api_solver_resources_pb2
from ortools.service.v1.mathopt import (
    sparse_containers_pb2 as api_sparse_containers_pb2,
)
from absl.testing import absltest
from ortools.math_opt import parameters_pb2
from ortools.math_opt import result_pb2
from ortools.math_opt import rpc_pb2
from ortools.math_opt import solution_pb2
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import mathopt
from ortools.math_opt.python.ipc import proto_converter
from ortools.math_opt.python.testing import compare_proto


def _simple_request() -> rpc_pb2.SolveRequest:
    mod = mathopt.Model(name="test_mod")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    mod.maximize(x - y)
    resources = mathopt.SolverResources(cpu=2.0, ram=1024 * 1024 * 1024)
    params = mathopt.SolveParameters(threads=2)

    request = rpc_pb2.SolveRequest(
        solver_type=parameters_pb2.SOLVER_TYPE_GSCIP,
        model=mod.export_model(),
        resources=resources.to_proto(),
        parameters=params.to_proto(),
    )
    return request


class ProtoConverterTest(compare_proto.MathOptProtoAssertions, absltest.TestCase):

    def test_convert_request(self):
        request = _simple_request()
        expected = optimization_pb2.SolveMathOptModelRequest(
            solver_type=api_parameters_pb2.SOLVER_TYPE_GSCIP,
            model=api_model_pb2.ModelProto(
                name="test_mod",
                variables=api_model_pb2.VariablesProto(
                    ids=[0, 1],
                    lower_bounds=[0, 0],
                    upper_bounds=[1, 1],
                    integers=[True, True],
                    names=["x", "y"],
                ),
                objective=api_model_pb2.ObjectiveProto(
                    maximize=True,
                    linear_coefficients=api_sparse_containers_pb2.SparseDoubleVectorProto(
                        ids=[0, 1],
                        values=[1.0, -1.0],
                    ),
                ),
            ),
            resources=api_solver_resources_pb2.SolverResourcesProto(
                cpu=2.0, ram=1024 * 1024 * 1024
            ),
            parameters=api_parameters_pb2.SolveParametersProto(threads=2),
        )

        self.assert_protos_equal(proto_converter.convert_request(request), expected)

    def test_initializer_is_not_supported(self):
        request = _simple_request()
        request.initializer.gurobi.isv_key.name = "test_name_key"

        with self.assertRaisesRegex(ValueError, "initializer is not supported"):
            proto_converter.convert_request(request)

    def test_solver_parameters_are_not_supported(self):
        request = _simple_request()
        request.parameters.osqp.rho = 1.0

        with self.assertRaisesRegex(ValueError, "SolveParameters.osqp not supported"):
            proto_converter.convert_request(request)

    def test_convert_response(self):
        api_response = optimization_pb2.SolveMathOptModelResponse()
        result = api_response.result
        result.termination.reason = api_result_pb2.TERMINATION_REASON_OPTIMAL
        result.termination.problem_status.primal_status = (
            api_result_pb2.FEASIBILITY_STATUS_FEASIBLE
        )
        result.termination.problem_status.dual_status = (
            api_result_pb2.FEASIBILITY_STATUS_FEASIBLE
        )
        result.termination.objective_bounds.primal_bound = 1.0
        result.termination.objective_bounds.dual_bound = 1.0
        result.solutions.append(
            api_solution_pb2.SolutionProto(
                primal_solution=api_solution_pb2.PrimalSolutionProto(
                    variable_values=api_sparse_containers_pb2.SparseDoubleVectorProto(
                        ids=[0, 1], values=[1.0, 0.0]
                    )
                )
            )
        )
        expected_response = rpc_pb2.SolveResponse()
        expected_result = expected_response.result
        expected_result.termination.reason = result_pb2.TERMINATION_REASON_OPTIMAL
        expected_result.termination.problem_status.primal_status = (
            result_pb2.FEASIBILITY_STATUS_FEASIBLE
        )
        expected_result.termination.problem_status.dual_status = (
            result_pb2.FEASIBILITY_STATUS_FEASIBLE
        )
        expected_result.termination.objective_bounds.primal_bound = 1.0
        expected_result.termination.objective_bounds.dual_bound = 1.0
        expected_result.solutions.append(
            solution_pb2.SolutionProto(
                primal_solution=solution_pb2.PrimalSolutionProto(
                    variable_values=sparse_containers_pb2.SparseDoubleVectorProto(
                        ids=[0, 1], values=[1.0, 0.0]
                    )
                )
            )
        )

        self.assert_protos_equal(
            proto_converter.convert_response(api_response), expected_response
        )


if __name__ == "__main__":
    absltest.main()
