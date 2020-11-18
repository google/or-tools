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

  Set covering deployment in Google CP Solver

  From http://mathworld.wolfram.com/SetCoveringDeployment.html
  '''
  Set covering deployment (sometimes written 'set-covering deployment'
  and abbreviated SCDP for 'set covering deployment problem') seeks
  an optimal stationing of troops in a set of regions so that a
  relatively small number of troop units can control a large
  geographic region. ReVelle and Rosing (2000) first described
  this in a study of Emperor Constantine the Great's mobile field
  army placements to secure the Roman Empire.
  '''

  Compare with the the following models:
  * MiniZinc: http://www.hakank.org/minizinc/set_covering_deployment.mzn
  * Comet   : http://www.hakank.org/comet/set_covering_deployment.co
  * Gecode  : http://www.hakank.org/gecode/set_covering_deployment.cpp
  * ECLiPSe : http://www.hakank.org/eclipse/set_covering_deployment.ecl
  * SICStus : http://hakank.org/sicstus/set_covering_deployment.pl

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver("Set covering deployment")

  #
  # data
  #

  countries = [
      "Alexandria", "Asia Minor", "Britain", "Byzantium", "Gaul", "Iberia",
      "Rome", "Tunis"
  ]
  n = len(countries)

  # the incidence matrix (neighbours)
  mat = [[0, 1, 0, 1, 0, 0, 1, 1], [1, 0, 0, 1, 0, 0, 0, 0],
         [0, 0, 0, 0, 1, 1, 0, 0], [1, 1, 0, 0, 0, 0, 1, 0],
         [0, 0, 1, 0, 0, 1, 1, 0], [0, 0, 1, 0, 1, 0, 1, 1],
         [1, 0, 0, 1, 1, 1, 0, 1], [1, 0, 0, 0, 0, 1, 1, 0]]

  #
  # declare variables
  #

  # First army
  X = [solver.IntVar(0, 1, "X[%i]" % i) for i in range(n)]

  # Second (reserv) army
  Y = [solver.IntVar(0, 1, "Y[%i]" % i) for i in range(n)]

  #
  # constraints
  #

  # total number of armies
  num_armies = solver.Sum([X[i] + Y[i] for i in range(n)])

  #
  #  Constraint 1: There is always an army in a city
  #                (+ maybe a backup)
  #                Or rather: Is there a backup, there
  #                must be an an army
  #
  [solver.Add(X[i] >= Y[i]) for i in range(n)]

  #
  # Constraint 2: There should always be an backup army near every city
  #
  for i in range(n):
    neighbors = solver.Sum([Y[j] for j in range(n) if mat[i][j] == 1])
    solver.Add(X[i] + neighbors >= 1)

  objective = solver.Minimize(num_armies, 1)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(X)
  solution.Add(Y)
  solution.Add(num_armies)
  solution.AddObjective(num_armies)

  collector = solver.LastSolutionCollector(solution)
  solver.Solve(
      solver.Phase(X + Y, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT),
      [collector, objective])

  print("num_armies:", collector.ObjectiveValue(0))
  print("X:", [collector.Value(0, X[i]) for i in range(n)])
  print("Y:", [collector.Value(0, Y[i]) for i in range(n)])

  for i in range(n):
    if collector.Value(0, X[i]) == 1:
      print("army:", countries[i], end=" ")
    if collector.Value(0, Y[i]) == 1:
      print("reserv army:", countries[i], " ")
  print()

  print()
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main()
