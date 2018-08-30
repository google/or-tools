# Copyright 2010-2011 Google
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
"""Cryptarithmetic puzzle

First attempt to solve equation CP + IS + FUN = TRUE
where each letter represents a unique digit.

This problem has 72 different solutions in base 10.
"""

from __future__ import print_function
from ortools.sat.python import cp_model


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, variables):
    cp_model.CpSolverSolutionCallback.__init__(self)
    self.__variables = variables
    self.__solution_count = 0

  def OnSolutionCallback(self):
    self.__solution_count += 1
    for v in self.__variables:
      print('%s=%i' % (v, self.Value(v)), end=' ')
    print()

  def SolutionCount(self):
    return self.__solution_count


def CPIsFun():

  kBase = 10

  # Constraint programming engine
  model = cp_model.CpModel()

  c = model.NewIntVar(1, 9, 'C')
  p = model.NewIntVar(0, 9, 'P')
  i = model.NewIntVar(1, 9, 'I')
  s = model.NewIntVar(0, 9, 'S')
  f = model.NewIntVar(1, 9, 'F')
  u = model.NewIntVar(0, 9, 'U')
  n = model.NewIntVar(0, 9, 'N')
  t = model.NewIntVar(1, 9, 'T')
  r = model.NewIntVar(0, 9, 'R')
  e = model.NewIntVar(0, 9, 'E')

  # We need to group variables in a list to use the constraint AllDifferent.
  letters = [c, p, i, s, f, u, n, t, r, e]

  # Verify that we have enough digits.
  assert kBase >= len(letters)

  # Define constraints.
  model.AddAllDifferent(letters)

  # CP + IS + FUN = TRUE
  model.Add(p + s + n + kBase * (c + i + u) + kBase * kBase * f == e +
            kBase * u + kBase * kBase * r + kBase * kBase * kBase * t)

  solver = cp_model.CpSolver()
  status = solver.Solve(model)

  ### Solve model.
  solver = cp_model.CpSolver()
  solution_printer = VarArraySolutionPrinter(letters)
  status = solver.SearchForAllSolutions(model, solution_printer)

  print()
  print('Statistics')
  print('  - conflicts       : %i' % solver.NumConflicts())
  print('  - branches        : %i' % solver.NumBranches())
  print('  - wall time       : %f ms' % solver.WallTime())
  print('  - solutions found : %i' % solution_printer.SolutionCount())


if __name__ == '__main__':
  CPIsFun()
