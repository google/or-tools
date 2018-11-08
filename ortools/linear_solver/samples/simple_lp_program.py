# Copyright 2010-2017 Google
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
"""Minimal example to call the GLOP solver."""

# [START program]
from __future__ import print_function

from ortools.linear_solver import pywraplp


def main():
  # Create the linear solver with the GLOP backend.
  solver = pywraplp.Solver('simple_lp_program',
                           pywraplp.Solver.GLOP_LINEAR_PROGRAMMING)
  # Create the variables x and y.
  x = solver.NumVar(0, 1, 'x')
  y = solver.NumVar(0, 2, 'y')

  # Create a linear constraint.
  ct = solver.Constraint(0, 2, 'ct')
  ct.SetCoefficient(x, 1)
  ct.SetCoefficient(y, 1)

  # Create the objective function, x + y.
  objective = solver.Objective()
  objective.SetCoefficient(x, 3)
  objective.SetCoefficient(y, 1)
  objective.SetMaximization()

  # Call the solver and display the results.
  solver.Solve()
  print('Solution:')
  print('Objective value = ', objective.Value())
  print('x = ', x.solution_value())
  print('y = ', y.solution_value())


if __name__ == '__main__':
  main()
# [END program]
