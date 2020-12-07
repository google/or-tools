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

  Who killed agatha? (The Dreadsbury Mansion Murder Mystery) in Google CP
  Solver.

  This is a standard benchmark for theorem proving.

  http://www.lsv.ens-cachan.fr/~goubault/H1.dist/H1.1/Doc/h1003.html
  '''
  Someone in Dreadsbury Mansion killed Aunt Agatha.
  Agatha, the butler, and Charles live in Dreadsbury Mansion, and
  are the only ones to live there. A killer always hates, and is no
  richer than his victim. Charles hates noone that Agatha hates. Agatha
  hates everybody except the butler. The butler hates everyone not richer
  than Aunt Agatha. The butler hates everyone whom Agatha hates.
  Noone hates everyone. Who killed Agatha?
  '''

  Originally from F. J. Pelletier:
  Seventy-five problems for testing automatic theorem provers.
  Journal of Automated Reasoning, 2: 216, 1986.

  Note1: Since Google CP Solver/Pythons (currently) don't have
         special support for logical operations on decision
         variables (i.e. ->, <->, and, or, etc), this model
         use some IP modeling tricks.

  Note2: There are 8 different solutions, all stating that Agatha
         killed herself

  Compare with the following models:
  * Choco   : http://www.hakank.org/choco/WhoKilledAgatha.java
  * Choco   : http://www.hakank.org/choco/WhoKilledAgatha_element.java
  * Comet   : http://www.hakank.org/comet/who_killed_agatha.co
  * ECLiPSE : http://www.hakank.org/eclipse/who_killed_agatha.ecl
  * Gecode  : http://www.hakank.org/gecode/who_killed_agatha.cpp
  * JaCoP   : http://www.hakank.org/JaCoP/WhoKilledAgatha.java
  * JaCoP   : http://www.hakank.org/JaCoP/WhoKilledAgatha_element.java
  * MiniZinc: http://www.hakank.org/minizinc/who_killed_agatha.mzn
  * Tailor/Essence': http://www.hakank.org/tailor/who_killed_agatha.eprime
  * SICStus : http://hakank.org/sicstus/who_killed_agatha.pl
  * Zinc    :http://hakank.org/minizinc/who_killed_agatha.zinc


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from collections import defaultdict

from ortools.constraint_solver import pywrapcp


def var_matrix_array(solver, rows, cols, lb, ub, name):
  x = []
  for i in range(rows):
    t = []
    for j in range(cols):
      t.append(solver.IntVar(lb, ub, "%s[%i,%i]" % (name, i, j)))
    x.append(t)
  return x


def flatten_matrix(solver, m, rows, cols):
  return [m[i][j] for i in range(rows) for j in range(cols)]


def print_flat_matrix(m_flat, rows, cols):
  for i in range(rows):
    for j in range(cols):
      print(m_flat[i * cols + j].Value(), end=" ")
    print()
  print()


def main(the_killers):

  # Create the solver.
  solver = pywrapcp.Solver("Who killed agatha?")

  #
  # data
  #
  n = 3
  agatha = 0
  butler = 1
  charles = 2

  #
  # declare variables
  #
  the_killer = solver.IntVar(0, 2, "the_killer")
  the_victim = solver.IntVar(0, 2, "the_victim")

  hates = var_matrix_array(solver, n, n, 0, 1, "hates")
  richer = var_matrix_array(solver, n, n, 0, 1, "richer")

  hates_flat = flatten_matrix(solver, hates, n, n)
  richer_flat = flatten_matrix(solver, richer, n, n)

  #
  # constraints
  #

  # Agatha, the butler, and Charles live in Dreadsbury Mansion, and
  # are the only ones to live there.

  # A killer always hates, and is no richer than his victim.
  # solver.Add(hates[the_killer, the_victim] == 1)
  solver.Add(solver.Element(hates_flat, the_killer * n + the_victim) == 1)

  # solver.Add(richer[the_killer, the_victim] == 0)
  solver.Add(solver.Element(richer_flat, the_killer * n + the_victim) == 0)

  # define the concept of richer: no one is richer than him-/herself
  for i in range(n):
    solver.Add(richer[i][i] == 0)

  # (contd...) if i is richer than j then j is not richer than i
  #  (i != j) => (richer[i,j] = 1) <=> (richer[j,i] = 0),
  for i in range(n):
    for j in range(n):
      if i != j:
        solver.Add((richer[i][j] == 1) == (richer[j][i] == 0))

  # Charles hates noone that Agatha hates.
  # forall i : Range .
  #  (hates[agatha, i] = 1) => (hates[charles, i] = 0),
  for i in range(n):
    solver.Add((hates[agatha][i] == 1) <= (hates[charles][i] == 0))

  # Agatha hates everybody except the butler.
  solver.Add(hates[agatha][charles] == 1)
  solver.Add(hates[agatha][agatha] == 1)
  solver.Add(hates[agatha][butler] == 0)

  # The butler hates everyone not richer than Aunt Agatha.
  # forall i : Range .
  #  (richer[i, agatha] = 0) => (hates[butler, i] = 1),
  for i in range(n):
    solver.Add((richer[i][agatha] == 0) <= (hates[butler][i] == 1))

  # The butler hates everyone whom Agatha hates.
  # forall i : Range .
  #  (hates[agatha, i] = 1) => (hates[butler, i] = 1),
  for i in range(n):
    solver.Add((hates[agatha][i] == 1) <= (hates[butler][i] == 1))

  # Noone hates everyone.
  # forall i : Range .
  #   (sum j : Range . hates[i,j]) <= 2,
  for i in range(n):
    solver.Add(solver.Sum([hates[i][j] for j in range(n)]) <= 2)

  # Who killed Agatha?
  solver.Add(the_victim == agatha)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(the_killer)
  solution.Add(the_victim)
  solution.Add(hates_flat)
  solution.Add(richer_flat)

  # db: DecisionBuilder
  db = solver.Phase(hates_flat + richer_flat, solver.CHOOSE_FIRST_UNBOUND,
                    solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    print("the_killer:", the_killer.Value())
    the_killers[the_killer.Value()] += 1
    print("the_victim:", the_victim.Value())
    print("hates:")
    print_flat_matrix(hates_flat, n, n)
    print("richer:")
    print_flat_matrix(richer_flat, n, n)
    print()
    num_solutions += 1

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


the_killers = defaultdict(int)
p = ["agatha", "butler", "charles"]
if __name__ == "__main__":
  main(the_killers)

  print("\n")
  for k in the_killers:
    print("the killer %s was choosen in %i solutions" % (p[k], the_killers[k]))
