# Copyright 2011 Hakan Kjellerstrand hakank@gmail.com
#
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
"""

  Rogo puzzle solver in Google CP Solver.

  From http://www.rogopuzzle.co.nz/
  '''
  The object is to collect the biggest score possible using a given
  number of steps in a loop around a grid. The best possible score
  for a puzzle is given with it, so you can easily check that you have
  solved the puzzle. Rogo puzzles can also include forbidden squares,
  which must be avoided in your loop.
  '''

  Also see Mike Trick:
  'Operations Research, Sudoko, Rogo, and Puzzles'
  http://mat.tepper.cmu.edu/blog/?p=1302

  Problem instances:
  * http://www.hakank.org/google_or_tools/rogo_mike_trick.py
  * http://www.hakank.org/google_or_tools/rogo_20110106.py
  * http://www.hakank.org/google_or_tools/rogo_20110107.py


  Compare with the following models:
  * Answer Set Programming:
     http://www.hakank.org/answer_set_programming/rogo2.lp
  * MiniZinc: http://www.hakank.org/minizinc/rogo2.mzn

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""
import sys
import re

from ortools.constraint_solver import pywrapcp


def main(problem, rows, cols, max_steps):

  # Create the solver.
  solver = pywrapcp.Solver("Rogo grid puzzle")

  #
  # data
  #
  W = 0
  B = -1
  print("rows: %i cols: %i max_steps: %i" % (rows, cols, max_steps))

  problem_flatten = [problem[i][j] for i in range(rows) for j in range(cols)]
  max_point = max(problem_flatten)
  print("max_point:", max_point)
  max_sum = sum(problem_flatten)
  print("max_sum:", max_sum)
  print()

  #
  # declare variables
  #

  # the coordinates
  x = [solver.IntVar(0, rows - 1, "x[%i]" % i) for i in range(max_steps)]
  y = [solver.IntVar(0, cols - 1, "y[%i]" % i) for i in range(max_steps)]

  # the collected points
  points = [
      solver.IntVar(0, max_point, "points[%i]" % i) for i in range(max_steps)
  ]

  # objective: sum of points in the path
  sum_points = solver.IntVar(0, max_sum)

  #
  # constraints
  #

  # all coordinates must be unique
  for s in range(max_steps):
    for t in range(s + 1, max_steps):
      b1 = x[s] != x[t]
      b2 = y[s] != y[t]
      solver.Add(b1 + b2 >= 1)

  # calculate the points (to maximize)
  for s in range(max_steps):
    solver.Add(points[s] == solver.Element(problem_flatten, x[s] * cols + y[s]))

  solver.Add(sum_points == sum(points))

  # ensure that there are not black cells in
  # the path
  for s in range(max_steps):
    solver.Add(solver.Element(problem_flatten, x[s] * cols + y[s]) != B)

  # get the path
  for s in range(max_steps - 1):
    solver.Add(abs(x[s] - x[s + 1]) + abs(y[s] - y[s + 1]) == 1)

  # close the path around the corner
  solver.Add(abs(x[max_steps - 1] - x[0]) + abs(y[max_steps - 1] - y[0]) == 1)

  # symmetry breaking: the cell with lowest coordinates
  # should be in the first step.
  for i in range(1, max_steps):
    solver.Add(x[0] * cols + y[0] < x[i] * cols + y[i])

  # symmetry breaking: second step is larger than
  # first step
  # solver.Add(x[0]*cols+y[0] < x[1]*cols+y[1])

  #
  # objective
  #
  objective = solver.Maximize(sum_points, 1)

  #
  # solution and search
  #
  # db = solver.Phase(x + y,
  #                    solver.CHOOSE_MIN_SIZE_LOWEST_MIN,
  #                    solver.ASSIGN_MIN_VALUE)

  # Default search
  parameters = pywrapcp.DefaultPhaseParameters()

  parameters.heuristic_period = 200000
  # parameters.var_selection_schema = parameters.CHOOSE_MAX_SUM_IMPACT
  parameters.var_selection_schema = parameters.CHOOSE_MAX_AVERAGE_IMPACT  # <-
  # parameters.var_selection_schema = parameters.CHOOSE_MAX_VALUE_IMPACT

  parameters.value_selection_schema = parameters.SELECT_MIN_IMPACT  # <-
  # parameters.value_selection_schema = parameters.SELECT_MAX_IMPACT

  # parameters.initialization_splits = 10

  db = solver.DefaultPhase(x + y, parameters)

  solver.NewSearch(db, [objective])

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print("sum_points:", sum_points.Value())
    print("adding 1 to coords...")
    for s in range(max_steps):
      print("%i %i" % (x[s].Value() + 1, y[s].Value() + 1))
    print()

  print("\nnum_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


# Default problem:
# Data from
# Mike Trick: "Operations Research, Sudoko, Rogo, and Puzzles"
# http://mat.tepper.cmu.edu/blog/?p=1302
#
# This has 48 solutions with symmetries;
# 4 when the path symmetry is removed.
#
rows = 5
cols = 9
max_steps = 12
W = 0
B = -1
problem = [[2, W, W, W, W, W, W, W, W], [W, 3, W, W, 1, W, W, 2, W],
           [W, W, W, W, W, W, B, W, 2], [W, W, 2, B, W, W, W, W, W],
           [W, W, W, W, 2, W, W, 1, W]]
if __name__ == "__main__":
  if len(sys.argv) > 1:
    exec(compile(open(sys.argv[1]).read(), sys.argv[1], "exec"))
  main(problem, rows, cols, max_steps)
