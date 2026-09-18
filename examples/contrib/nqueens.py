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

  n-queens problem in Google CP Solver.

  N queens problem.

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.constraint_solver.python import constraint_solver as cp


def main(n=8):
  # Create the solver.
  solver = cp.Solver("n-queens")

  #
  # data
  #
  # n = 8 # size of board (n x n)

  # declare variables
  q = [solver.new_int_var(0, n - 1, "x%i" % i) for i in range(n)]

  #
  # constraints
  #
  solver.add_all_different(q)
  for i in range(n):
    for j in range(i):
      solver.add(q[i] != q[j])
      solver.add(q[i] + i != q[j] + j)
      solver.add(q[i] - i != q[j] - j)

  # for i in range(n):
  #     for j in range(i):
  #         solver.add(abs(q[i]-q[j]) != abs(i-j))

  # symmetry breaking
  # solver.add(q[0] == 0)

  #
  # solution and search
  #
  solution = solver.assignment()
  solution.add([q[i] for i in range(n)])

  collector = solver.all_solution_collector(solution)
  # collector = solver.FirstSolutionCollector(solution)
  # search_log = solver.search_log(100, x[0])
  solver.solve(
      solver.phase([q[i] for i in range(n)], cp.IntVarStrategy.INT_VAR_SIMPLE,
                   cp.IntValueStrategy.ASSIGN_MIN_VALUE), [collector])

  num_solutions = collector.solution_count
  print("num_solutions: ", num_solutions)
  if num_solutions > 0:
    for s in range(num_solutions):
      qval = [collector.value(s, q[i]) for i in range(n)]
      print("q:", qval)
      for i in range(n):
        for j in range(n):
          if qval[i] == j:
            print("Q", end=" ")
          else:
            print("_", end=" ")
        print()
      print()

    print()
    print("num_solutions:", num_solutions)
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)

  else:
    print("No solutions found")


n = 8
if __name__ == "__main__":
  main(n)
