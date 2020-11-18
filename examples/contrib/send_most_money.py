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

  SEND+MOST=MONEY in Google CP Solver.


  Alphametic problem were we maximize MONEY.

  Problem from the lecture notes:
  http://www.ict.kth.se/courses/ID2204/notes/L01.pdf

  Compare with the following models:
  * Comet   : http://www.hakank.org/comet/send_most_money.co
  * Comet   : http://www.hakank.org/comet/send_most_money2.co
  * ECLiPSE : http://www.hakank.org/eclipse/send_most_money.ecl
  * SICStus: http://hakank.org/sicstus/send_most_money.pl
  * MiniZinc: http://www.hakank.org/minizinc/send_most_money.mzn
  * Gecode/R: http://www.hakank.org/gecode_r/send_most_money2.rb
  * Tailor/Essence': http://www.hakank.org/tailor/send_most_money.eprime
  * Zinc: http://www.hakank.org/minizinc/send_most_money.zinc


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""

from ortools.constraint_solver import pywrapcp


def main(MONEY=0):

  # Create the solver.
  solver = pywrapcp.Solver('Send most money')

  # data

  # declare variables
  s = solver.IntVar(0, 9, 's')
  e = solver.IntVar(0, 9, 'e')
  n = solver.IntVar(0, 9, 'n')
  d = solver.IntVar(0, 9, 'd')
  m = solver.IntVar(0, 9, 'm')
  o = solver.IntVar(0, 9, 'o')
  t = solver.IntVar(0, 9, 't')
  y = solver.IntVar(0, 9, 'y')
  money = solver.IntVar(0, 100000, 'money')

  x = [s, e, n, d, m, o, t, y]

  #
  # constraints
  #
  if MONEY > 0:
    solver.Add(money == MONEY)

  solver.Add(solver.AllDifferent(x))
  solver.Add(money == m * 10000 + o * 1000 + n * 100 + e * 10 + y)
  solver.Add(money > 0)
  solver.Add(1000 * s + 100 * e + 10 * n + d + 1000 * m + 100 * o + 10 * s +
             t == money)
  solver.Add(s > 0)
  solver.Add(m > 0)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x)
  solution.Add(money)

  collector = solver.AllSolutionCollector(solution)
  objective = solver.Maximize(money, 100)
  cargs = [collector]
  if MONEY == 0:
    objective = solver.Maximize(money, 1)
    cargs.extend([objective])

  solver.Solve(
      solver.Phase(x, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MAX_VALUE),
      cargs)

  num_solutions = collector.SolutionCount()
  money_val = 0
  for s in range(num_solutions):
    print('x:', [collector.Value(s, x[i]) for i in range(len(x))])
    money_val = collector.Value(s, money)
    print('money:', money_val)
    print()

  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime())

  if MONEY == 0:
    return money_val


if __name__ == '__main__':
  # First get the maximised MONEY, and then show all solutions for
  # this value
  print('Minimize money...')
  money = main(0)
  print('\nCheck all solutions for money=%i' % money)
  main(money)
