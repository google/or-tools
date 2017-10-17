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

"""Send + more = money.

In this model, we try to solve the following cryptarythm
SEND + MORE = MONEY
Each letter corresponds to one figure and all letters have different values.
"""

from __future__ import print_function
from ortools.constraint_solver import pywrapcp


def main():
  # Create the solver.
  solver = pywrapcp.Solver('SEND + MORE = MONEY')

  digits = list(range(0, 10))
  s = solver.IntVar(digits, 's')
  e = solver.IntVar(digits, 'e')
  n = solver.IntVar(digits, 'n')
  d = solver.IntVar(digits, 'd')
  m = solver.IntVar(digits, 'm')
  o = solver.IntVar(digits, 'o')
  r = solver.IntVar(digits, 'r')
  y = solver.IntVar(digits, 'y')

  letters = [s, e, n, d, m, o, r, y]

  solver.Add(
      1000 * s + 100 * e + 10 * n + d +
      1000 * m + 100 * o + 10 * r + e ==
      10000 * m + 1000 * o + 100 * n + 10 * e + y)

  # pylint: disable=g-explicit-bool-comparison
  solver.Add(s != 0)
  solver.Add(m != 0)

  solver.Add(solver.AllDifferent(letters))

  solver.NewSearch(solver.Phase(letters,
                                solver.INT_VAR_DEFAULT,
                                solver.INT_VALUE_DEFAULT))
  solver.NextSolution()
  print(letters)
  solver.EndSearch()


if __name__ == '__main__':
  main()
