# Copyright 2010 Hakan Kjellerstrand hakank@bonetmail.com
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

  Least diff problem in Google CP Solver.

  This model solves the following problem:

  What is the smallest difference between two numbers X - Y
  if you must use all the digits (0..9) exactly once.

  Compare with the following models:
  * Choco   : http://www.hakank.org/choco/LeastDiff2.java
  * ECLiPSE : http://www.hakank.org/eclipse/least_diff2.ecl
  * Comet   : http://www.hakank.org/comet/least_diff.co
  * Tailor/Essence': http://www.hakank.org/tailor/leastDiff.eprime
  * Gecode  : http://www.hakank.org/gecode/least_diff.cpp
  * Gecode/R: http://www.hakank.org/gecode_r/least_diff.rb
  * JaCoP   : http://www.hakank.org/JaCoP/LeastDiff.java
  * MiniZinc: http://www.hakank.org/minizinc/least_diff.mzn
  * SICStus : http://www.hakank.org/sicstus/least_diff.pl
  * Zinc    : http://hakank.org/minizinc/least_diff.zinc

  This model was created by Hakan Kjellerstrand (hakank@bonetmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_cp_solver/
"""
from __future__ import print_function
from ortools.constraint_solver import pywrapcp


def main(unused_argv):
  # Create the solver.
  solver = pywrapcp.Solver("Least diff")

  #
  # declare variables
  #
  digits = list(range(0, 10))
  a = solver.IntVar(digits, "a")
  b = solver.IntVar(digits, "b")
  c = solver.IntVar(digits, "c")
  d = solver.IntVar(digits, "d")
  e = solver.IntVar(digits, "e")

  f = solver.IntVar(digits, "f")
  g = solver.IntVar(digits, "g")
  h = solver.IntVar(digits, "h")
  i = solver.IntVar(digits, "i")
  j = solver.IntVar(digits, "j")

  letters = [a, b, c, d, e, f, g, h, i, j]

  digit_vector = [10000,1000,100,10,1]
  x = solver.ScalProd(letters[0:5],digit_vector)
  y = solver.ScalProd(letters[5:],digit_vector)
  diff = x - y
  
  #
  # constraints
  #
  solver.Add(diff > 0)
  solver.Add(solver.AllDifferent(letters))

  # objective
  objective = solver.Minimize(diff, 1)

  #
  # solution
  #
  solution = solver.Assignment()
  solution.Add(letters)
  solution.Add(x)
  solution.Add(y)
  solution.Add(diff)

  # last solution since it's a minimization problem
  collector = solver.LastSolutionCollector(solution)
  search_log = solver.SearchLog(100, diff)
  # Note: I'm not sure what CHOOSE_PATH do, but it is fast:
  #       find the solution in just 4 steps
  solver.Solve(solver.Phase(letters,
                            solver.CHOOSE_PATH,
                            solver.ASSIGN_MIN_VALUE),
               [objective, search_log, collector])

  # get the first (and only) solution

  xval = collector.Value(0, x)
  yval = collector.Value(0, y)
  diffval = collector.Value(0, diff)
  print("x:", xval)
  print("y:", yval)
  print("diff:", diffval)
  print(xval, "-", yval, "=", diffval)
  print([("abcdefghij"[i], collector.Value(0, letters[i])) for i in range(10)])
  print()
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())
  print()

if __name__ == "__main__":
  main("cp sample")
