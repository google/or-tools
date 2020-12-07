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

  Decomposition of the circuit constraint in Google CP Solver.


  Cf Global constraint catalog:
  http://www.emn.fr/x-info/sdemasse/gccat/Ccircuit.html

  Solution of n=4:
  x: [2, 0, 3, 1]
  x: [3, 0, 1, 2]
  x: [1, 3, 0, 2]
  x: [3, 2, 0, 1]
  x: [1, 2, 3, 0]
  x: [2, 3, 1, 0]

  The 'orbit' method that is used here is based on some
  observations on permutation orbits.

  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/circuit_test.mzn
  * Gecode: http://www.hakank.org/gecode/circuit_orbit.mzn


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""


import sys
from ortools.constraint_solver import pywrapcp

#
# circuit(x)
# constraints x to be an circuit
#
# Note: This assumes that x is has the domain 0..len(x)-1,
#       i.e. 0-based.
#


def circuit(solver, x):
  n = len(x)
  z = [solver.IntVar(0, n - 1, "z%i" % i) for i in range(n)]

  solver.Add(solver.AllDifferent(x))
  solver.Add(solver.AllDifferent(z))

  # put the orbit of x[0] in in z[0..n-1]
  solver.Add(z[0] == x[0])
  for i in range(1, n - 1):
    # The following constraint give the error
    # "TypeError: list indices must be integers, not IntVar"
    # solver.Add(z[i] == x[z[i-1]])

    # solution: use Element instead
    solver.Add(z[i] == solver.Element(x, z[i - 1]))

  #
  # Note: At least one of the following two constraint must be set.
  #
  # may not be 0 for i < n-1
  for i in range(1, n - 1):
    solver.Add(z[i] != 0)

  # when i = n-1 it must be 0
  solver.Add(z[n - 1] == 0)


def main(n=5):

  # Create the solver.
  solver = pywrapcp.Solver("Send most money")

  # data
  print("n:", n)

  # declare variables
  # Note: domain should be 0..n-1
  x = [solver.IntVar(0, n - 1, "x%i" % i) for i in range(n)]

  #
  # constraints
  #
  circuit(solver, x)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x)

  collector = solver.AllSolutionCollector(solution)

  solver.Solve(
      solver.Phase(x, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE),
      [collector])

  num_solutions = collector.SolutionCount()
  for s in range(num_solutions):
    print("x:", [collector.Value(s, x[i]) for i in range(len(x))])

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())
  print()


n = 5
if __name__ == "__main__":
  if len(sys.argv) > 1:
    n = int(sys.argv[1])

  main(n)
