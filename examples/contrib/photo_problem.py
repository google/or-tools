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

  Photo problem in Google CP Solver.

  Problem statement from Mozart/Oz tutorial:
  http://www.mozart-oz.org/home/doc/fdt/node37.html#section.reified.photo
  '''
  Betty, Chris, Donald, Fred, Gary, Mary, and Paul want to align in one
  row for taking a photo. Some of them have preferences next to whom
  they want to stand:

     1. Betty wants to stand next to Gary and Mary.
     2. Chris wants to stand next to Betty and Gary.
     3. Fred wants to stand next to Mary and Donald.
     4. Paul wants to stand next to Fred and Donald.

  Obviously, it is impossible to satisfy all preferences. Can you find
  an alignment that maximizes the number of satisfied preferences?
  '''

  Oz solution:
    6 # alignment(betty:5  chris:6  donald:1  fred:3  gary:7   mary:4   paul:2)
  [5, 6, 1, 3, 7, 4, 2]


  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/photo_hkj.mzn
  * Comet: http://hakank.org/comet/photo_problem.co
  * SICStus: http://hakank.org/sicstus/photo_problem.pl

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys

from ortools.constraint_solver import pywrapcp


def main(show_all_max=0):

  # Create the solver.
  solver = pywrapcp.Solver("Photo problem")

  #
  # data
  #
  persons = ["Betty", "Chris", "Donald", "Fred", "Gary", "Mary", "Paul"]
  n = len(persons)
  preferences = [
      # 0 1 2 3 4 5 6
      # B C D F G M P
      [0, 0, 0, 0, 1, 1, 0],  # Betty  0
      [1, 0, 0, 0, 1, 0, 0],  # Chris  1
      [0, 0, 0, 0, 0, 0, 0],  # Donald 2
      [0, 0, 1, 0, 0, 1, 0],  # Fred   3
      [0, 0, 0, 0, 0, 0, 0],  # Gary   4
      [0, 0, 0, 0, 0, 0, 0],  # Mary   5
      [0, 0, 1, 1, 0, 0, 0]  # Paul   6
  ]

  print("""Preferences:
     1. Betty wants to stand next to Gary and Mary.
     2. Chris wants to stand next to Betty and Gary.
     3. Fred wants to stand next to Mary and Donald.
     4. Paul wants to stand next to Fred and Donald.
    """)

  #
  # declare variables
  #
  positions = [solver.IntVar(0, n - 1, "positions[%i]" % i) for i in range(n)]

  # successful preferences
  z = solver.IntVar(0, n * n, "z")

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(positions))

  # calculate all the successful preferences
  b = [
      solver.IsEqualCstVar(abs(positions[i] - positions[j]), 1)
      for i in range(n)
      for j in range(n)
      if preferences[i][j] == 1
  ]
  solver.Add(z == solver.Sum(b))

  #
  # Symmetry breaking (from the Oz page):
  #   Fred is somewhere left of Betty
  solver.Add(positions[3] < positions[0])

  # objective
  objective = solver.Maximize(z, 1)
  if show_all_max != 0:
    print("Showing all maximum solutions (z == 6).\n")
    solver.Add(z == 6)

  #
  # search and result
  #
  db = solver.Phase(positions, solver.CHOOSE_FIRST_UNBOUND,
                    solver.ASSIGN_MAX_VALUE)

  if show_all_max == 0:
    solver.NewSearch(db, [objective])
  else:
    solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    print("z:", z.Value())
    p = [positions[i].Value() for i in range(n)]

    print(" ".join(
        [persons[j] for i in range(n) for j in range(n) if p[j] == i]))
    print("Successful preferences:")
    for i in range(n):
      for j in range(n):
        if preferences[i][j] == 1 and abs(p[i] - p[j]) == 1:
          print("\t", persons[i], persons[j])
    print()
    num_solutions += 1

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


show_all_max = 0  # show all maximal solutions
if __name__ == "__main__":
  if len(sys.argv) > 1:
    show_all_max = 1
  main(show_all_max)
