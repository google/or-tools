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

  Map coloring problem in Google CP Solver.


  From Pascal Van Hentenryck 'The OPL Optimization Programming Language',
  page 7, 42.

  Compare with the following models:
  * Comet: http://www.hakank.org/comet/map.co
  * Tailor/Essence': http://hakank.org/tailor/map_coloring.eprime
  * SICStus: http://hakank.org/sicstus/map_coloring.pl
  * ECLiPSe: http://hakank.org/eclipse/map.ecl
  * Gecode: http://hakank.org/gecode/map.cpp
  * MiniZinc: http://hakank.org/minizinc/map.mzn
  * Zinc: http://hakank.org/minizinc/map.zinc

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.constraint_solver.python import constraint_solver as cp


def main():
  # Create the solver.
  solver = cp.Solver("Map coloring")

  #
  # data
  #
  Belgium = 0
  Denmark = 1
  France = 2
  Germany = 3
  Netherlands = 4
  Luxembourg = 5

  n = 6
  max_num_colors = 4

  # declare variables
  color = [solver.new_int_var(1, max_num_colors, "x%i" % i) for i in range(n)]

  #
  # constraints
  #
  solver.add(color[Belgium] == 1)  # Symmetry breaking
  solver.add(color[France] != color[Belgium])
  solver.add(color[France] != color[Luxembourg])
  solver.add(color[France] != color[Germany])
  solver.add(color[Luxembourg] != color[Germany])
  solver.add(color[Luxembourg] != color[Belgium])
  solver.add(color[Belgium] != color[Netherlands])
  solver.add(color[Belgium] != color[Germany])
  solver.add(color[Germany] != color[Netherlands])
  solver.add(color[Germany] != color[Denmark])

  #
  # solution and search
  #
  solution = solver.assignment()
  solution.add([color[i] for i in range(n)])

  collector = solver.all_solution_collector(solution)
  # collector = solver.FirstSolutionCollector(solution)
  # search_log = solver.search_log(100, x[0])
  solver.solve(
      solver.phase([color[i] for i in range(n)], cp.IntVarStrategy.INT_VAR_SIMPLE,
                   cp.IntValueStrategy.ASSIGN_MIN_VALUE), [collector])

  num_solutions = collector.solution_count
  print("num_solutions: ", num_solutions)
  if num_solutions > 0:
    for s in range(num_solutions):
      colorval = [collector.value(s, color[i]) for i in range(n)]
      print("color:", colorval)

    print()
    print("num_solutions:", num_solutions)
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)

  else:
    print("No solutions found")


if __name__ == "__main__":
  main()
