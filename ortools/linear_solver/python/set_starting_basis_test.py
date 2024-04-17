#!/usr/bin/env python3
# Copyright 2010-2024 Google LLC
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

"""Simple unit tests for python LP Basis API."""

import unittest
import random
from ortools.linear_solver import pywraplp

class TestSetStartingBasis(unittest.TestCase):
    def build_large_lp(self, solver):
        n_vars = 10
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

    def test_xpress(self):
        # Build an LP and solve it, then fetch LP basis
        solver = pywraplp.Solver.CreateSolver("XPRESS_LP")
        self.build_large_lp(solver)
        solver.Solve()
        assert solver.iterations() >= 1

        var_basis = []
        con_basis = []
        for var in solver.variables():
            var_basis.append(var.basis_status())
        for con in solver.constraints():
            con_basis.append(con.basis_status())

        # Re-build the same optimization problem in another MPSolver
        solver_with_basis = pywraplp.Solver.CreateSolver("XPRESS_LP")
        self.build_large_lp(solver_with_basis)
        # Set same basis as previous Solver
        solver_with_basis.SetStartingLpBasis(var_basis, con_basis)
        # Solve and check that it finds the same solution with no iterations at all
        solver_with_basis.Solve()
        self.assertAlmostEqual(solver.Objective().Value(), solver_with_basis.Objective().Value(), delta=1)
        assert solver_with_basis.iterations() == 0

if __name__ == "__main__":
    unittest.main()
