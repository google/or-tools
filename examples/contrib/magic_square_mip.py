# Copyright 2011 Hakan Kjellerstrand hakank@gmail.com
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

  Magic square (integer programming) in Google or-tools.

  Translated from GLPK:s example magic.mod
  '''
  MAGIC, Magic Square

  Written in GNU MathProg by Andrew Makhorin <mao@mai2.rcnet.ru>

  In recreational mathematics, a magic square of order n is an
  arrangement of n^2 numbers, usually distinct integers, in a square,
  such that n numbers in all rows, all columns, and both diagonals sum
  to the same constant. A normal magic square contains the integers
  from 1 to n^2.

  (From Wikipedia, the free encyclopedia.)
  '''

  Compare to the CP version:
     http://www.hakank.org/google_or_tools/magic_square.py

  Here we also experiment with how long it takes when
  using an output_matrix (much longer).


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.linear_solver import pywraplp

#
# main(n, use_output_matrix)
#   n: size of matrix
#   use_output_matrix: use the output_matrix
#


def main(n=3, sol='CBC', use_output_matrix=0):

  # Create the solver.

  print('Solver: ', sol)

  # using GLPK
  if sol == 'GLPK':
    solver = pywraplp.Solver('CoinsGridGLPK',
                             pywraplp.Solver.GLPK_MIXED_INTEGER_PROGRAMMING)
  else:
    # Using CLP
    solver = pywraplp.Solver('CoinsGridCLP',
                             pywraplp.Solver.CBC_MIXED_INTEGER_PROGRAMMING)

  #
  # data
  #
  print('n = ', n)

  # range_n = range(1, n+1)
  range_n = list(range(0, n))

  N = n * n
  range_N = list(range(1, N + 1))

  #
  # variables
  #

  # x[i,j,k] = 1 means that cell (i,j) contains integer k
  x = {}
  for i in range_n:
    for j in range_n:
      for k in range_N:
        x[i, j, k] = solver.IntVar(0, 1, 'x[%i,%i,%i]' % (i, j, k))

  # For output. Much slower....
  if use_output_matrix == 1:
    print('Using an output matrix')
    square = {}
    for i in range_n:
      for j in range_n:
        square[i, j] = solver.IntVar(1, n * n, 'square[%i,%i]' % (i, j))

  # the magic sum
  s = solver.IntVar(1, n * n * n, 's')

  #
  # constraints
  #

  # each cell must be assigned exactly one integer
  for i in range_n:
    for j in range_n:
      solver.Add(solver.Sum([x[i, j, k] for k in range_N]) == 1)

  # each integer must be assigned exactly to one cell
  for k in range_N:
    solver.Add(solver.Sum([x[i, j, k] for i in range_n for j in range_n]) == 1)

  # # the sum in each row must be the magic sum
  for i in range_n:
    solver.Add(
        solver.Sum([k * x[i, j, k] for j in range_n for k in range_N]) == s)

  # # the sum in each column must be the magic sum
  for j in range_n:
    solver.Add(
        solver.Sum([k * x[i, j, k] for i in range_n for k in range_N]) == s)

  # # the sum in the diagonal must be the magic sum
  solver.Add(
      solver.Sum([k * x[i, i, k] for i in range_n for k in range_N]) == s)

  # # the sum in the co-diagonal must be the magic sum
  if range_n[0] == 1:
    # for range_n = 1..n
    solver.Add(
        solver.Sum([k * x[i, n - i + 1, k]
                    for i in range_n
                    for k in range_N]) == s)
  else:
    # for range_n = 0..n-1
    solver.Add(
        solver.Sum([k * x[i, n - i - 1, k]
                    for i in range_n
                    for k in range_N]) == s)

  # for output
  if use_output_matrix == 1:
    for i in range_n:
      for j in range_n:
        solver.Add(
            square[i, j] == solver.Sum([k * x[i, j, k] for k in range_N]))

  #
  # solution and search
  #
  solver.Solve()

  print()

  print('s: ', int(s.SolutionValue()))
  if use_output_matrix == 1:
    for i in range_n:
      for j in range_n:
        print(int(square[i, j].SolutionValue()), end=' ')
      print()
    print()
  else:
    for i in range_n:
      for j in range_n:
        print(
            sum([int(k * x[i, j, k].SolutionValue()) for k in range_N]),
            ' ',
            end=' ')
      print()

  print('\nx:')
  for i in range_n:
    for j in range_n:
      for k in range_N:
        print(int(x[i, j, k].SolutionValue()), end=' ')
      print()

  print()
  print('walltime  :', solver.WallTime(), 'ms')
  if sol == 'CBC':
    print('iterations:', solver.Iterations())


if __name__ == '__main__':
  n = 3
  sol = 'CBC'
  use_output_matrix = 0
  if len(sys.argv) > 1:
    n = int(sys.argv[1])

  if len(sys.argv) > 2:
    sol = sys.argv[2]
    if sol != 'GLPK' and sol != 'CBC':
      print('Solver must be either GLPK or CBC')
      sys.exit(1)

  if len(sys.argv) > 3:
    use_output_matrix = int(sys.argv[3])

  main(n, sol, use_output_matrix)
