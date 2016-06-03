# Copyright 2010 Hakan Kjellerstrand hakank@bonetmail.com
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

  Kakuru puzzle in Google CP Solver.

  http://en.wikipedia.org/wiki/Kakuro
  '''
  The object of the puzzle is to insert a digit from 1 to 9 inclusive
  into each white cell such that the sum of the numbers in each entry
  matches the clue associated with it and that no digit is duplicated in
  any entry. It is that lack of duplication that makes creating Kakuro
  puzzles with unique solutions possible, and which means solving a Kakuro
  puzzle involves investigating combinations more, compared to Sudoku in
  which the focus is on permutations. There is an unwritten rule for
  making Kakuro puzzles that each clue must have at least two numbers
  that add up to it. This is because including one number is mathematically
  trivial when solving Kakuro puzzles; one can simply disregard the
  number entirely and subtract it from the clue it indicates.
  '''

  This model solves the problem at the Wikipedia page.
  For a larger picture, see
  http://en.wikipedia.org/wiki/File:Kakuro_black_box.svg

  The solution:
    9 7 0 0 8 7 9
    8 9 0 8 9 5 7
    6 8 5 9 7 0 0
    0 6 1 0 2 6 0
    0 0 4 6 1 3 2
    8 9 3 1 0 1 4
    3 1 2 0 0 2 1

  Compare with the following models:
  * Comet   : http://www.hakank.org/comet/kakuro.co
  * MiniZinc: http://www.hakank.org/minizinc/kakuro.mzn
  * SICStus : http://www.hakank.org/sicstus/kakuro.pl
  * ECLiPSe: http://www.hakank.org/eclipse/kakuro.ecl
  * Gecode: http://www.hakank.org/gecode/kenken2.cpp

  This model was created by Hakan Kjellerstrand (hakank@bonetmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from __future__ import print_function
import sys

from ortools.constraint_solver import pywrapcp

#
# Ensure that the sum of the segments
# in cc == res
#


def calc(cc, x, res):

  solver = list(x.values())[0].solver()

  # ensure that the values are positive
  for i in cc:
    solver.Add(x[i[0] - 1, i[1] - 1] >= 1)

  # sum the numbers
  solver.Add(solver.Sum([x[i[0] - 1, i[1] - 1] for i in cc]) == res)


def main():

  # Create the solver.
  solver = pywrapcp.Solver("Kakuro")

  #
  # data
  #

  # size of matrix
  n = 7

  # segments
  #    [sum, [segments]]
  # Note: 1-based
  problem = [
      [16, [1, 1], [1, 2]],
      [24, [1, 5], [1, 6], [1, 7]],
      [17, [2, 1], [2, 2]],
      [29, [2, 4], [2, 5], [2, 6], [2, 7]],
      [35, [3, 1], [3, 2], [3, 3], [3, 4], [3, 5]],
      [7, [4, 2], [4, 3]],
      [8, [4, 5], [4, 6]],
      [16, [5, 3], [5, 4], [5, 5], [5, 6], [5, 7]],
      [21, [6, 1], [6, 2], [6, 3], [6, 4]],
      [5, [6, 6], [6, 7]],
      [6, [7, 1], [7, 2], [7, 3]],
      [3, [7, 6], [7, 7]],

      [23, [1, 1], [2, 1], [3, 1]],
      [30, [1, 2], [2, 2], [3, 2], [4, 2]],
      [27, [1, 5], [2, 5], [3, 5], [4, 5], [5, 5]],
      [12, [1, 6], [2, 6]],
      [16, [1, 7], [2, 7]],
      [17, [2, 4], [3, 4]],
      [15, [3, 3], [4, 3], [5, 3], [6, 3], [7, 3]],
      [12, [4, 6], [5, 6], [6, 6], [7, 6]],
      [7, [5, 4], [6, 4]],
      [7, [5, 7], [6, 7], [7, 7]],
      [11, [6, 1], [7, 1]],
      [10, [6, 2], [7, 2]]
  ]

  num_p = len(problem)

  # The blanks
  # Note: 1-based
  blanks = [
      [1, 3], [1, 4],
      [2, 3],
      [3, 6], [3, 7],
      [4, 1], [4, 4], [4, 7],
      [5, 1], [5, 2],
      [6, 5],
      [7, 4], [7, 5]
  ]
  num_blanks = len(blanks)

  #
  # variables
  #

  # the set
  x = {}
  for i in range(n):
    for j in range(n):
      x[i, j] = solver.IntVar(0, 9, "x[%i,%i]" % (i, j))

  x_flat = [x[i, j] for i in range(n) for j in range(n)]

  #
  # constraints
  #

  # fill the blanks with 0
  for i in range(num_blanks):
    solver.Add(x[blanks[i][0] - 1, blanks[i][1] - 1] == 0)

  for i in range(num_p):
    segment = problem[i][1::]
    res = problem[i][0]

    # sum this segment
    calc(segment, x, res)

    # all numbers in this segment must be distinct
    segment = [x[p[0] - 1, p[1] - 1] for p in segment]
    solver.Add(solver.AllDifferent(segment))

  #
  # search and solution
  #
  db = solver.Phase(x_flat,
                    solver.INT_VAR_DEFAULT,
                    solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    for i in range(n):
      for j in range(n):
        val = x[i, j].Value()
        if val > 0:
          print(val, end=' ')
        else:
          print(" ", end=' ')
      print()

    print()
    num_solutions += 1

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main()
