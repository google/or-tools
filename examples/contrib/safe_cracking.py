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

  Safe cracking puzzle in Google CP Solver.

  From the Oz Primer:
  http://www.comp.nus.edu.sg/~henz/projects/puzzles/digits/index.html
  '''
  The code of Professor Smart's safe is a sequence of 9 distinct
  nonzero digits C1 .. C9 such that the following equations and
  inequations are satisfied:

        C4 - C6   =   C7
   C1 * C2 * C3   =   C8 + C9
   C2 + C3 + C6   <   C8
             C9   <   C8

   and

   C1 <> 1, C2 <> 2, ..., C9 <> 9

  can you find the correct combination?
  '''

  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/safe_cracking.mzn
  * ECLiPSe : http://www.hakank.org/eclipse/safe_cracking.ecl
  * SICStus : http://www.hakank.org/sicstus/safe_cracking.pl
  * Gecode: http://hakank.org/gecode/safe_cracking.cpp

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Safe cracking puzzle')

  #
  # data
  #
  n = 9
  digits = list(range(1, n + 1))

  #
  # variables
  #

  LD = [solver.IntVar(digits, 'LD[%i]' % i) for i in range(n)]
  C1, C2, C3, C4, C5, C6, C7, C8, C9 = LD

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(LD))

  solver.Add(C4 - C6 == C7)
  solver.Add(C1 * C2 * C3 == C8 + C9)
  solver.Add(C2 + C3 + C6 < C8)
  solver.Add(C9 < C8)
  for i in range(n):
    solver.Add(LD[i] != i + 1)

  #
  # search and result
  #
  db = solver.Phase(LD, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db)

  num_solutions = 0

  while solver.NextSolution():
    num_solutions += 1
    print('LD:', [LD[i].Value() for i in range(n)])

  solver.EndSearch()

  print()
  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
