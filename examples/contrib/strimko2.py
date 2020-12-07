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
"""Strimko problem in Google CP Solver.

  From
  360: A New Twist on Latin Squares
  http://threesixty360.wordpress.com/2009/08/04/a-new-twist-on-latin-squares/
  '''
  The idea is simple: each row and column of an nxn grid must contain
  the number 1, 2, ... n exactly once (that is, the grid must form a
  Latin square), and each "stream" (connected path in the grid) must
  also contain the numbers 1, 2, ..., n exactly once.
  '''

  For more information, see:
  * http://www.strimko.com/
  * http://www.strimko.com/rules.htm
  * http://www.strimko.com/about.htm
  * http://www.puzzlersparadise.com/Strimko.htm

  I have blogged about this (using MiniZinc model) in
  'Strimko - Latin squares puzzle with "streams"'
  http://www.hakank.org/constraint_programming_blog/2009/08/strimko_latin_squares_puzzle_w_1.html

  Compare with the following models:
  * MiniZinc: http://hakank.org/minizinc/strimko2.mzn
  * ECLiPSe: http://hakank.org/eclipse/strimko2.ecl
  * SICStus: http://hakank.org/sicstus/strimko2.pl
  * Gecode: http://hakank.org/gecode/strimko2.cpp

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  See my other Google CP Solver models: http://www.hakank.org/google_or_tools/
"""
import sys

from ortools.constraint_solver import pywrapcp


def main(streams='', placed=''):

  # Create the solver.
  solver = pywrapcp.Solver('Strimko')

  #
  # default problem
  #
  if streams == '':
    streams = [[1, 1, 2, 2, 2, 2, 2], [1, 1, 2, 3, 3, 3, 2],
               [1, 4, 1, 3, 3, 5, 5], [4, 4, 3, 1, 3, 5, 5],
               [4, 6, 6, 6, 7, 7, 5], [6, 4, 6, 4, 5, 5, 7],
               [6, 6, 4, 7, 7, 7, 7]]

    # Note: This is 1-based
    placed = [[2, 1, 1], [2, 3, 7], [2, 5, 6], [2, 7, 4], [3, 2, 7], [3, 6, 1],
              [4, 1, 4], [4, 7, 5], [5, 2, 2], [5, 6, 6]]

  n = len(streams)
  num_placed = len(placed)

  print('n:', n)

  #
  # variables
  #

  x = {}
  for i in range(n):
    for j in range(n):
      x[i, j] = solver.IntVar(1, n, 'x[%i,%i]' % (i, j))

  x_flat = [x[i, j] for i in range(n) for j in range(n)]

  #
  # constraints
  #

  # all rows and columns must be unique, i.e. a Latin Square
  for i in range(n):
    row = [x[i, j] for j in range(n)]
    solver.Add(solver.AllDifferent(row))

    col = [x[j, i] for j in range(n)]
    solver.Add(solver.AllDifferent(col))

  #
  # streams
  #
  for s in range(1, n + 1):
    tmp = [x[i, j] for i in range(n) for j in range(n) if streams[i][j] == s]
    solver.Add(solver.AllDifferent(tmp))

  #
  # placed
  #
  for i in range(num_placed):
    # note: also adjust to 0-based
    solver.Add(x[placed[i][0] - 1, placed[i][1] - 1] == placed[i][2])

  #
  # search and solution
  #
  db = solver.Phase(x_flat, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    for i in range(n):
      for j in range(n):
        print(x[i, j].Value(), end=' ')
      print()

    print()
    num_solutions += 1

  solver.EndSearch()

  print()
  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  if len(sys.argv) > 1:
    problem_file = sys.argv[1]
    exec(compile(open(problem_file).read(), problem_file, 'exec'))
    main(streams, placed)
  else:
    main()
