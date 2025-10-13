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

"""Tests for ortools.pdlp.python.quadratic_program."""

from absl.testing import absltest
import numpy as np
import scipy.sparse

from ortools.pdlp import solve_log_pb2
from ortools.pdlp import solvers_pb2
from ortools.pdlp.python import pdlp
from ortools.linear_solver import linear_solver_pb2


def small_proto_lp():
    # min -2y
    # s.t. x + y <= 1
    #      x, y >= 0
    return linear_solver_pb2.MPModelProto(
        # Defaults are specified for the benefit of assertProto2Equal.
        maximize=False,
        objective_offset=0.0,
        variable=[
            linear_solver_pb2.MPVariableProto(
                lower_bound=0, upper_bound=np.inf, objective_coefficient=0, name="x"
            ),
            linear_solver_pb2.MPVariableProto(
                lower_bound=0, upper_bound=np.inf, objective_coefficient=-2, name="y"
            ),
        ],
        constraint=[
            linear_solver_pb2.MPConstraintProto(
                var_index=[0, 1], coefficient=[1, 1], lower_bound=-np.inf, upper_bound=1
            )
        ],
    )


def small_proto_qp():
    # min 2 x*x
    # s.t. x + y <= 1
    #      x, y >= 0
    return linear_solver_pb2.MPModelProto(
        # Defaults are specified for the benefit of assertProto2Equal.
        maximize=False,
        objective_offset=0.0,
        variable=[
            linear_solver_pb2.MPVariableProto(
                lower_bound=0, upper_bound=np.inf, objective_coefficient=0, name="x"
            ),
            linear_solver_pb2.MPVariableProto(
                lower_bound=0, upper_bound=np.inf, objective_coefficient=0, name="y"
            ),
        ],
        constraint=[
            linear_solver_pb2.MPConstraintProto(
                var_index=[0, 1], coefficient=[1, 1], lower_bound=-np.inf, upper_bound=1
            )
        ],
        quadratic_objective=linear_solver_pb2.MPQuadraticObjective(
            qvar1_index=[0], qvar2_index=[0], coefficient=[2]
        ),
    )


class QuadraticProgramTest(absltest.TestCase):

    def test_validate_quadratic_program_dimensions_for_empty_qp(self):
        qp = pdlp.QuadraticProgram()
        qp.resize_and_initialize(3, 2)
        pdlp.validate_quadratic_program_dimensions(qp)
        self.assertTrue(pdlp.is_linear_program(qp))

    def test_converts_from_tiny_mpmodel_lp(self):
        lp_proto = small_proto_lp()
        qp = pdlp.qp_from_mpmodel_proto(lp_proto, relax_integer_variables=False)
        pdlp.validate_quadratic_program_dimensions(qp)
        self.assertTrue(pdlp.is_linear_program(qp))
        self.assertSameElements(qp.objective_vector, [0, -2])

    def test_converts_from_tiny_mpmodel_qp(self):
        qp_proto = small_proto_qp()
        qp = pdlp.qp_from_mpmodel_proto(qp_proto, relax_integer_variables=False)
        pdlp.validate_quadratic_program_dimensions(qp)
        self.assertFalse(pdlp.is_linear_program(qp))
        self.assertSameElements(qp.objective_vector, [0, 0])

    def test_build_lp(self):
        qp = pdlp.QuadraticProgram()
        qp.objective_vector = [0, -2]
        qp.constraint_matrix = scipy.sparse.csr_matrix(np.array([[1.0, 1.0]]))
        qp.constraint_lower_bounds = [-np.inf]
        qp.constraint_upper_bounds = [1.0]
        qp.variable_lower_bounds = [0.0, 0.0]
        qp.variable_upper_bounds = [np.inf, np.inf]
        qp.variable_names = ["x", "y"]
        self.assertEqual(
            pdlp.qp_to_mpmodel_proto(qp),
            small_proto_lp(),
        )

    def test_build_qp(self):
        qp = pdlp.QuadraticProgram()
        qp.objective_vector = [0, 0]
        qp.constraint_matrix = scipy.sparse.csr_matrix(np.array([[1.0, 1.0]]))
        qp.set_objective_matrix_diagonal([4.0])
        qp.constraint_lower_bounds = [-np.inf]
        qp.constraint_upper_bounds = [1.0]
        qp.variable_lower_bounds = [0.0, 0.0]
        qp.variable_upper_bounds = [np.inf, np.inf]
        qp.variable_names = ["x", "y"]
        self.assertEqual(
            pdlp.qp_to_mpmodel_proto(qp),
            small_proto_qp(),
        )


def tiny_lp():
    """Returns a small test LP.

      The LP:
        min 5 x_1 + 2 x_2 + x_3 +   x_4 - 14 s.t.
        2 x_1 +   x_2 + x_3 + 2 x_4  = 12
        x_1 +         x_3         >=  7
                      x_3 -   x_4 >=  1
      0 <= x_1 <= 2
      0 <= x_2 <= 4
      0 <= x_3 <= 6
      0 <= x_4 <= 3

    Optimum solutions:
      Primal: x_1 = 1, x_2 = 0, x_3 = 6, x_4 = 2. Value: 5 + 0 + 6 + 2 - 14 = -1.
      Dual: [0.5, 4.0, 0.0]  Value: 6 + 28 - 3.5*6 - 14 = -1
      Reduced costs: [0.0, 1.5, -3.5, 0.0]
    """
    qp = pdlp.QuadraticProgram()
    qp.objective_offset = -14
    qp.objective_vector = [5, 2, 1, 1]
    qp.constraint_lower_bounds = [12, 7, 1]
    qp.constraint_upper_bounds = [12, np.inf, np.inf]
    qp.variable_lower_bounds = np.zeros(4)
    qp.variable_upper_bounds = [2, 4, 6, 3]
    constraint_matrix = np.array([[2, 1, 1, 2], [1, 0, 1, 0], [0, 0, 1, -1]])
    qp.constraint_matrix = scipy.sparse.csr_matrix(constraint_matrix)
    return qp


def small_lp():
    """Returns a small LP with all 4 patterns lower and upper bounds.

    min 5.5 x_0 - 2 x_1 - x_2 +   x_3 - 14 s.t.
        2 x_0 +     x_1 +   x_2 + 2 x_3  = 12
          x_0 +             x_2          <=  7
        4 x_0                            >=  -4
       -1 <=            1.5 x_2 -   x_3  <= 1
      -infinity <= x_0 <= infinity
             -2 <= x_1 <= infinity
      -infinity <= x_2 <= 6
            2.5 <= x_3 <= 3.5

    Optimal solutions:
     Primal: [-1, 8, 1, 2.5]
     Dual: [-2, 0, 2.375, 2.0/3]
     Value: -5.5 - 16 -1 + 2.5 - 14 = -34
    """
    qp = pdlp.QuadraticProgram()
    qp.objective_offset = -14
    qp.objective_vector = [5.5, -2, -1, 1]
    qp.constraint_lower_bounds = [12, -np.inf, -4, -1]
    qp.constraint_upper_bounds = [12, 7, np.inf, 1]
    qp.variable_lower_bounds = [-np.inf, -2, -np.inf, 2.5]
    qp.variable_upper_bounds = [np.inf, np.inf, 6, 3.5]
    constraint_matrix = np.array(
        [[2, 1, 1, 2], [1, 0, 1, 0], [4, 0, 0, 0], [0, 0, 1.5, -1]]
    )
    qp.constraint_matrix = scipy.sparse.csr_matrix(constraint_matrix)
    return qp


class PrimalDualHybridGradientTest(absltest.TestCase):

    def test_iteration_limit(self):
        params = solvers_pb2.PrimalDualHybridGradientParams()
        params.termination_criteria.iteration_limit = 1
        params.termination_check_frequency = 1
        result = pdlp.primal_dual_hybrid_gradient(tiny_lp(), params)
        self.assertLessEqual(result.solve_log.iteration_count, 1)
        self.assertEqual(
            result.solve_log.termination_reason,
            solve_log_pb2.TERMINATION_REASON_ITERATION_LIMIT,
        )

    def test_solution(self):
        params = solvers_pb2.PrimalDualHybridGradientParams()
        opt_criteria = params.termination_criteria.simple_optimality_criteria
        opt_criteria.eps_optimal_relative = 0.0
        opt_criteria.eps_optimal_absolute = 1.0e-10
        result = pdlp.primal_dual_hybrid_gradient(tiny_lp(), params)
        self.assertEqual(
            result.solve_log.termination_reason,
            solve_log_pb2.TERMINATION_REASON_OPTIMAL,
        )
        self.assertSequenceAlmostEqual(result.primal_solution, [1.0, 0.0, 6.0, 2.0])
        self.assertSequenceAlmostEqual(result.dual_solution, [0.5, 4.0, 0.0])
        self.assertSequenceAlmostEqual(result.reduced_costs, [0.0, 1.5, -3.5, 0.0])

    def test_solution_2(self):
        params = solvers_pb2.PrimalDualHybridGradientParams()
        opt_criteria = params.termination_criteria.simple_optimality_criteria
        opt_criteria.eps_optimal_relative = 0.0
        opt_criteria.eps_optimal_absolute = 1.0e-10
        result = pdlp.primal_dual_hybrid_gradient(small_lp(), params)
        self.assertEqual(
            result.solve_log.termination_reason,
            solve_log_pb2.TERMINATION_REASON_OPTIMAL,
        )
        self.assertSequenceAlmostEqual(result.primal_solution, [-1, 8, 1, 2.5])
        self.assertSequenceAlmostEqual(result.dual_solution, [-2, 0, 2.375, 2 / 3])

    def test_starting_point(self):
        params = solvers_pb2.PrimalDualHybridGradientParams()
        opt_criteria = params.termination_criteria.simple_optimality_criteria
        opt_criteria.eps_optimal_relative = 0.0
        opt_criteria.eps_optimal_absolute = 1.0e-10
        params.l_inf_ruiz_iterations = 0
        params.l2_norm_rescaling = False

        start = pdlp.PrimalAndDualSolution()
        start.primal_solution = [1.0, 0.0, 6.0, 2.0]
        start.dual_solution = [0.5, 4.0, 0.0]
        result = pdlp.primal_dual_hybrid_gradient(
            tiny_lp(), params, initial_solution=start
        )
        self.assertEqual(
            result.solve_log.termination_reason,
            solve_log_pb2.TERMINATION_REASON_OPTIMAL,
        )
        self.assertEqual(result.solve_log.iteration_count, 0)


if __name__ == "__main__":
    absltest.main()
