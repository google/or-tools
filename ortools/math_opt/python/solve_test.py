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

import io
from typing import Dict, List, Union

from absl.testing import absltest
from ortools.math_opt.core.python import solver as core_solver
from ortools.math_opt.python import linear_constraints
from ortools.math_opt.python import message_callback
from ortools.math_opt.python import model
from ortools.math_opt.python import model_parameters
from ortools.math_opt.python import parameters
from ortools.math_opt.python import result
from ortools.math_opt.python import solve
from ortools.math_opt.python import sparse_containers
from ortools.math_opt.python import variables
from ortools.util.python import solve_interrupter

VarOrConstraintDict = Union[
    Dict[variables.Variable, float],
    Dict[linear_constraints.LinearConstraint, float],
]

# This string appears in the logs if and only if SCIP is doing an incremental
# solve.
_SCIP_LOG_SOLVE_WAS_INCREMENTAL = (
    "feasible solutions given by solution candidate storage"
)


def _list_is_near(v1: List[float], v2: List[float], tolerance: float = 1e-5) -> bool:
    if len(v1) != len(v2):
        return False
    for i, v in enumerate(v1):
        if abs(v2[i] - v) > tolerance:
            return False
    return True


class SolveTest(absltest.TestCase):

    def _assert_dict_almost_equal(
        self, expected: VarOrConstraintDict, actual: VarOrConstraintDict, places=5
    ):
        act_keys = set(actual.keys())
        exp_keys = set(expected.keys())
        self.assertSetEqual(
            exp_keys,
            act_keys,
            msg=f"actual keys: {act_keys} not equal expected keys: {exp_keys}",
        )
        for k, v in expected.items():
            self.assertAlmostEqual(
                v,
                actual[k],
                places,
                msg=f"actual: {actual} and expected: {expected} disagree on key: {k}",
            )

    def test_solve_error(self) -> None:
        mod = model.Model(name="test_model")
        mod.add_variable(lb=1.0, ub=-1.0, name="x1")
        with self.assertRaisesRegex(ValueError, "variables.*lower_bound > upper_bound"):
            solve.solve(mod, parameters.SolverType.GLOP)

    def test_lp_solve(self) -> None:
        mod = model.Model(name="test_model")
        # Solve the problem:
        #   max    x1 + 2 * x2
        #   s.t.   x1 + x2 <= 1
        #          x1, x2 in [0, 1]
        # Primal optimal: [x1, x2] = [0.0, 1.0]
        #
        # Following: go/mathopt-dual#primal-dual-optimal-pairs, the optimal dual
        # solution [y, r1, r2] for [x1, x2] = [0.0, 1.0] must satisfy:
        #   y + r1 = 1
        #   y + r2 = 2
        #   y >= 0
        #   r1 <= 0
        #   r2 >= 0
        # We see that any convex combination of [1, 0, 1] and [2, -1, 0] gives a
        # dual optimal solution.
        x1 = mod.add_variable(lb=0.0, ub=1.0, name="x1")
        x2 = mod.add_variable(lb=0.0, ub=1.0, name="x2")
        mod.objective.is_maximize = True
        mod.objective.set_linear_coefficient(x1, 1.0)
        mod.objective.set_linear_coefficient(x2, 2.0)
        c = mod.add_linear_constraint(ub=1.0, name="c")
        c.set_coefficient(x1, 1.0)
        c.set_coefficient(x2, 1.0)
        res = solve.solve(mod, parameters.SolverType.GLOP)
        self.assertEqual(
            res.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertAlmostEqual(2.0, res.termination.objective_bounds.primal_bound)
        self.assertGreaterEqual(len(res.solutions), 1)
        assert (
            res.solutions[0].primal_solution is not None
            and res.solutions[0].dual_solution is not None
        )
        self.assertAlmostEqual(2.0, res.solutions[0].primal_solution.objective_value)
        self._assert_dict_almost_equal(
            {x1: 0.0, x2: 1.0}, res.solutions[0].primal_solution.variable_values
        )
        dual = res.solutions[0].dual_solution
        self.assertAlmostEqual(2.0, dual.objective_value)
        self.assertSetEqual({c}, set(dual.dual_values.keys()))
        self.assertSetEqual({x1, x2}, set(dual.reduced_costs.keys()))
        # Possible values for [y, r1, r2] are [1, 0, 1] and [2, -1, 0].
        dual_vec = [
            dual.dual_values[c],
            dual.reduced_costs[x1],
            dual.reduced_costs[x2],
        ]
        # Warning: the code below assumes the returned solution is a basic solution.
        # If a non-simplex solver was used, we could get any convex combination of
        # the vectors below.
        expected1 = [1.0, 0.0, 1.0]
        expected2 = [2.0, -1.0, 0.0]
        self.assertTrue(
            _list_is_near(expected1, dual_vec) or _list_is_near(expected2, dual_vec),
            msg=f"dual_vec is {dual_vec}; expected {expected1} or {expected2}",
        )

    def test_indicator(self) -> None:
        # min  2 * x + y + 10 z
        # s.t. if not z then x + y >= 6
        #      x, y >= 0
        #      z binary
        #
        # Optimal solution is (x, y, z) = (0, 6, 0), objective value 6.
        mod = model.Model()
        x = mod.add_variable(lb=0)
        y = mod.add_variable(lb=0)
        z = mod.add_binary_variable()
        mod.add_indicator_constraint(
            indicator=z, activate_on_zero=True, implied_constraint=x + y >= 6.0
        )
        mod.minimize(2 * x + y + 10 * z)

        res = solve.solve(mod, parameters.SolverType.GSCIP)
        self.assertEqual(res.termination.reason, result.TerminationReason.OPTIMAL)
        self.assertAlmostEqual(res.objective_value(), 6.0, delta=1e-5)
        self.assertAlmostEqual(res.variable_values(x), 0.0, delta=1e-5)
        self.assertAlmostEqual(res.variable_values(y), 6.0, delta=1e-5)
        self.assertAlmostEqual(res.variable_values(z), 0.0, delta=1e-5)

    def test_filters(self) -> None:
        mod = model.Model(name="test_model")
        # Solve the problem:
        #   max    x + y + z
        #   s.t.   x + y <= 1
        #          y + z <= 1
        #          x, y, z in [0, 1]
        # Primal optimal: [x, y, z] = [1.0, 0.0, 1.0]
        x = mod.add_variable(lb=0.0, ub=1.0, name="x")
        y = mod.add_variable(lb=0.0, ub=1.0, name="y")
        z = mod.add_variable(lb=0.0, ub=1.0, name="z")
        mod.objective.is_maximize = True
        mod.objective.set_linear_coefficient(x, 1.0)
        mod.objective.set_linear_coefficient(y, 1.0)
        mod.objective.set_linear_coefficient(z, 1.0)
        c = mod.add_linear_constraint(ub=1.0, name="c")
        c.set_coefficient(x, 1.0)
        c.set_coefficient(y, 1.0)
        d = mod.add_linear_constraint(ub=1.0, name="d")
        d.set_coefficient(y, 1.0)
        d.set_coefficient(z, 1.0)
        model_params = model_parameters.ModelSolveParameters()
        model_params.variable_values_filter = sparse_containers.SparseVectorFilter(
            skip_zero_values=True
        )
        model_params.dual_values_filter = sparse_containers.SparseVectorFilter(
            filtered_items=[d]
        )
        model_params.reduced_costs_filter = sparse_containers.SparseVectorFilter(
            filtered_items=[y, z]
        )
        res = solve.solve(mod, parameters.SolverType.GLOP, model_params=model_params)
        self.assertEqual(
            res.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertAlmostEqual(2.0, res.termination.objective_bounds.primal_bound)
        self.assertGreaterEqual(len(res.solutions), 1)
        assert (
            res.solutions[0].primal_solution is not None
            and res.solutions[0].dual_solution is not None
        )
        self.assertAlmostEqual(2.0, res.solutions[0].primal_solution.objective_value)
        # y is zero and thus filtered.
        self._assert_dict_almost_equal(
            {x: 1.0, z: 1.0}, res.solutions[0].primal_solution.variable_values
        )
        self.assertGreaterEqual(len(res.solutions), 1)
        dual = res.solutions[0].dual_solution
        self.assertAlmostEqual(2.0, dual.objective_value)
        # The dual was filtered by id
        self.assertSetEqual({d}, set(dual.dual_values.keys()))
        self.assertSetEqual({y, z}, set(dual.reduced_costs.keys()))

    def test_message_callback(self):
        opt_model = model.Model()
        x = opt_model.add_binary_variable(name="x")
        opt_model.objective.is_maximize = True
        opt_model.objective.set_linear_coefficient(x, 2.0)

        logs = io.StringIO()
        res = solve.solve(
            opt_model,
            parameters.SolverType.GSCIP,
            msg_cb=message_callback.printer_message_callback(file=logs),
        )

        self.assertEqual(
            res.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertIn("problem is solved", logs.getvalue())

    def test_solve_interrupter(self):
        opt_model = model.Model()
        x = opt_model.add_binary_variable(name="x")
        opt_model.objective.is_maximize = True
        opt_model.objective.set_linear_coefficient(x, 2.0)

        interrupter = solve_interrupter.SolveInterrupter()
        interrupter.interrupt()
        res = solve.solve(
            opt_model,
            parameters.SolverType.GSCIP,
            interrupter=interrupter,
        )

        self.assertEqual(
            res.termination.reason,
            result.TerminationReason.NO_SOLUTION_FOUND,
            msg=res.termination,
        )
        self.assertEqual(
            res.termination.limit,
            result.Limit.INTERRUPTED,
            msg=res.termination,
        )

    def test_solve_duplicated_names(self):
        opt_model = model.Model()
        opt_model.add_binary_variable(name="x")
        opt_model.add_binary_variable(name="x")

        with self.assertRaisesRegex(ValueError, "duplicate name"):
            solve.solve(
                opt_model,
                parameters.SolverType.GSCIP,
            )

    def test_solve_remove_names(self):
        opt_model = model.Model()
        opt_model.add_binary_variable(name="x")
        opt_model.add_binary_variable(name="x")

        # We test that remove_names was taken into account by testing that no error
        # is raised.
        solve.solve(
            opt_model,
            parameters.SolverType.GSCIP,
            remove_names=True,
        )

    def test_incremental_solve_remove_names(self) -> None:
        """Here we expect no errors dues to duplicated names.

        See test_incremental_solve_init_error for test of duplicated names.
        """
        mod = model.Model(name="test_model")
        mod.add_variable(lb=1.0, ub=1.0, name="x1")
        mod.add_variable(lb=1.0, ub=1.0, name="x1")
        # We test that remove_names was taken into account by testing that no error
        # is raised.
        solve.IncrementalSolver(mod, parameters.SolverType.GLOP, remove_names=True)

    def test_incremental_solve_init_error(self) -> None:
        mod = model.Model(name="test_model")
        mod.add_variable(lb=1.0, ub=1.0, name="x1")
        mod.add_variable(lb=1.0, ub=1.0, name="x1")
        with self.assertRaisesRegex(ValueError, "duplicate name"):
            solve.IncrementalSolver(mod, parameters.SolverType.GLOP)

    def test_incremental_solve_error(self) -> None:
        mod = model.Model(name="test_model")
        mod.add_variable(lb=1.0, ub=-1.0, name="x1")
        solver = solve.IncrementalSolver(mod, parameters.SolverType.GLOP)
        with self.assertRaisesRegex(ValueError, "variables.*lower_bound > upper_bound"):
            solver.solve()

    def test_incremental_solve_error_on_reject(self) -> None:
        opt_model = model.Model()
        x = opt_model.add_binary_variable(name="x")
        opt_model.objective.set_linear_coefficient(x, 2.0)
        opt_model.objective.is_maximize = True
        # CP-SAT rejects all model changes and solves from scratch each time.
        solver = solve.IncrementalSolver(opt_model, parameters.SolverType.CP_SAT)

        res = solver.solve(
            msg_cb=message_callback.printer_message_callback(prefix="[solve 1] ")
        )

        self.assertEqual(
            res.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertGreaterEqual(len(res.solutions), 1)
        assert res.solutions[0].primal_solution is not None
        self.assertAlmostEqual(2.0, res.solutions[0].primal_solution.objective_value)
        self._assert_dict_almost_equal(
            {x: 1.0}, res.solutions[0].primal_solution.variable_values
        )

        opt_model.add_binary_variable(name="x")
        with self.assertRaisesRegex(ValueError, "duplicate name"):
            solver.solve(
                msg_cb=message_callback.printer_message_callback(prefix="[solve 2] ")
            )

    def test_incremental_lp(self) -> None:
        opt_model = model.Model()
        x = opt_model.add_variable(lb=0, ub=1, name="x")
        opt_model.objective.set_linear_coefficient(x, 2.0)
        opt_model.objective.is_maximize = True
        solver = solve.IncrementalSolver(opt_model, parameters.SolverType.GLOP)
        params = parameters.SolveParameters()
        params.enable_output = True
        res = solver.solve(params=params)
        self.assertEqual(
            res.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertGreaterEqual(len(res.solutions), 1)
        assert res.solutions[0].primal_solution is not None
        self.assertAlmostEqual(2.0, res.solutions[0].primal_solution.objective_value)
        self._assert_dict_almost_equal(
            {x: 1.0}, res.solutions[0].primal_solution.variable_values
        )

        x.upper_bound = 3.0
        res2 = solver.solve(params=params)
        self.assertEqual(
            res2.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertGreaterEqual(len(res2.solutions), 1)
        self.assertAlmostEqual(6.0, res2.solutions[0].primal_solution.objective_value)
        self._assert_dict_almost_equal(
            {x: 3.0}, res2.solutions[0].primal_solution.variable_values
        )

    def test_incremental_mip(self) -> None:
        opt_model = model.Model()
        x = opt_model.add_binary_variable(name="x")
        y = opt_model.add_binary_variable(name="y")
        c = opt_model.add_linear_constraint(ub=1.0, name="c")
        c.set_coefficient(x, 1.0)
        c.set_coefficient(y, 1.0)
        opt_model.objective.set_linear_coefficient(x, 2.0)
        opt_model.objective.set_linear_coefficient(y, 3.0)
        opt_model.objective.is_maximize = True
        solver = solve.IncrementalSolver(opt_model, parameters.SolverType.GSCIP)
        params = parameters.SolveParameters()
        params.enable_output = True
        res = solver.solve(params=params)
        self.assertEqual(
            res.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertGreaterEqual(len(res.solutions), 1)
        assert res.solutions[0].primal_solution is not None
        self.assertAlmostEqual(3.0, res.solutions[0].primal_solution.objective_value)
        self._assert_dict_almost_equal(
            {x: 0.0, y: 1.0}, res.solutions[0].primal_solution.variable_values
        )

        c.upper_bound = 2.0
        res2 = solver.solve(params=params)
        self.assertEqual(
            res2.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertGreaterEqual(len(res2.solutions), 1)
        assert res2.solutions[0].primal_solution is not None
        self.assertAlmostEqual(5.0, res2.solutions[0].primal_solution.objective_value)
        self._assert_dict_almost_equal(
            {x: 1.0, y: 1.0}, res2.solutions[0].primal_solution.variable_values
        )

    def test_incremental_mip_with_message_cb(self) -> None:
        opt_model = model.Model()
        x = opt_model.add_binary_variable(name="x")
        y = opt_model.add_binary_variable(name="y")
        c = opt_model.add_linear_constraint(ub=1.0, name="c")
        c.set_coefficient(x, 1.0)
        c.set_coefficient(y, 1.0)
        opt_model.objective.set_linear_coefficient(x, 2.0)
        opt_model.objective.set_linear_coefficient(y, 3.0)
        opt_model.objective.is_maximize = True
        solver = solve.IncrementalSolver(opt_model, parameters.SolverType.GSCIP)
        params = parameters.SolveParameters()
        params.enable_output = True

        logs = io.StringIO()
        res = solver.solve(
            params=params,
            msg_cb=message_callback.printer_message_callback(file=logs),
        )

        self.assertEqual(
            res.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertGreaterEqual(len(res.solutions), 1)
        assert res.solutions[0].primal_solution is not None
        self.assertAlmostEqual(3.0, res.solutions[0].primal_solution.objective_value)
        self._assert_dict_almost_equal(
            {x: 0.0, y: 1.0}, res.solutions[0].primal_solution.variable_values
        )
        self.assertNotIn(_SCIP_LOG_SOLVE_WAS_INCREMENTAL, logs.getvalue())

        c.upper_bound = 2.0

        logs = io.StringIO()
        res2 = solver.solve(
            params=params,
            msg_cb=message_callback.printer_message_callback(file=logs),
        )

        self.assertEqual(
            res2.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertGreaterEqual(len(res2.solutions), 1)
        assert res2.solutions[0].primal_solution is not None
        self.assertAlmostEqual(5.0, res2.solutions[0].primal_solution.objective_value)
        self._assert_dict_almost_equal(
            {x: 1.0, y: 1.0}, res2.solutions[0].primal_solution.variable_values
        )
        self.assertIn(_SCIP_LOG_SOLVE_WAS_INCREMENTAL, logs.getvalue())

    def test_incremental_solve_interrupter(self) -> None:
        opt_model = model.Model()
        x = opt_model.add_binary_variable(name="x")
        y = opt_model.add_binary_variable(name="y")
        c = opt_model.add_linear_constraint(ub=1.0, name="c")
        c.set_coefficient(x, 1.0)
        c.set_coefficient(y, 1.0)
        opt_model.objective.set_linear_coefficient(x, 2.0)
        opt_model.objective.set_linear_coefficient(y, 3.0)
        opt_model.objective.is_maximize = True
        solver = solve.IncrementalSolver(opt_model, parameters.SolverType.GSCIP)

        interrupter = solve_interrupter.SolveInterrupter()
        interrupter.interrupt()
        res = solver.solve(interrupter=interrupter)

        self.assertEqual(
            res.termination.reason,
            result.TerminationReason.NO_SOLUTION_FOUND,
            msg=res.termination,
        )
        self.assertEqual(
            res.termination.limit,
            result.Limit.INTERRUPTED,
            msg=res.termination,
        )

    def test_incremental_solve_rejected(self) -> None:
        opt_model = model.Model()
        x = opt_model.add_binary_variable(name="x")
        opt_model.objective.set_linear_coefficient(x, 2.0)
        opt_model.objective.is_maximize = True
        # CP-SAT rejects all model changes and solves from scratch each time.
        solver = solve.IncrementalSolver(opt_model, parameters.SolverType.CP_SAT)

        res = solver.solve(
            msg_cb=message_callback.printer_message_callback(prefix="[solve 1] ")
        )

        self.assertEqual(
            res.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertGreaterEqual(len(res.solutions), 1)
        assert res.solutions[0].primal_solution is not None
        self.assertAlmostEqual(2.0, res.solutions[0].primal_solution.objective_value)
        self._assert_dict_almost_equal(
            {x: 1.0}, res.solutions[0].primal_solution.variable_values
        )

        x.upper_bound = 3.0

        res2 = solver.solve(
            msg_cb=message_callback.printer_message_callback(prefix="[solve 2] ")
        )

        self.assertEqual(
            res2.termination.reason,
            result.TerminationReason.OPTIMAL,
            msg=res.termination,
        )
        self.assertGreaterEqual(len(res2.solutions), 1)
        assert res2.solutions[0].primal_solution is not None
        self.assertAlmostEqual(6.0, res2.solutions[0].primal_solution.objective_value)
        self._assert_dict_almost_equal(
            {
                x: 3.0,
            },
            res2.solutions[0].primal_solution.variable_values,
        )

    def test_multiple_incremental_lps(self) -> None:
        opt_model = model.Model()
        x = opt_model.add_variable(lb=0, ub=1, name="x")
        opt_model.objective.set_linear_coefficient(x, 2.0)
        opt_model.objective.is_maximize = True
        solver = solve.IncrementalSolver(opt_model, parameters.SolverType.GLOP)
        params = parameters.SolveParameters()
        params.presolve = parameters.Emphasis.OFF
        params.enable_output = True
        for ub in [2.0, 3.0, 4.0, 5.0]:
            x.upper_bound = ub
            res = solver.solve(params=params)
            self.assertEqual(
                res.termination.reason,
                result.TerminationReason.OPTIMAL,
                msg=res.termination,
            )
            self.assertGreaterEqual(len(res.solutions), 1)
            assert res.solutions[0].primal_solution is not None
            self.assertAlmostEqual(
                2.0 * ub, res.solutions[0].primal_solution.objective_value
            )
            self._assert_dict_almost_equal(
                {x: ub}, res.solutions[0].primal_solution.variable_values
            )

    def test_incremental_solver_delete(self) -> None:
        opt_model = model.Model()
        start_num_solver = core_solver.debug_num_solver()
        solver = solve.IncrementalSolver(opt_model, parameters.SolverType.GLOP)
        self.assertEqual(core_solver.debug_num_solver(), start_num_solver + 1)
        del solver
        self.assertEqual(core_solver.debug_num_solver(), start_num_solver)

    def test_incremental_solver_close(self) -> None:
        opt_model = model.Model()
        start_num_solver = core_solver.debug_num_solver()
        solver = solve.IncrementalSolver(opt_model, parameters.SolverType.GLOP)
        self.assertEqual(core_solver.debug_num_solver(), start_num_solver + 1)
        solver.close()
        self.assertEqual(core_solver.debug_num_solver(), start_num_solver)
        with self.assertRaisesRegex(RuntimeError, "closed"):
            solver.solve()

    def test_incremental_solver_close_twice(self) -> None:
        opt_model = model.Model()
        start_num_solver = core_solver.debug_num_solver()
        solver = solve.IncrementalSolver(opt_model, parameters.SolverType.GLOP)
        self.assertEqual(core_solver.debug_num_solver(), start_num_solver + 1)
        solver.close()
        solver.close()
        self.assertEqual(core_solver.debug_num_solver(), start_num_solver)

    def test_incremental_solver_context_manager(self) -> None:
        opt_model = model.Model()
        start_num_solver = core_solver.debug_num_solver()
        with solve.IncrementalSolver(opt_model, parameters.SolverType.GLOP) as solver:
            self.assertEqual(core_solver.debug_num_solver(), start_num_solver + 1)
            res1 = solver.solve()
            self.assertEqual(
                res1.termination.reason,
                result.TerminationReason.OPTIMAL,
                msg=res1.termination,
            )
            res2 = solver.solve()
            self.assertEqual(
                res2.termination.reason, result.TerminationReason.OPTIMAL, msg=res2
            )

        self.assertEqual(core_solver.debug_num_solver(), start_num_solver)
        with self.assertRaisesRegex(RuntimeError, "closed"):
            solver.solve()

    def test_incremental_solver_context_manager_exception(self) -> None:
        """Tests that exceptions raised in the context manager are not lost."""
        opt_model = model.Model()
        with self.assertRaisesRegex(NotImplementedError, "some message"):
            with solve.IncrementalSolver(opt_model, parameters.SolverType.GLOP):
                raise NotImplementedError("some message")


if __name__ == "__main__":
    absltest.main()
