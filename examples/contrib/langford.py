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

  Langford's number problem in Google CP Solver.

  Langford's number problem (CSP lib problem 24)
  http://www.csplib.org/prob/prob024/
  '''
  Arrange 2 sets of positive integers 1..k to a sequence,
  such that, following the first occurence of an integer i,
  each subsequent occurrence of i, appears i+1 indices later
  than the last.
  For example, for k=4, a solution would be 41312432
  '''

  * John E. Miller: Langford's Problem
    http://www.lclark.edu/~miller/langford.html

  * Encyclopedia of Integer Sequences for the number of solutions for each k
    http://www.research.att.com/cgi-bin/access.cgi/as/njas/sequences/eisA.cgi?Anum=014552


  Also, see the following models:
  * MiniZinc: http://www.hakank.org/minizinc/langford2.mzn
  * Gecode/R: http://www.hakank.org/gecode_r/langford.rb
  * ECLiPSe: http://hakank.org/eclipse/langford.ecl
  * SICStus: http://hakank.org/sicstus/langford.pl


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys

from ortools.constraint_solver import pywrapcp


def main(k=8, num_sol=0):

  # Create the solver.
  solver = pywrapcp.Solver("Langford")

  #
  # data
  #
  print("k:", k)
  p = list(range(2 * k))

  #
  # declare variables
  #
  position = [solver.IntVar(0, 2 * k - 1, "position[%i]" % i) for i in p]
  solution = [solver.IntVar(1, k, "position[%i]" % i) for i in p]

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(position))

  for i in range(1, k + 1):
    solver.Add(position[i + k - 1] == position[i - 1] + i + 1)
    solver.Add(solver.Element(solution, position[i - 1]) == i)
    solver.Add(solver.Element(solution, position[k + i - 1]) == i)

  # symmetry breaking
  solver.Add(solution[0] < solution[2 * k - 1])

  #
  # search and result
  #
  db = solver.Phase(position, solver.CHOOSE_FIRST_UNBOUND,
                    solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    print("solution:", ",".join([str(solution[i].Value()) for i in p]))
    num_solutions += 1
    if num_sol > 0 and num_solutions >= num_sol:
      break

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


k = 8
num_sol = 0
if __name__ == "__main__":
  if len(sys.argv) > 1:
    k = int(sys.argv[1])
  if len(sys.argv) > 2:
    num_sol = int(sys.argv[2])

  main(k, num_sol)
