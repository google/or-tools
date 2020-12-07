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

  Set covering in Google CP Solver.

  Example from Steven Skiena, The Stony Brook Algorithm Repository
  http://www.cs.sunysb.edu/~algorith/files/set-cover.shtml
  '''
  Input Description: A set of subsets S_1, ..., S_m of the
  universal set U = {1,...,n}.

  Problem: What is the smallest subset of subsets T subset S such
  that \cup_{t_i in T} t_i = U?
  '''
  Data is from the pictures INPUT/OUTPUT.

  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/set_covering_skiena.mzn
  * Comet: http://www.hakank.org/comet/set_covering_skiena.co
  * ECLiPSe: http://www.hakank.org/eclipse/set_covering_skiena.ecl
  * SICStus Prolog: http://www.hakank.org/sicstus/set_covering_skiena.pl
  * Gecode: http://hakank.org/gecode/set_covering_skiena.cpp


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Set covering Skiena')

  #
  # data
  #
  num_sets = 7
  num_elements = 12
  belongs = [
      # 1 2 3 4 5 6 7 8 9 0 1 2  elements
      [1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],  # Set 1
      [0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0],  # 2
      [0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0],  # 3
      [0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0],  # 4
      [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0],  # 5
      [1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0],  # 6
      [0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1]  # 7
  ]

  #
  # variables
  #
  x = [solver.IntVar(0, 1, 'x[%i]' % i) for i in range(num_sets)]

  # number of choosen sets
  z = solver.IntVar(0, num_sets * 2, 'z')

  # total number of elements in the choosen sets
  tot_elements = solver.IntVar(0, num_sets * num_elements)

  #
  # constraints
  #
  solver.Add(z == solver.Sum(x))

  # all sets must be used
  for j in range(num_elements):
    s = solver.Sum([belongs[i][j] * x[i] for i in range(num_sets)])
    solver.Add(s >= 1)

  # number of used elements
  solver.Add(tot_elements == solver.Sum([
      x[i] * belongs[i][j] for i in range(num_sets) for j in range(num_elements)
  ]))

  # objective
  objective = solver.Minimize(z, 1)

  #
  # search and result
  #
  db = solver.Phase(x, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db, [objective])

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print('z:', z.Value())
    print('tot_elements:', tot_elements.Value())
    print('x:', [x[i].Value() for i in range(num_sets)])

  solver.EndSearch()

  print()
  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
