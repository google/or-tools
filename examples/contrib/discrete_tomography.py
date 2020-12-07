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

  Discrete tomography in Google CP Solver.

  Problem from http://eclipse.crosscoreop.com/examples/tomo.ecl.txt
  '''
  This is a little 'tomography' problem, taken from an old issue
  of Scientific American.

  A matrix which contains zeroes and ones gets "x-rayed" vertically and
  horizontally, giving the total number of ones in each row and column.
  The problem is to reconstruct the contents of the matrix from this
  information. Sample run:

  ?- go.
    0 0 7 1 6 3 4 5 2 7 0 0
 0
 0
 8      * * * * * * * *
 2      *             *
 6      *   * * * *   *
 4      *   *     *   *
 5      *   *   * *   *
 3      *   *         *
 7      *   * * * * * *
 0
 0

 Eclipse solution by Joachim Schimpf, IC-Parc
 '''

 Compare with the following models:
 * Comet: http://www.hakank.org/comet/discrete_tomography.co
 * Gecode: http://www.hakank.org/gecode/discrete_tomography.cpp
 * MiniZinc: http://www.hakank.org/minizinc/tomography.mzn
 * Tailor/Essence': http://www.hakank.org/tailor/tomography.eprime
 * SICStus: http://hakank.org/sicstus/discrete_tomography.pl

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def main(row_sums="", col_sums=""):

  # Create the solver.
  solver = pywrapcp.Solver("n-queens")

  #
  # data
  #
  if row_sums == "":
    print("Using default problem instance")
    row_sums = [0, 0, 8, 2, 6, 4, 5, 3, 7, 0, 0]
    col_sums = [0, 0, 7, 1, 6, 3, 4, 5, 2, 7, 0, 0]

  r = len(row_sums)
  c = len(col_sums)

  # declare variables
  x = []
  for i in range(r):
    t = []
    for j in range(c):
      t.append(solver.IntVar(0, 1, "x[%i,%i]" % (i, j)))
    x.append(t)
  x_flat = [x[i][j] for i in range(r) for j in range(c)]

  #
  # constraints
  #
  [
      solver.Add(solver.Sum([x[i][j]
                             for j in range(c)]) == row_sums[i])
      for i in range(r)
  ]
  [
      solver.Add(solver.Sum([x[i][j]
                             for i in range(r)]) == col_sums[j])
      for j in range(c)
  ]

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x_flat)

  # db: DecisionBuilder
  db = solver.Phase(x_flat, solver.INT_VAR_SIMPLE, solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    print_solution(x, r, c, row_sums, col_sums)
    print()

    num_solutions += 1
  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


#
# Print solution
#


def print_solution(x, rows, cols, row_sums, col_sums):
  print("  ", end=" ")
  for j in range(cols):
    print(col_sums[j], end=" ")
  print()
  for i in range(rows):
    print(row_sums[i], end=" ")
    for j in range(cols):
      if x[i][j].Value() == 1:
        print("#", end=" ")
      else:
        print(".", end=" ")
    print("")


#
# Read a problem instance from a file
#
def read_problem(file):
  f = open(file, "r")
  row_sums = f.readline()
  col_sums = f.readline()
  row_sums = [int(r) for r in (row_sums.rstrip()).split(",")]
  col_sums = [int(c) for c in (col_sums.rstrip()).split(",")]

  return [row_sums, col_sums]


if __name__ == "__main__":
  if len(sys.argv) > 1:
    file = sys.argv[1]
    print("Problem instance from", file)
    [row_sums, col_sums] = read_problem(file)
    main(row_sums, col_sums)
  else:
    main()
