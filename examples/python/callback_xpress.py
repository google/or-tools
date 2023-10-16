#!/usr/bin/env python3
# Copyright 2023 RTE
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
"""MIP example/test that shows how to use the callback API."""

from ortools.linear_solver import pywraplp
from ortools.linear_solver.pywraplp import MPCallbackContext, MPCallback
import random
import unittest


class MyMPCallback(MPCallback):
    def __init__(self, mp_solver: pywraplp.Solver):
        super().__init__(False, False)
        self._mp_solver_ = mp_solver
        self._solutions_ = 0
        self._last_var_values_ = [0] * len(mp_solver.variables())

    def RunCallback(self, ctx: MPCallbackContext):
        self._solutions_ += 1
        for i in range(0, len(self._mp_solver_.variables())) :
            self._last_var_values_[i] = ctx.VariableValue(self._mp_solver_.variable(i))


class TestSiriusXpress(unittest.TestCase):
    def test_callback(self):
        """Builds a large MIP that is difficult to solve, in order for us to have time to intercept non-optimal
         feasible solutions using callback"""
        solver = pywraplp.Solver.CreateSolver('XPRESS_MIXED_INTEGER_PROGRAMMING')
        n_vars = 30
        max_time = 30
        if not solver:
            return

        random.seed(123)

        objective = solver.Objective()
        objective.SetMaximization()

        for i in range(0, n_vars):
            x = solver.IntVar(-random.random() * 200, random.random() * 200, 'x_' + str(i))
            objective.SetCoefficient(x, random.random() * 200 - 100)
            if i == 0:
                continue
            rand1 = -random.random() * 2000
            rand2 = random.random() * 2000
            c = solver.Constraint(min(rand1, rand2), max(rand1, rand2))
            c.SetCoefficient(x, random.random() * 200 - 100)
            for j in range(0, i):
                c.SetCoefficient(solver.variable(j), random.random() * 200 - 100)

        solver.SetSolverSpecificParametersAsString("PRESOLVE 0 MAXTIME " + str(max_time))
        solver.EnableOutput()

        cb = MyMPCallback(solver)
        solver.SetCallback(cb)

        solver.Solve()

        # This is a tough MIP, in 30 seconds XPRESS should have found at least 5
        # solutions (tested with XPRESS v9.0, may change in later versions)
        self.assertTrue(cb._solutions_ > 5)
        # Test that the last solution intercepted by callback is the same as the optimal one retained
        for i in range(0, len(solver.variables())):
            self.assertAlmostEqual(cb._last_var_values_[i], solver.variable(i).SolutionValue())


if __name__ == '__main__':
    unittest.main()

