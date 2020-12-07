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

  Example 9.1-2, page 354ff, from
  Taha 'Operations Research - An Introduction'
  Minimize the number of security telephones in street
  corners on a campus.

  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/set_covering2.mzn
  * Comet   : http://www.hakank.org/comet/set_covering2.co
  * ECLiPSe : http://www.hakank.org/eclipse/set_covering2.ecl
  * SICStus: http://hakank.org/sicstus/set_covering2.pl
  * Gecode: http://hakank.org/gecode/set_covering2.cpp

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""
from ortools.constraint_solver import pywrapcp


def main(unused_argv):

  # Create the solver.
  solver = pywrapcp.Solver("Set covering")

  #
  # data
  #
  n = 8  # maximum number of corners
  num_streets = 11  # number of connected streets

  # corners of each street
  # Note: 1-based (handled below)
  corner = [[1, 2], [2, 3], [4, 5], [7, 8], [6, 7], [2, 6], [1, 6], [4, 7],
            [2, 4], [5, 8], [3, 5]]

  #
  # declare variables
  #
  x = [solver.IntVar(0, 1, "x[%i]" % i) for i in range(n)]

  #
  # constraints
  #

  # number of telephones, to be minimized
  z = solver.Sum(x)

  # ensure that all corners are covered
  for i in range(num_streets):
    # also, convert to 0-based
    solver.Add(solver.SumGreaterOrEqual([x[j - 1] for j in corner[i]], 1))

  objective = solver.Minimize(z, 1)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x)
  solution.AddObjective(z)

  collector = solver.LastSolutionCollector(solution)
  solver.Solve(
      solver.Phase(x, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT),
      [collector, objective])

  print("z:", collector.ObjectiveValue(0))
  print("x:", [collector.Value(0, x[i]) for i in range(n)])

  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main("cp sample")
