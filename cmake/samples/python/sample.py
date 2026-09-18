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

"""Sample to test or-tools installation."""

import ortools
# from ortools.algorithms import knapsack_solver
from ortools.constraint_solver.python import constraint_solver as cp
# from ortools.graph.python import linear_sum_assignment
# from ortools.graph.python import max_flow
# from ortools.graph.python import min_cost_flow
from ortools.linear_solver import pywraplp

# from ortools.linear_solver import linear_solver_pb2
# from ortools.sat.python import cp_model_helper
# from ortools.sat.python import cp_model
# from ortools.scheduling import rcpsp
# from ortools.util.python import sorted_interval_list


def lpsolver_test():
    """Test pywraplp."""
    print("Test lpsolver...")
    lpsolver = pywraplp.Solver("LinearTest", pywraplp.Solver.GLOP_LINEAR_PROGRAMMING)
    lpsolver.Solve()
    print("Test lpsolver...DONE")


def cpsolver_test():
    """Test solver CP."""
    print("Test solver CP...")
    solver = cp.Solver("ConstraintTest")
    num_vals = 3
    x = solver.new_int_var(0, num_vals - 1, "x")
    y = solver.new_int_var(0, num_vals - 1, "y")
    z = solver.new_int_var(0, num_vals - 1, "z")
    solver.add(x != y)
    db = solver.phase(
        [x, y, z],
        cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
        cp.IntValueStrategy.ASSIGN_MIN_VALUE,
    )
    solver.solve(db)
    print("Test solver CP...DONE")


def main():
    print(ortools.__version__)
    lpsolver_test()
    cpsolver_test()


if __name__ == "__main__":
    main()
