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

  Eq 10 in Google CP Solver.

  Standard benchmark problem.

  Compare with the following models:
  * MiniZinc: http://hakank.org/minizinc/eq10.mzn
  * ECLiPSe: http://hakank.org/eclipse/eq10.ecl
  * SICStus: http://hakank.org/sicstus/eq10.pl

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver("Eq 10")

  #
  # data
  #
  n = 7

  #
  # variables
  #
  X = [solver.IntVar(0, 10, "X(%i)" % i) for i in range(n)]
  X1, X2, X3, X4, X5, X6, X7 = X

  #
  # constraints
  #
  solver.Add(0 + 98527 * X1 + 34588 * X2 + 5872 * X3 + 59422 * X5 +
             65159 * X7 == 1547604 + 30704 * X4 + 29649 * X6)

  solver.Add(0 + 98957 * X2 + 83634 * X3 + 69966 * X4 + 62038 * X5 +
             37164 * X6 + 85413 * X7 == 1823553 + 93989 * X1)

  solver.Add(900032 + 10949 * X1 + 77761 * X2 + 67052 * X5 == 0 + 80197 * X3 +
             61944 * X4 + 92964 * X6 + 44550 * X7)

  solver.Add(0 + 73947 * X1 + 84391 * X3 + 81310 * X5 == 1164380 + 96253 * X2 +
             44247 * X4 + 70582 * X6 + 33054 * X7)

  solver.Add(0 + 13057 * X3 + 42253 * X4 + 77527 * X5 + 96552 * X7 == 1185471 +
             60152 * X1 + 21103 * X2 + 97932 * X6)

  solver.Add(1394152 + 66920 * X1 + 55679 * X4 == 0 + 64234 * X2 + 65337 * X3 +
             45581 * X5 + 67707 * X6 + 98038 * X7)

  solver.Add(0 + 68550 * X1 + 27886 * X2 + 31716 * X3 + 73597 * X4 +
             38835 * X7 == 279091 + 88963 * X5 + 76391 * X6)

  solver.Add(0 + 76132 * X2 + 71860 * X3 + 22770 * X4 + 68211 * X5 +
             78587 * X6 == 480923 + 48224 * X1 + 82817 * X7)

  solver.Add(519878 + 94198 * X2 + 87234 * X3 + 37498 * X4 == 0 + 71583 * X1 +
             25728 * X5 + 25495 * X6 + 70023 * X7)

  solver.Add(361921 + 78693 * X1 + 38592 * X5 + 38478 * X6 == 0 + 94129 * X2 +
             43188 * X3 + 82528 * X4 + 69025 * X7)

  #
  # search and result
  #
  db = solver.Phase(X, solver.INT_VAR_SIMPLE, solver.INT_VALUE_SIMPLE)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print("X:", [X[i].Value() for i in range(n)])
    print()

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main()
