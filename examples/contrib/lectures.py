# Copyright 2010 Hakan Kjellerstrand hakank@gmail.com
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

  Lectures problem in Google CP Solver.

  Biggs: Discrete Mathematics (2nd ed), page 187.
  '''
  Suppose we wish to schedule six one-hour lectures, v1, v2, v3, v4, v5, v6.
  Among the the potential audience there are people who wish to hear both

   - v1 and v2
   - v1 and v4
   - v3 and v5
   - v2 and v6
   - v4 and v5
   - v5 and v6
   - v1 and v6

  How many hours are necessary in order that the lectures can be given
  without clashes?
  '''

  Compare with the following models:
 * MiniZinc: http://www.hakank.org/minizinc/lectures.mzn
 * SICstus: http://hakank.org/sicstus/lectures.pl
 * ECLiPSe: http://hakank.org/eclipse/lectures.ecl
 * Gecode: http://hakank.org/gecode/lectures.cpp


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Lectures')

  #
  # data
  #

  #
  # The schedule requirements:
  # lecture a cannot be held at the same time as b
  # Note: 1-based
  g = [[1, 2], [1, 4], [3, 5], [2, 6], [4, 5], [5, 6], [1, 6]]

  # number of nodes
  n = 6

  # number of edges
  edges = len(g)

  #
  # declare variables
  #
  v = [solver.IntVar(0, n - 1, 'v[%i]' % i) for i in range(n)]

  # maximum color, to minimize
  # Note: since Python is 0-based, the
  # number of colors is +1
  max_c = solver.IntVar(0, n - 1, 'max_c')

  #
  # constraints
  #
  solver.Add(max_c == solver.Max(v))

  # ensure that there are no clashes
  # also, adjust to 0-base
  for i in range(edges):
    solver.Add(v[g[i][0] - 1] != v[g[i][1] - 1])

  # symmetry breaking:
  # - v0 has the color 0,
  # - v1 has either color 0 or 1
  solver.Add(v[0] == 0)
  solver.Add(v[1] <= 1)

  # objective
  objective = solver.Minimize(max_c, 1)

  #
  # solution and search
  #
  db = solver.Phase(v, solver.CHOOSE_MIN_SIZE_LOWEST_MIN,
                    solver.ASSIGN_CENTER_VALUE)

  solver.NewSearch(db, [objective])

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print('max_c:', max_c.Value() + 1, 'colors')
    print('v:', [v[i].Value() for i in range(n)])
    print()

  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
