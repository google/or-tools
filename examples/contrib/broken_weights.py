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

  Broken weights problem in Google CP Solver.

  From http://www.mathlesstraveled.com/?p=701
  '''
  Here's a fantastic problem I recently heard. Apparently it was first
  posed by Claude Gaspard Bachet de Meziriac in a book of arithmetic problems
  published in 1612, and can also be found in Heinrich Dorrie's 100
  Great Problems of Elementary Mathematics.

      A merchant had a forty pound measuring weight that broke
      into four pieces as the result of a fall. When the pieces were
      subsequently weighed, it was found that the weight of each piece
      was a whole number of pounds and that the four pieces could be
      used to weigh every integral weight between 1 and 40 pounds. What
      were the weights of the pieces?

  Note that since this was a 17th-century merchant, he of course used a
  balance scale to weigh things. So, for example, he could use a 1-pound
  weight and a 4-pound weight to weigh a 3-pound object, by placing the
  3-pound object and 1-pound weight on one side of the scale, and
  the 4-pound weight on the other side.
  '''

  Compare with the following problems:
  * MiniZinc: http://www.hakank.org/minizinc/broken_weights.mzn
  * ECLiPSE: http://www.hakank.org/eclipse/broken_weights.ecl
  * Gecode: http://www.hakank.org/gecode/broken_weights.cpp
  * Comet: http://hakank.org/comet/broken_weights.co

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys

from ortools.constraint_solver.python import constraint_solver as cp


def main(m=40, n=4):

  # Create the solver.
  solver = cp.Solver('Broken weights')

  #
  # data
  #
  print('total weight (m):', m)
  print('number of pieces (n):', n)
  print()

  #
  # variables
  #
  weights = [solver.new_int_var(1, m, 'weights[%i]' % j) for j in range(n)]
  x = {}
  for i in range(m):
    for j in range(n):
      x[i, j] = solver.new_int_var(-1, 1, 'x[%i,%i]' % (i, j))
  x_flat = [x[i, j] for i in range(m) for j in range(n)]

  #
  # constraints
  #

  # symmetry breaking
  for j in range(1, n):
    solver.add(weights[j - 1] < weights[j])

  solver.add_sum_equality(weights, m)

  # Check that all weights from 1 to 40 can be made.
  #
  # Since all weights can be on either side
  # of the side of the scale we allow either
  # -1, 0, or 1 or the weights, assuming that
  # -1 is the weights on the left and 1 is on the right.
  #
  for i in range(m):
    solver.add(i + 1 == solver.sum([weights[j] * x[i, j] for j in range(n)]))

  # objective
  objective = solver.minimize(weights[n - 1], 1)

  #
  # search and result
  #
  db = solver.phase(weights + x_flat, cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
                    cp.IntValueStrategy.ASSIGN_MIN_VALUE)

  search_log = solver.search_log(1)

  solver.new_search(db, [objective])

  num_solutions = 0
  while solver.next_solution():
    num_solutions += 1
    print('weights:   ', end=' ')
    for w in [weights[j].value() for j in range(n)]:
      print('%3i ' % w, end=' ')
    print()
    print('-' * 30)
    for i in range(m):
      print('weight  %2i:' % (i + 1), end=' ')
      for j in range(n):
        print('%3i ' % x[i, j].value(), end=' ')
      print()
    print()
  print()
  solver.end_search()

  print('num_solutions:', num_solutions)
  print('failures :', solver.num_failures)
  print('branches :', solver.num_branches)
  print('WallTime:', solver.wall_time_ms, 'ms')


m = 40
n = 4
if __name__ == '__main__':
  if len(sys.argv) > 1:
    m = int(sys.argv[1])
  if len(sys.argv) > 2:
    n = int(sys.argv[2])
  main(m, n)
