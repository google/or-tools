#!/usr/bin/env python3
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

"""Unit tests for solve.py that require using Gurobi as the underlying solver.

These tests are in a separate file because Gurobi can only run on a licensed
machine.
"""

import unittest
from ortools.math_opt.python import callback
from ortools.math_opt.python import compute_infeasible_subsystem_result
from ortools.math_opt.python import model
from ortools.math_opt.python import parameters
from ortools.math_opt.python import result
from ortools.math_opt.python import solve


_Bounds = compute_infeasible_subsystem_result.ModelSubsetBounds


class SolveTest(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
