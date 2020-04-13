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

  KenKen puzzle in Google CP Solver.

  http://en.wikipedia.org/wiki/KenKen
  '''
  KenKen or KEN-KEN is a style of arithmetic and logical puzzle sharing
  several characteristics with sudoku. The name comes from Japanese and
  is translated as 'square wisdom' or 'cleverness squared'.
  ...
  The objective is to fill the grid in with the digits 1 through 6 such that:

    * Each row contains exactly one of each digit
    * Each column contains exactly one of each digit
    * Each bold-outlined group of cells is a cage containing digits which
      achieve the specified result using the specified mathematical operation:
        addition (+),
        subtraction (-),
        multiplication (x),
        and division (/).
        (Unlike in Killer sudoku, digits may repeat within a group.)

  ...
  More complex KenKen problems are formed using the principles described
  above but omitting the symbols +, -, x and /, thus leaving them as
  yet another unknown to be determined.
  '''


  The solution is:

    5 6 3 4 1 2
    6 1 4 5 2 3
    4 5 2 3 6 1
    3 4 1 2 5 6
    2 3 6 1 4 5
    1 2 5 6 3 4



  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys

from ortools.constraint_solver import pywrapcp
from functools import reduce

#
# Ensure that the sum of the segments
# in cc == res
#


def calc(cc, x, res):

  solver = list(x.values())[0].solver()

  if len(cc) == 2:

    # for two operands there may be
    # a lot of variants

    c00, c01 = cc[0]
    c10, c11 = cc[1]
    a = x[c00 - 1, c01 - 1]
    b = x[c10 - 1, c11 - 1]

    r1 = solver.IsEqualCstVar(a + b, res)
    r2 = solver.IsEqualCstVar(a * b, res)
    r3 = solver.IsEqualVar(a * res, b)
    r4 = solver.IsEqualVar(b * res, a)
    r5 = solver.IsEqualCstVar(a - b, res)
    r6 = solver.IsEqualCstVar(b - a, res)
    solver.Add(r1 + r2 + r3 + r4 + r5 + r6 >= 1)

  else:

    # res is either sum or product of the segment

    xx = [x[i[0] - 1, i[1] - 1] for i in cc]

    # Sum
    # # SumEquality don't work:
    # this_sum = solver.SumEquality(xx, res)
    this_sum = solver.IsEqualCstVar(solver.Sum(xx), res)

    # Product
    # # Prod (or MakeProd) don't work:
    # this_prod = solver.IsEqualCstVar(solver.Prod(xx), res)
    this_prod = solver.IsEqualCstVar(reduce(lambda a, b: a * b, xx), res)
    solver.Add(this_sum + this_prod >= 1)


def main():

  # Create the solver.
  solver = pywrapcp.Solver("KenKen")

  #
  # data
  #

  # size of matrix
  n = 6

  # For a better view of the problem, see
  #  http://en.wikipedia.org/wiki/File:KenKenProblem.svg

  # hints
  #    [sum, [segments]]
  # Note: 1-based
  problem = [[11, [[1, 1], [2, 1]]], [2, [[1, 2], [1, 3]]],
             [20, [[1, 4], [2, 4]]], [6, [[1, 5], [1, 6], [2, 6], [3, 6]]],
             [3, [[2, 2], [2, 3]]], [3, [[2, 5], [3, 5]]],
             [240, [[3, 1], [3, 2], [4, 1], [4, 2]]], [6, [[3, 3], [3, 4]]],
             [6, [[4, 3], [5, 3]]], [7, [[4, 4], [5, 4], [5, 5]]],
             [30, [[4, 5], [4, 6]]], [6, [[5, 1], [5, 2]]],
             [9, [[5, 6], [6, 6]]], [8, [[6, 1], [6, 2], [6, 3]]],
             [2, [[6, 4], [6, 5]]]]

  num_p = len(problem)

  #
  # variables
  #

  # the set
  x = {}
  for i in range(n):
    for j in range(n):
      x[i, j] = solver.IntVar(1, n, "x[%i,%i]" % (i, j))

  x_flat = [x[i, j] for i in range(n) for j in range(n)]

  #
  # constraints
  #

  # all rows and columns must be unique
  for i in range(n):
    row = [x[i, j] for j in range(n)]
    solver.Add(solver.AllDifferent(row))

    col = [x[j, i] for j in range(n)]
    solver.Add(solver.AllDifferent(col))

  # calculate the segments
  for (res, segment) in problem:
    calc(segment, x, res)

  #
  # search and solution
  #
  db = solver.Phase(x_flat, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    for i in range(n):
      for j in range(n):
        print(x[i, j].Value(), end=" ")
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
