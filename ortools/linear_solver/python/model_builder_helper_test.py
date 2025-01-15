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

import gzip
import os
import threading

from absl.testing import absltest
import numpy as np
from scipy import sparse

from ortools.linear_solver import linear_solver_pb2
from ortools.linear_solver.python import model_builder_helper


class PywrapModelBuilderHelperTest(absltest.TestCase):

    def test_export_model_proto_to_mps_string(self):
        model = model_builder_helper.ModelBuilderHelper()
        model.set_name("testmodel")
        result = model.export_to_mps_string()
        self.assertIn("testmodel", result)
        self.assertIn("ENDATA", result)

    def test_export_model_proto_to_lp_string(self):
        model = model_builder_helper.ModelBuilderHelper()
        model.set_maximize(True)
        lp_string = model.export_to_lp_string()
        self.assertIn("Maximize", lp_string)

    def test_import_from_mps_string(self):
        model = model_builder_helper.ModelBuilderHelper()
        self.assertTrue(model.import_from_mps_string("NAME testmodel"))
        self.assertEqual(model.name(), "testmodel")

    # ImportFromMpsFile doesn't read from files yet
    def test_import_from_mps_file(self):
        path = os.path.dirname(__file__)
        mps_path = f"{path}/../testdata/maximization.mps"
        model = model_builder_helper.ModelBuilderHelper()
        self.assertTrue(model.import_from_mps_file(mps_path))
        self.assertEqual(model.name(), "SupportedMaximizationProblem")

    def test_import_from_lp_string(self):
        model = model_builder_helper.ModelBuilderHelper()
        model.import_from_lp_string("max:")
        self.assertTrue(model.maximize())

    # TODO(user): Add test_import_from_lp_file after the implementation is fixed

    def test_solve_with_glop(self):
        model = linear_solver_pb2.MPModelProto()
        model.variable.append(
            linear_solver_pb2.MPVariableProto(
                lower_bound=0.0, upper_bound=1.0, objective_coefficient=1.0
            )
        )
        model.maximize = True
        request = linear_solver_pb2.MPModelRequest(
            model=model,
            solver_type=linear_solver_pb2.MPModelRequest.GLOP_LINEAR_PROGRAMMING,
        )
        solver_helper = model_builder_helper.ModelSolverHelper("")
        result = solver_helper.solve_serialized_request(request.SerializeToString())
        response = linear_solver_pb2.MPSolutionResponse().FromString(result)
        self.assertEqual(
            response.status,
            linear_solver_pb2.MPSolverResponseStatus.MPSOLVER_OPTIMAL,
        )
        self.assertAlmostEqual(response.objective_value, 1.0)

    def test_solve_with_glop_direct(self):
        model = model_builder_helper.ModelBuilderHelper()
        self.assertEqual(0, model.add_var())
        model.set_var_lower_bound(0, 0.0)
        model.set_var_upper_bound(0, 1.0)
        model.set_var_objective_coefficient(0, 1.0)
        model.set_maximize(True)

        solver = model_builder_helper.ModelSolverHelper("glop")
        solver.solve(model)
        self.assertEqual(
            solver.status(),
            linear_solver_pb2.MPSolverResponseStatus.MPSOLVER_OPTIMAL,
        )
        self.assertAlmostEqual(solver.objective_value(), 1.0)
        self.assertAlmostEqual(solver.variable_value(0), 1.0)
        values = solver.variable_values()
        self.assertEqual(1, len(values))
        self.assertAlmostEqual(1.0, values[0])

    def test_solve_with_pdlp(self):
        model = linear_solver_pb2.MPModelProto()
        model.variable.append(
            linear_solver_pb2.MPVariableProto(
                lower_bound=0.0, upper_bound=1.0, objective_coefficient=1.0
            )
        )
        model.maximize = True
        request = linear_solver_pb2.MPModelRequest(
            model=model,
            solver_type=linear_solver_pb2.MPModelRequest.PDLP_LINEAR_PROGRAMMING,
        )
        solver_helper = model_builder_helper.ModelSolverHelper("")
        result = solver_helper.solve_serialized_request(request.SerializeToString())
        if result:
            response = linear_solver_pb2.MPSolutionResponse().FromString(result)
            self.assertEqual(
                response.status,
                linear_solver_pb2.MPSolverResponseStatus.MPSOLVER_OPTIMAL,
            )
            self.assertAlmostEqual(response.objective_value, 1.0)
        else:
            print("Solver not supported.")

    # TODO(user): Test the log callback after the implementation is completed.

    def test_interrupt_solve(self):
        # This is an instance that we know Glop won't solve quickly.
        path = os.path.dirname(__file__)
        mps_path = f"{path}/../testdata/large_model.mps.gz"
        with gzip.open(mps_path, "r") as f:
            mps_data = f.read()
        model_helper = model_builder_helper.ModelBuilderHelper()
        self.assertTrue(model_helper.import_from_mps_string(mps_data))
        solver_helper = model_builder_helper.ModelSolverHelper("glop")

        result = []
        solve_thread = threading.Thread(
            target=lambda: result.append(solver_helper.solve(model_helper))
        )
        solve_thread.start()
        self.assertTrue(solver_helper.interrupt_solve())
        solve_thread.join(timeout=30.0)
        self.assertTrue(solver_helper.has_response())
        self.assertEqual(
            solver_helper.status(),
            model_builder_helper.SolveStatus.CANCELLED_BY_USER,
        )

    def test_build_model(self):
        var_lb = np.array([-1.0])
        var_ub = np.array([np.inf])
        obj = np.array([10.0])
        con_lb = np.array([-5.0, -6.0])
        con_ub = np.array([5.0, 6.0])
        constraint_matrix = sparse.csr_matrix(np.array([[1.0], [2.0]]))

        model = model_builder_helper.ModelBuilderHelper()
        model.fill_model_from_sparse_data(
            var_lb, var_ub, obj, con_lb, con_ub, constraint_matrix
        )
        self.assertEqual(1, model.num_variables())
        self.assertEqual(-1.0, model.var_lower_bound(0))
        self.assertEqual(np.inf, model.var_upper_bound(0))
        self.assertEqual(10.0, model.var_objective_coefficient(0))

        self.assertEqual(2, model.num_constraints())
        self.assertEqual(-5.0, model.constraint_lower_bound(0))
        self.assertEqual(5.0, model.constraint_upper_bound(0))
        self.assertEqual([0], model.constraint_var_indices(0))
        self.assertEqual([1.0], model.constraint_coefficients(0))

        self.assertEqual(-6.0, model.constraint_lower_bound(1))
        self.assertEqual(6.0, model.constraint_upper_bound(1))
        self.assertEqual([0], model.constraint_var_indices(1))
        self.assertEqual([2.0], model.constraint_coefficients(1))

        var_array = model.add_var_array([10], 1.0, 5.0, True, "var_")
        self.assertEqual(1, var_array.ndim)
        self.assertEqual(10, var_array.size)
        self.assertEqual((10,), var_array.shape)
        self.assertEqual(model.var_name(var_array[3]), "var_3")

    def test_set_coefficient(self):
        var_lb = np.array([-1.0, -2.0])
        var_ub = np.array([np.inf, np.inf])
        obj = np.array([10.0, 20.0])
        con_lb = np.array([-5.0, -6.0])
        con_ub = np.array([5.0, 6.0])
        constraint_matrix = sparse.csr_matrix(np.array([[1.0, 3.0], [2.0, 4.0]]))

        model = model_builder_helper.ModelBuilderHelper()
        model.fill_model_from_sparse_data(
            var_lb, var_ub, obj, con_lb, con_ub, constraint_matrix
        )
        # Here, we add new variables to test that we are able to set coefficients
        # for variables that are not yet in the constraint.
        var_index1 = model.add_var()
        var_index2 = model.add_var()
        model.set_constraint_coefficient(0, var_index2, 5.0)
        model.set_constraint_coefficient(0, var_index1, 6.0)
        self.assertEqual([1.0, 3.0, 5.0, 6.0], model.constraint_coefficients(0))
        # Here, we test that we are able to set coefficients for variables whose
        # index in the constraint is different from its index in the model.
        model.set_constraint_coefficient(0, var_index2, 7.0)
        self.assertEqual([1.0, 3.0, 7.0, 6.0], model.constraint_coefficients(0))


if __name__ == "__main__":
    absltest.main()
