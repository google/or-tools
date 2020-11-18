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

  Fill-a-Pix problem in Google CP Solver.

  From
  http://www.conceptispuzzles.com/index.aspx?uri=puzzle/fill-a-pix/basiclogic
  '''
  Each puzzle consists of a grid containing clues in various places. The
  object is to reveal a hidden picture by painting the squares around each
  clue so that the number of painted squares, including the square with
  the clue, matches the value of the clue.
  '''

  http://www.conceptispuzzles.com/index.aspx?uri=puzzle/fill-a-pix/rules
  '''
  Fill-a-Pix is a Minesweeper-like puzzle based on a grid with a pixilated
  picture hidden inside. Using logic alone, the solver determines which
  squares are painted and which should remain empty until the hidden picture
  is completely exposed.
  '''

  Fill-a-pix History:
  http://www.conceptispuzzles.com/index.aspx?uri=puzzle/fill-a-pix/history


  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/fill_a_pix.mzn
  * SICStus Prolog: http://www.hakank.org/sicstus/fill_a_pix.pl
  * ECLiPSe: http://hakank.org/eclipse/fill_a_pix.ecl
  * Gecode: http://hakank.org/gecode/fill_a_pix.cpp

  And see the Minesweeper model:
  * http://www.hakank.org/google_or_tools/minesweeper.py


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp

# Puzzle 1 from
# http://www.conceptispuzzles.com/index.aspx?uri=puzzle/fill-a-pix/rules
default_n = 10
X = -1
default_puzzle = [
    [X, X, X, X, X, X, X, X, 0, X], [X, 8, 8, X, 2, X, 0, X, X, X],
    [5, X, 8, X, X, X, X, X, X, X], [X, X, X, X, X, 2, X, X, X, 2],
    [1, X, X, X, 4, 5, 6, X, X, X], [X, 0, X, X, X, 7, 9, X, X, 6],
    [X, X, X, 6, X, X, 9, X, X, 6], [X, X, 6, 6, 8, 7, 8, 7, X, 5],
    [X, 4, X, 6, 6, 6, X, 6, X, 4], [X, X, X, X, X, X, 3, X, X, X]
]


def main(puzzle='', n=''):

  # Create the solver.
  solver = pywrapcp.Solver('Fill-a-Pix')

  #
  # data
  #

  # Set default problem
  if puzzle == '':
    puzzle = default_puzzle
    n = default_n
  else:
    print('n:', n)

  # for the neighbors of 'this' cell
  S = [-1, 0, 1]

  # print problem instance
  print('Problem:')
  for i in range(n):
    for j in range(n):
      if puzzle[i][j] == X:
        sys.stdout.write('.')
      else:
        sys.stdout.write(str(puzzle[i][j]))
    print()
  print()

  #
  # declare variables
  #
  pict = {}
  for i in range(n):
    for j in range(n):
      pict[(i, j)] = solver.IntVar(0, 1, 'pict %i %i' % (i, j))

  pict_flat = [pict[i, j] for i in range(n) for j in range(n)]

  #
  # constraints
  #
  for i in range(n):
    for j in range(n):
      if puzzle[i][j] > X:
        # this cell is the sum of all the surrounding cells
        solver.Add(puzzle[i][j] == solver.Sum([
            pict[i + a, j + b]
            for a in S
            for b in S
            if i + a >= 0 and j + b >= 0 and i + a < n and j + b < n
        ]))

  #
  # solution and search
  #
  db = solver.Phase(pict_flat, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db)
  num_solutions = 0
  print('Solution:')
  while solver.NextSolution():
    num_solutions += 1
    for i in range(n):
      row = [str(pict[i, j].Value()) for j in range(n)]
      for j in range(n):
        if row[j] == '0':
          row[j] = ' '
        else:
          row[j] = '#'
      print(''.join(row))
    print()

  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


#
# Read a problem instance from a file
#
def read_problem(file):
  f = open(file, 'r')
  n = int(f.readline())
  puzzle = []
  for i in range(n):
    x = f.readline()
    row = [0] * n
    for j in range(n):
      if x[j] == '.':
        tmp = -1
      else:
        tmp = int(x[j])
      row[j] = tmp
    puzzle.append(row)
  return [puzzle, n]


if __name__ == '__main__':
  if len(sys.argv) > 1:
    file = sys.argv[1]
    print('Problem instance from', file)
    [puzzle, n] = read_problem(file)
    main(puzzle, n)
  else:
    main()
