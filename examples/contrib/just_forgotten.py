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

  Just forgotten puzzle (Enigma 1517) in Google CP Solver.

  From http://www.f1compiler.com/samples/Enigma 201517.f1.html
  '''
  Enigma 1517 Bob Walker, New Scientist magazine, October 25, 2008.

  Joe was furious when he forgot one of his bank account numbers.
  He remembered that it had all the digits 0 to 9 in some order,
  so he tried the following four sets without success:

      9 4 6 2 1 5 7 8 3 0
      8 6 0 4 3 9 1 2 5 7
      1 6 4 0 2 9 7 8 5 3
      6 8 2 4 3 1 9 0 7 5

  When Joe finally remembered his account number, he realised that
  in each set just four of the digits were in their correct position
  and that, if one knew that, it was possible to work out his
  account number. What was it?
  '''

  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/just_forgotten.mzn
  * SICStus Prolog: http://www.hakank.org/sicstis/just_forgotten.pl
  * ECLiPSe: http://hakank.org/eclipse/just_forgotten.ecl
  * Gecpde: http://hakank.org/gecode/just_forgotten.cpp

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver("Just forgotten")

  #
  # data
  #
  rows = 4
  cols = 10
  # The four tries
  a = [[9, 4, 6, 2, 1, 5, 7, 8, 3, 0], [8, 6, 0, 4, 3, 9, 1, 2, 5, 7],
       [1, 6, 4, 0, 2, 9, 7, 8, 5, 3], [6, 8, 2, 4, 3, 1, 9, 0, 7, 5]]

  #
  # variables
  #
  x = [solver.IntVar(0, 9, "x[%i]" % j) for j in range(cols)]

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(x))

  for r in range(rows):
    b = [solver.IsEqualCstVar(x[c], a[r][c]) for c in range(cols)]
    solver.Add(solver.Sum(b) == 4)

  #
  # search and result
  #
  db = solver.Phase(x, solver.INT_VAR_SIMPLE, solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    xval = [x[j].Value() for j in range(cols)]
    print("Account number:")
    for j in range(cols):
      print("%i " % xval[j], end=" ")
    print()
    print("\nThe four tries, where '!' represents a correct digit:")
    for i in range(rows):
      for j in range(cols):
        check = " "
        if a[i][j] == xval[j]:
          check = "!"
        print("%i%s" % (a[i][j], check), end=" ")
      print()
    print()
  print()

  solver.EndSearch()

  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main()
