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

  toNum in Google CP Solver.

  Convert a number <-> array of int in a specific base.

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""
from ortools.constraint_solver import pywrapcp

#
# converts a number (s) <-> an array of integers (t) in the specific base.
#


def toNum(solver, t, s, base):
  tlen = len(t)
  solver.Add(
      s == solver.Sum([(base**(tlen - i - 1)) * t[i] for i in range(tlen)]))


def main(unused_argv):
  # Create the solver.
  solver = pywrapcp.Solver("toNum test")

  # data
  n = 4
  base = 10

  # declare variables
  x = [solver.IntVar(0, n - 1, "x%i" % i) for i in range(n)]
  y = solver.IntVar(0, 10**n - 1, "y")

  #
  # constraints
  #
  # solver.Add(solver.AllDifferent([x[i] for i in range(n)]))
  solver.Add(solver.AllDifferent(x))
  # solver.Add(x[0] > 0) # just for fun

  toNum(solver, x, y, base)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add([x[i] for i in range(n)])
  solution.Add(y)

  collector = solver.AllSolutionCollector(solution)
  solver.Solve(
      solver.Phase([x[i] for i in range(n)], solver.CHOOSE_FIRST_UNBOUND,
                   solver.ASSIGN_MIN_VALUE), [collector])

  num_solutions = collector.SolutionCount()
  for s in range(num_solutions):
    print("x:", [collector.Value(s, x[i]) for i in range(n)])
    print("y:", collector.Value(s, y))
    print()

  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main("cp sample")
