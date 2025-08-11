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

"""Unit tests for solve.py that require using Gurobi as the underlying solver.

These tests are in a separate file because Gurobi can only run on a licensed
machine.
"""

from absl.testing import absltest
from ortools.gurobi.isv.secret import gurobi_test_isv_key
from ortools.math_opt.python import callback
from ortools.math_opt.python import compute_infeasible_subsystem_result
from ortools.math_opt.python import init_arguments
from ortools.math_opt.python import model
from ortools.math_opt.python import model_parameters
from ortools.math_opt.python import parameters
from ortools.math_opt.python import result
from ortools.math_opt.python import solve
from ortools.math_opt.python import sparse_containers


_Bounds = compute_infeasible_subsystem_result.ModelSubsetBounds

_bad_isv_key = init_arguments.GurobiISVKey(
    name="cat", application_name="hat", expiration=10, key="bat"
)


def _init_args(
    gurobi_key: init_arguments.GurobiISVKey,
) -> init_arguments.StreamableSolverInitArguments:
    return init_arguments.StreamableSolverInitArguments(
        gurobi=init_arguments.StreamableGurobiInitArguments(isv_key=gurobi_key)
    )


class SolveTest(absltest.TestCase):

    def test_callback(self) -> None:
        mod = model.Model(name="test_model")
        # Solve the problem:
        #   max    x + 2 * y
        #   s.t.   x + y <= 1 (added in callback)
        #          x, y in {0, 1}
        # Primal optimal: [x, y] = [0.0, 1.0]
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        mod.objective.is_maximize = True
        mod.objective.set_linear_coefficient(x, 1.0)
        mod.objective.set_linear_coefficient(y, 2.0)

        def cb(cb_data: callback.CallbackData) -> callback.CallbackResult:
            cb_res = callback.CallbackResult()
            if cb_data.solution[x] + cb_data.solution[y] >= 1 + 1e-4:
                cb_res.add_lazy_constraint(x + y <= 1.0)
            return cb_res

        cb_reg = callback.CallbackRegistration()
        cb_reg.events.add(callback.Event.MIP_SOLUTION)
        cb_reg.add_lazy_constraints = True
        params = parameters.SolveParameters(enable_output=True)
        res = solve.solve(
            mod,
            parameters.SolverType.GUROBI,
            params=params,
            callback_reg=cb_reg,
            cb=cb,
        )
        self.assertEqual(
            res.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertAlmostEqual(2.0, res.termination.objective_bounds.primal_bound)

    def test_hierarchical_objectives(self) -> None:
        mod = model.Model()
        # The model is:
        #   max    x + y + 2 z
        #   s.t.   x + y + z <= 1.5
        #          x, y in [0, 1]
        #          z binary
        # With secondary objective
        #   max y
        #
        # The first problem is solved by any convex combination of:
        #   (0.5, 0, 1) and (0, 0.5, 1)
        # But with the secondary objective, the unique solution is (0, 0.5, 1), with
        # a primary objective value of 2.5 and secondary objective value of 0.5.
        x = mod.add_variable(lb=0, ub=1)
        y = mod.add_variable(lb=0, ub=1)
        z = mod.add_binary_variable()
        mod.add_linear_constraint(x + y + z <= 1.5)
        mod.maximize(x + y + 2 * z)
        aux = mod.add_maximization_objective(y, priority=1)

        res = solve.solve(mod, parameters.SolverType.GUROBI)
        self.assertEqual(
            res.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertAlmostEqual(res.objective_value(), 2.5, delta=1e-4)
        self.assertAlmostEqual(res.variable_values(x), 0.0, delta=1e-4)
        self.assertAlmostEqual(res.variable_values(y), 0.5, delta=1e-4)
        self.assertAlmostEqual(res.variable_values(z), 1.0, delta=1e-4)
        prim_sol = res.solutions[0].primal_solution
        self.assertIsNotNone(prim_sol)
        self.assertDictEqual(prim_sol.auxiliary_objective_values, {aux: 0.5})

    def test_quadratic_dual(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        mod.minimize(x)
        c = mod.add_quadratic_constraint(expr=x * x, ub=1.0)
        params = parameters.SolveParameters()
        params.gurobi.param_values["QCPDual"] = "1"
        res = solve.solve(mod, parameters.SolverType.GUROBI, params=params)
        self.assertEqual(res.termination.reason, result.TerminationReason.OPTIMAL)
        sol = res.solutions[0]
        primal = sol.primal_solution
        dual = sol.dual_solution
        self.assertIsNotNone(primal)
        self.assertIsNotNone(dual)
        self.assertAlmostEqual(primal.variable_values[x], -1.0)
        self.assertAlmostEqual(dual.quadratic_dual_values[c], -0.5)

    def test_quadratic_dual_filter(self) -> None:
        # Same as the previous test, but now with a filter on the quadratic duals
        # that are returned.
        mod = model.Model()
        x = mod.add_variable()
        mod.minimize(x)
        mod.add_quadratic_constraint(expr=x * x, ub=1.0)
        params = parameters.SolveParameters()
        params.gurobi.param_values["QCPDual"] = "1"
        mod_params = model_parameters.ModelSolveParameters(
            quadratic_dual_values_filter=sparse_containers.QuadraticConstraintFilter(
                filtered_items={}
            )
        )
        res = solve.solve(
            mod,
            parameters.SolverType.GUROBI,
            params=params,
            model_params=mod_params,
        )
        self.assertEqual(res.termination.reason, result.TerminationReason.OPTIMAL)
        sol = res.solutions[0]
        primal = sol.primal_solution
        dual = sol.dual_solution
        self.assertIsNotNone(primal)
        self.assertIsNotNone(dual)
        self.assertAlmostEqual(primal.variable_values[x], -1.0)
        self.assertEmpty(dual.quadratic_dual_values)

    def test_compute_infeasible_subsystem_infeasible(self):
        mod = model.Model()
        x = mod.add_variable(lb=0.0, ub=1.0)
        y = mod.add_variable(lb=0.0, ub=1.0)
        z = mod.add_variable(lb=0.0, ub=1.0)
        mod.add_linear_constraint(x + y <= 4.0)
        d = mod.add_linear_constraint(x + z >= 3.0)

        iis = solve.compute_infeasible_subsystem(mod, parameters.SolverType.GUROBI)
        self.assertTrue(iis.is_minimal)
        self.assertEqual(iis.feasibility, result.FeasibilityStatus.INFEASIBLE)
        self.assertDictEqual(
            iis.infeasible_subsystem.variable_bounds,
            {x: _Bounds(upper=True), z: _Bounds(upper=True)},
        )
        self.assertDictEqual(
            iis.infeasible_subsystem.linear_constraints, {d: _Bounds(lower=True)}
        )
        self.assertEmpty(iis.infeasible_subsystem.variable_integrality)

    def test_solve_valid_isv_success(self):
        mod = model.Model()
        x = mod.add_binary_variable()
        mod.maximize(x)
        res = solve.solve(
            mod,
            parameters.SolverType.GUROBI,
            streamable_init_args=_init_args(
                gurobi_test_isv_key.google_test_isv_key_placeholder()
            ),
        )
        self.assertEqual(
            res.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertAlmostEqual(1.0, res.termination.objective_bounds.primal_bound)

    def test_solve_wrong_isv_error(self):
        mod = model.Model()
        x = mod.add_binary_variable()
        mod.maximize(x)
        with self.assertRaisesRegex(
            ValueError, "failed to create Gurobi primary environment with ISV key"
        ):
            solve.solve(
                mod,
                parameters.SolverType.GUROBI,
                streamable_init_args=_init_args(_bad_isv_key),
            )

    def test_incremental_solver_valid_isv_success(self):
        mod = model.Model()
        x = mod.add_binary_variable()
        mod.maximize(x)
        s = solve.IncrementalSolver(
            mod,
            parameters.SolverType.GUROBI,
            streamable_init_args=_init_args(
                gurobi_test_isv_key.google_test_isv_key_placeholder()
            ),
        )
        res = s.solve()
        self.assertEqual(
            res.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertAlmostEqual(1.0, res.termination.objective_bounds.primal_bound)

    def test_incremental_solver_wrong_isv_error(self):
        mod = model.Model()
        x = mod.add_binary_variable()
        mod.maximize(x)
        with self.assertRaisesRegex(
            ValueError, "failed to create Gurobi primary environment with ISV key"
        ):
            solve.IncrementalSolver(
                mod,
                parameters.SolverType.GUROBI,
                streamable_init_args=_init_args(_bad_isv_key),
            )

    def test_compute_infeasible_subsystem_valid_isv_success(self):
        mod = model.Model()
        x = mod.add_binary_variable()
        mod.add_linear_constraint(x >= 3.0)
        res = solve.compute_infeasible_subsystem(
            mod,
            parameters.SolverType.GUROBI,
            streamable_init_args=_init_args(
                gurobi_test_isv_key.google_test_isv_key_placeholder()
            ),
        )
        self.assertEqual(res.feasibility, result.FeasibilityStatus.INFEASIBLE)

    def test_compute_infeasible_subsystem_wrong_isv_error(self):
        mod = model.Model()
        x = mod.add_binary_variable()
        mod.add_linear_constraint(x >= 3.0)
        with self.assertRaisesRegex(
            ValueError, "failed to create Gurobi primary environment with ISV key"
        ):
            solve.compute_infeasible_subsystem(
                mod,
                parameters.SolverType.GUROBI,
                streamable_init_args=_init_args(_bad_isv_key),
            )

    def test_compute_infeasible_subsystem_duplicated_names(self):
        opt_model = model.Model()
        opt_model.add_binary_variable(name="x")
        opt_model.add_binary_variable(name="x")
        with self.assertRaisesRegex(ValueError, "duplicate name"):
            solve.compute_infeasible_subsystem(
                opt_model,
                parameters.SolverType.GUROBI,
            )

    def test_compute_infeasible_subsystem_remove_names(self):
        opt_model = model.Model()
        opt_model.add_binary_variable(name="x")
        opt_model.add_binary_variable(name="x")
        # We test that remove_names was taken into account by testing that no error
        # is raised.
        solve.compute_infeasible_subsystem(
            opt_model,
            parameters.SolverType.GUROBI,
            remove_names=True,
        )


if __name__ == "__main__":
    absltest.main()
