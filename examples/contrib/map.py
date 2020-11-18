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
from ortools.constraint_solver import pywrapcp


def main():
  # Create the solver.
  solver = pywrapcp.Solver("Map coloring")

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
  color = [solver.IntVar(1, max_num_colors, "x%i" % i) for i in range(n)]

  #
  # constraints
  #
  solver.Add(color[Belgium] == 1)  # Symmetry breaking
  solver.Add(color[France] != color[Belgium])
  solver.Add(color[France] != color[Luxembourg])
  solver.Add(color[France] != color[Germany])
  solver.Add(color[Luxembourg] != color[Germany])
  solver.Add(color[Luxembourg] != color[Belgium])
  solver.Add(color[Belgium] != color[Netherlands])
  solver.Add(color[Belgium] != color[Germany])
  solver.Add(color[Germany] != color[Netherlands])
  solver.Add(color[Germany] != color[Denmark])

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add([color[i] for i in range(n)])

  collector = solver.AllSolutionCollector(solution)
  # collector = solver.FirstSolutionCollector(solution)
  # search_log = solver.SearchLog(100, x[0])
  solver.Solve(
      solver.Phase([color[i] for i in range(n)], solver.INT_VAR_SIMPLE,
                   solver.ASSIGN_MIN_VALUE), [collector])

  num_solutions = collector.SolutionCount()
  print("num_solutions: ", num_solutions)
  if num_solutions > 0:
    for s in range(num_solutions):
      colorval = [collector.Value(s, color[i]) for i in range(n)]
      print("color:", colorval)

    print()
    print("num_solutions:", num_solutions)
    print("failures:", solver.Failures())
    print("branches:", solver.Branches())
    print("WallTime:", solver.WallTime())

  else:
    print("No solutions found")


if __name__ == "__main__":
  main()
