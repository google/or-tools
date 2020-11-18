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

  SEND+MORE=MONEY in 'any' base in Google CP Solver.

  Alphametic problem SEND+MORE=MONEY in any base.

  Examples:
  Base 10 has one solution:
     {9, 5, 6, 7, 1, 0, 8, 2}
  Base 11 has three soltutions:
     {10, 5, 6, 8, 1, 0, 9, 2}
     {10, 6, 7, 8, 1, 0, 9, 3}
     {10, 7, 8, 6, 1, 0, 9, 2}

  Also, compare with the following models:
  * Comet   : http://www.hakank.org/comet/send_more_money_any_base.co
  * ECLiPSE : http://www.hakank.org/eclipse/send_more_money_any_base.ecl
  * Essence : http://www.hakank.org/tailor/send_more_money_any_base.eprime
  * Gecode  : http://www.hakank.org/gecode/send_more_money_any_base.cpp
  * Gecode/R: http://www.hakank.org/gecode_r/send_more_money_any_base.rb
  * MiniZinc: http://www.hakank.org/minizinc/send_more_money_any_base.mzn
  * Zinc: http://www.hakank.org/minizinc/send_more_money_any_base.zinc
  * SICStus: http://www.hakank.org/sicstus/send_more_money_any_base.pl


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""
import sys
from ortools.constraint_solver import pywrapcp


def main(base=10):

  # Create the solver.
  solver = pywrapcp.Solver('Send most money')

  # data
  print('base:', base)

  # declare variables
  s = solver.IntVar(0, base - 1, 's')
  e = solver.IntVar(0, base - 1, 'e')
  n = solver.IntVar(0, base - 1, 'n')
  d = solver.IntVar(0, base - 1, 'd')
  m = solver.IntVar(0, base - 1, 'm')
  o = solver.IntVar(0, base - 1, 'o')
  r = solver.IntVar(0, base - 1, 'r')
  y = solver.IntVar(0, base - 1, 'y')

  x = [s, e, n, d, m, o, r, y]

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(x))
  solver.Add(
      s * base**3 + e * base**2 + n * base + d + m * base**3 + o * base**2 +
      r * base + e == m * base**4 + o * base**3 + n * base**2 + e * base + y,)
  solver.Add(s > 0)
  solver.Add(m > 0)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x)

  collector = solver.AllSolutionCollector(solution)

  solver.Solve(
      solver.Phase(x, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MAX_VALUE),
      [collector])

  num_solutions = collector.SolutionCount()
  money_val = 0
  for s in range(num_solutions):
    print('x:', [collector.Value(s, x[i]) for i in range(len(x))])

  print()
  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime())
  print()


base = 10
if __name__ == '__main__':
  # for base in range(10,30):
  #    main(base)
  if len(sys.argv) > 1:
    base = int(sys.argv[1])

  main(base)
