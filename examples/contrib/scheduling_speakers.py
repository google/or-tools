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

  Scheduling speakers problem in Google CP Solver.

  From Rina Dechter, Constraint Processing, page 72
  Scheduling of 6 speakers in 6 slots.

  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/scheduling_speakers.mzn
  * SICStus Prolog: http://www.hakank.org/sicstus/scheduling_speakers.pl
  * ECLiPSe: http://hakank.org/eclipse/scheduling_speakers.ecl
  * Gecode: http://hakank.org/gecode/scheduling_speakers.cpp

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Scheduling speakers')

  #
  # data
  #
  n = 6  # number of speakers

  # slots available to speak
  available = [
      # Reasoning:
      [3, 4, 5, 6],  # 2) the only one with 6 after speaker F -> 1
      [3, 4],  # 5) 3 or 4
      [2, 3, 4, 5],  # 3) only with 5 after F -> 1 and A -> 6
      [2, 3, 4],  # 4) only with 2 after C -> 5 and F -> 1
      [3, 4],  # 5) 3 or 4
      [1, 2, 3, 4, 5, 6]  # 1) the only with 1
  ]

  #
  # variables
  #
  x = [solver.IntVar(1, n, 'x[%i]' % i) for i in range(n)]

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(x))

  for i in range(n):
    solver.Add(solver.MemberCt(x[i], available[i]))

  #
  # search and result
  #
  db = solver.Phase(x, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db)

  num_solutions = 0

  while solver.NextSolution():
    num_solutions += 1
    print('x:', [x[i].Value() for i in range(n)])

  solver.EndSearch()

  print()
  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
