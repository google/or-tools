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

  Secret Santa problem in Google CP Solver.

  From Ruby Quiz Secret Santa
  http://www.rubyquiz.com/quiz2.html
  '''
  Honoring a long standing tradition started by my wife's dad, my friends
  all play a Secret Santa game around Christmas time. We draw names and
  spend a week sneaking that person gifts and clues to our identity. On the
  last night of the game, we get together, have dinner, share stories, and,
  most importantly, try to guess who our Secret Santa was. It's a crazily
  fun way to enjoy each other's company during the holidays.

  To choose Santas, we use to draw names out of a hat. This system was
  tedious, prone to many 'Wait, I got myself...' problems. This year, we
  made a change to the rules that further complicated picking and we knew
  the hat draw would not stand up to the challenge. Naturally, to solve
  this problem, I scripted the process. Since that turned out to be more
  interesting than I had expected, I decided to share.

  This weeks Ruby Quiz is to implement a Secret Santa selection script.

  Your script will be fed a list of names on STDIN.
  ...
  Your script should then choose a Secret Santa for every name in the list.
  Obviously, a person cannot be their own Secret Santa. In addition, my friends
  no longer allow people in the same family to be Santas for each other and your
  script should take this into account.
  '''

  Comment: This model skips the file input and mail parts. We
           assume that the friends are identified with a number from 1..n,
           and the families is identified with a number 1..num_families.

  Compare with the following model:
  * MiniZinc: http://www.hakank.org/minizinc/secret_santa.mzn


  This model gives 4089600 solutions and the following statistics:
  - failures: 31264
  - branches: 8241726
  - WallTime: 23735 ms (note: without any printing of the solutions)

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Secret Santa problem')

  #
  # data
  #
  family = [1, 1, 1, 1, 2, 3, 3, 3, 3, 3, 4, 4]
  num_families = max(family)
  n = len(family)

  #
  # declare variables
  #
  x = [solver.IntVar(0, n - 1, 'x[%i]' % i) for i in range(n)]

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(x))

  # Can't be one own's Secret Santa
  # Ensure that there are no fix-point in the array
  for i in range(n):
    solver.Add(x[i] != i)

  # No Secret Santa to a person in the same family
  for i in range(n):
    solver.Add(family[i] != solver.Element(family, x[i]))

  #
  # solution and search
  #
  db = solver.Phase(x, solver.INT_VAR_SIMPLE, solver.INT_VALUE_SIMPLE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print('x:', [x[i].Value() for i in range(n)])
    print()

  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
