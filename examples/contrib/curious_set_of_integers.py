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

  Crypto problem in Google CP Solver.

  Martin Gardner (February 1967):
  '''
  The integers 1,3,8, and 120 form a set with a remarkable property: the
  product of any two integers is one less than a perfect square. Find
  a fifth number that can be added to the set without destroying
  this property.
  '''

  Solution: The number is 0.

  There are however other sets of five numbers with this property.
  Here are the one in the range of 0.10000:
  [0, 1, 3, 8, 120]
  [0, 1, 3, 120, 1680]
  [0, 1, 8, 15, 528]
  [0, 1, 8, 120, 4095]
  [0, 1, 15, 24, 1520]
  [0, 1, 24, 35, 3480]
  [0, 1, 35, 48, 6888]
  [0, 2, 4, 12, 420]
  [0, 2, 12, 24, 2380]
  [0, 2, 24, 40, 7812]
  [0, 3, 5, 16, 1008]
  [0, 3, 8, 21, 2080]
  [0, 3, 16, 33, 6440]
  [0, 4, 6, 20, 1980]
  [0, 4, 12, 30, 5852]
  [0, 5, 7, 24, 3432]
  [0, 6, 8, 28, 5460]
  [0, 7, 9, 32, 8160]


  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/crypta.mzn
  * Comet   : http://www.hakank.org/comet/crypta.co
  * ECLiPSe : http://www.hakank.org/eclipse/crypta.ecl
  * SICStus : http://hakank.org/sicstus/crypta.pl


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.constraint_solver import pywrapcp


def decreasing(solver, x):
  for i in range(len(x) - 1):
    solver.Add(x[i] <= x[i + 1])


def main():

  # Create the solver.
  solver = pywrapcp.Solver("Curious set of integers")

  #
  # data
  #
  n = 5
  max_val = 10000

  #
  # variables
  #
  x = [solver.IntVar(0, max_val, "x[%i]" % i) for i in range(n)]

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(x))
  decreasing(solver, x)

  for i in range(n):
    for j in range(n):
      if i != j:
        p = solver.IntVar(0, max_val, "p[%i,%i]" % (i, j))
        solver.Add(p * p - 1 == (x[i] * x[j]))

  # This is the original problem:
  # Which is the fifth number?
  v = [1, 3, 8, 120]
  b = [solver.IsMemberVar(x[i], v) for i in range(n)]
  solver.Add(solver.Sum(b) == 4)

  #
  # search and result
  #
  db = solver.Phase(x, solver.CHOOSE_MIN_SIZE_LOWEST_MIN,
                    solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print("x:", [int(x[i].Value()) for i in range(n)])

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main()
