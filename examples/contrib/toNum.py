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
from ortools.constraint_solver.python import constraint_solver as cp

#
# converts a number (s) <-> an array of integers (t) in the specific base.
#


def toNum(solver, t, s, base):
  tlen = len(t)
  solver.add(
      s == solver.sum([(base**(tlen - i - 1)) * t[i] for i in range(tlen)]))


def main(unused_argv):
  # Create the solver.
  solver = cp.Solver("toNum test")

  # data
  n = 4
  base = 10

  # declare variables
  x = [solver.new_int_var(0, n - 1, "x%i" % i) for i in range(n)]
  y = solver.new_int_var(0, 10**n - 1, "y")

  #
  # constraints
  #
  # solver.add(solver.add_all_different([x[i] for i in range(n)]))
  solver.add_all_different(x)
  # solver.add(x[0] > 0) # just for fun

  toNum(solver, x, y, base)

  #
  # solution and search
  #
  solution = solver.assignment()
  solution.add([x[i] for i in range(n)])
  solution.add(y)

  collector = solver.all_solution_collector(solution)
  solver.solve(
      solver.phase([x[i] for i in range(n)], cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
                   cp.IntValueStrategy.ASSIGN_MIN_VALUE), [collector])

  num_solutions = collector.solution_count
  for s in range(num_solutions):
    print("x:", [collector.value(s, x[i]) for i in range(n)])
    print("y:", collector.value(s, y))
    print()

  print("failures:", solver.num_failures)
  print("branches:", solver.num_branches)
  print("WallTime:", solver.wall_time_ms)


if __name__ == "__main__":
  main("cp sample")
