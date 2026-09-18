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

  Stable marriage problem in Google CP Solver.

  Problem and OPL model from Pascal Van Hentenryck
  'The OPL Optimization Programming Language', page 43ff.

  Also, see
  http://www.comp.rgu.ac.uk/staff/ha/ZCSP/additional_problems/stable_marriage/stable_marriage.pdf

  Note: This model is translated from my Comet model
       http://www.hakank.org/comet/stable_marriage.co
  I have kept some of the constraint from that code.

  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/stable_marriage.mzn
  * Comet   : http://www.hakank.org/comet/stable_marriage.co
  * ECLiPSe : http://www.hakank.org/eclipse/stable_marriage.ecl
  * Gecode  : http://hakank.org/gecode/stable_marriage.cpp
  * SICStus : http://hakank.org/sicstus/stable_marriage.pl

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""
import sys

from ortools.constraint_solver.python import constraint_solver as cp


def main(ranks, problem_name):

  # Create the solver
  solver = cp.Solver("Stable marriage")

  #
  # data
  #
  print("Problem name:", problem_name)

  rankMen = ranks["rankMen"]
  rankWomen = ranks["rankWomen"]

  n = len(rankMen)

  #
  # declare variables
  #
  wife = [solver.new_int_var(0, n - 1, "wife[%i]" % i) for i in range(n)]
  husband = [solver.new_int_var(0, n - 1, "husband[%i]" % i) for i in range(n)]

  #
  # constraints
  #

  # forall(m in Men)
  #    cp.post(husband[wife[m]] == m);
  for m in range(n):
    solver.add(solver.element(husband, wife[m]) == m)

  # forall(w in Women)
  #    cp.post(wife[husband[w]] == w);
  for w in range(n):
    solver.add(solver.element(wife, husband[w]) == w)

  # forall(m in Men, o in Women)
  # cp.post(rankMen[m,o] < rankMen[m, wife[m]] => rankWomen[o,husband[o]] <
  # rankWomen[o,m]);
  for m in range(n):
    for o in range(n):
      b1 = solver.add_is_greater_cst_var(
          solver.element(rankMen[m], wife[m]), rankMen[m][o])
      b2 = (
          solver.add_is_less_cst_var(
              solver.element(rankWomen[o], husband[o]), rankWomen[o][m]))
      solver.add(b1 - b2 <= 0)

  # forall(w in Women, o in Men)
  # cp.post(rankWomen[w,o] < rankWomen[w,husband[w]] => rankMen[o,wife[o]] <
  # rankMen[o,w]);
  for w in range(n):
    for o in range(n):
      b1 = solver.add_is_greater_cst_var(
          solver.element(rankWomen[w], husband[w]), rankWomen[w][o])
      b2 = solver.add_is_less_cst_var(
          solver.element(rankMen[o], wife[o]), rankMen[o][w])
      solver.add(b1 - b2 <= 0)

  #
  # solution and search
  #
  solution = solver.assignment()
  solution.add(wife)
  solution.add(husband)

  db = solver.phase(wife + husband, cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
                    cp.IntValueStrategy.ASSIGN_MIN_VALUE)

  solver.new_search(db)
  num_solutions = 0
  solutions = []
  while solver.next_solution():
    # solutions.append([x[i].value() for i in range(x_len)])
    print("wife   : ", [wife[i].value() for i in range(n)])
    print("husband: ", [husband[i].value() for i in range(n)])
    print()
    num_solutions += 1

  solver.end_search()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.num_failures)
  print("branches:", solver.num_branches)
  print("WallTime:", solver.wall_time_ms)
  print("#############")
  print()


#
# From Van Hentenryck's OPL book
#
van_hentenryck = {
    "rankWomen": [[1, 2, 4, 3, 5], [3, 5, 1, 2, 4], [5, 4, 2, 1, 3],
                  [1, 3, 5, 4, 2], [4, 2, 3, 5, 1]],
    "rankMen": [[5, 1, 2, 4, 3], [4, 1, 3, 2, 5], [5, 3, 2, 4, 1],
                [1, 5, 4, 3, 2], [4, 3, 2, 1, 5]]
}

#
# Data from MathWorld
# http://mathworld.wolfram.com/StableMarriageProblem.html
#
mathworld = {
    "rankWomen": [[3, 1, 5, 2, 8, 7, 6, 9, 4], [9, 4, 8, 1, 7, 6, 3, 2, 5],
                  [3, 1, 8, 9, 5, 4, 2, 6, 7], [8, 7, 5, 3, 2, 6, 4, 9, 1],
                  [6, 9, 2, 5, 1, 4, 7, 3, 8], [2, 4, 5, 1, 6, 8, 3, 9, 7],
                  [9, 3, 8, 2, 7, 5, 4, 6, 1], [6, 3, 2, 1, 8, 4, 5, 9, 7],
                  [8, 2, 6, 4, 9, 1, 3, 7, 5]],
    "rankMen": [[7, 3, 8, 9, 6, 4, 2, 1, 5], [5, 4, 8, 3, 1, 2, 6, 7, 9],
                [4, 8, 3, 9, 7, 5, 6, 1, 2], [9, 7, 4, 2, 5, 8, 3, 1, 6],
                [2, 6, 4, 9, 8, 7, 5, 1, 3], [2, 7, 8, 6, 5, 3, 4, 1, 9],
                [1, 6, 2, 3, 8, 5, 4, 9, 7], [5, 6, 9, 1, 2, 8, 4, 3, 7],
                [6, 1, 4, 7, 5, 8, 3, 9, 2]]
}

#
# Data from
# http://www.csee.wvu.edu/~ksmani/courses/fa01/random/lecnotes/lecture5.pdf
#
problem3 = {
    "rankWomen": [[1, 2, 3, 4], [4, 3, 2, 1], [1, 2, 3, 4], [3, 4, 1, 2]],
    "rankMen": [[1, 2, 3, 4], [2, 1, 3, 4], [1, 4, 3, 2], [4, 3, 1, 2]]
}

#
# Data from
# http://www.comp.rgu.ac.uk/staff/ha/ZCSP/additional_problems/stable_marriage/stable_marriage.pdf
# page 4
#
problem4 = {
    "rankWomen": [[1, 5, 4, 6, 2, 3], [4, 1, 5, 2, 6, 3], [6, 4, 2, 1, 5, 3],
                  [1, 5, 2, 4, 3, 6], [4, 2, 1, 5, 6, 3], [2, 6, 3, 5, 1, 4]],
    "rankMen": [[1, 4, 2, 5, 6, 3], [3, 4, 6, 1, 5, 2], [1, 6, 4, 2, 3, 5],
                [6, 5, 3, 4, 2, 1], [3, 1, 2, 4, 5, 6], [2, 3, 1, 6, 5, 4]]
}

if __name__ == "__main__":
  main(van_hentenryck, "Van Hentenryck")
  main(mathworld, "MathWorld")
  main(problem3, "Problem 3")
  main(problem4, "Problem4")
