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

  Olympic puzzle in Google CP Solver.

  Benchmark for Prolog (BProlog)
  '''
  File   : olympic.pl
  Author : Neng-Fa ZHOU
  Date   : 1993

  Purpose: solve a puzzle taken from Olympic Arithmetic Contest

  Given ten variables with the following configuration:

                 X7   X8   X9   X10

                    X4   X5   X6

                       X2   X3

                          X1

  We already know that X1 is equal to 3 and want to assign each variable
  with a different integer from {1,2,...,10} such that for any three
  variables
                        Xi   Xj

                           Xk
  the following constraint is satisfied:

                      |Xi-Xj| = Xk
  '''

  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/olympic.mzn
  * SICStus Prolog: http://www.hakank.org/sicstus/olympic.pl
  * ECLiPSe: http://hakank.org/eclipse/olympic.ecl
  * Gecode: http://hakank.org/gecode/olympic.cpp


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def minus(solver, x, y, z):
  solver.Add(z == abs(x - y))


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Olympic')

  #
  # data
  #
  n = 10

  #
  # declare variables
  #
  Vars = [solver.IntVar(1, n, 'Vars[%i]' % i) for i in range(n)]
  X1, X2, X3, X4, X5, X6, X7, X8, X9, X10 = Vars

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(Vars))

  solver.Add(X1 == 3)
  minus(solver, X2, X3, X1)
  minus(solver, X4, X5, X2)
  minus(solver, X5, X6, X3)
  minus(solver, X7, X8, X4)
  minus(solver, X8, X9, X5)
  minus(solver, X9, X10, X6)

  #
  # solution and search
  #
  db = solver.Phase(Vars, solver.INT_VAR_SIMPLE, solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print('Vars:', [Vars[i].Value() for i in range(n)])

  print()
  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
