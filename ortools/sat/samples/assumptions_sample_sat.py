#!/usr/bin/env python3
# Copyright 2021 Xiang Chen
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
"""Code sample that solves a model and gets the infeasibility assumptions."""
# [START program]
# [START import]
from ortools.sat.python import cp_model
# [END import]


def main():
    """Showcases assumptions."""
    # Creates the model.
    # [START model]
    model = cp_model.CpModel()
    # [END model]

    # Creates the variables.
    # [START variables]
    x = model.NewIntVar(0, 10, 'x')
    y = model.NewIntVar(0, 10, 'y')
    z = model.NewIntVar(0, 10, 'z')
    a = model.NewBoolVar('a')
    b = model.NewBoolVar('b')
    c = model.NewBoolVar('c')
    # [END variables]

    # Creates the constraints.
    # [START constraints]
    model.Add(x > y).OnlyEnforceIf(a)
    model.Add(y > z).OnlyEnforceIf(b)
    model.Add(z > x).OnlyEnforceIf(c)
    # [END constraints]

    # Add assumptions
    model.AddAssumptions([a, b, c])

    # Creates a solver and solves.
    # [START solve]
    solver = cp_model.CpSolver()
    status = solver.Solve(model)
    # [END solve]

    # Print solution.
    # [START print_solution]
    print(f'Status = {solver.StatusName(status)}')
    if status == cp_model.INFEASIBLE:
        print('SufficientAssumptionsForInfeasibility = '
              f'{solver.SufficientAssumptionsForInfeasibility()}')
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program]
