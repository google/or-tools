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

  Organizing a day in Google CP Solver.

  Simple scheduling problem.

  Problem formulation from ECLiPSe:
  Slides on (Finite Domain) Constraint Logic Programming, page 38f
  http://eclipse-clp.org/reports/eclipse.ppt


  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/organize_day.mzn
  * Comet: http://www.hakank.org/comet/organize_day.co
  * Gecode: http://hakank.org/gecode/organize_day.cpp

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp

#
# No overlapping of tasks s1 and s2
#


def no_overlap(solver, s1, d1, s2, d2):
  b1 = solver.IsLessOrEqualVar(s1 + d1, s2)  # s1 + d1 <= s2
  b2 = solver.IsLessOrEqualVar(s2 + d2, s1)  # s2 + d2 <= s1
  solver.Add(b1 + b2 >= 1)


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Organizing a day')

  #
  # data
  #
  n = 4

  tasks = list(range(n))
  work, mail, shop, bank = tasks
  durations = [4, 1, 2, 1]

  # task [i,0] must be finished before task [i,1]
  before_tasks = [[bank, shop], [mail, work]]

  # the valid times of the day
  begin = 9
  end = 17

  #
  # declare variables
  #
  begins = [solver.IntVar(begin, end, 'begins[%i]% % i') for i in tasks]
  ends = [solver.IntVar(begin, end, 'ends[%i]% % i') for i in tasks]

  #
  # constraints
  #
  for i in tasks:
    solver.Add(ends[i] == begins[i] + durations[i])

  for i in tasks:
    for j in tasks:
      if i < j:
        no_overlap(solver, begins[i], durations[i], begins[j], durations[j])

  # specific constraints
  for (before, after) in before_tasks:
    solver.Add(ends[before] <= begins[after])

  solver.Add(begins[work] >= 11)

  #
  # solution and search
  #
  db = solver.Phase(begins + ends, solver.INT_VAR_DEFAULT,
                    solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print('begins:', [begins[i].Value() for i in tasks])
    print('ends:', [ends[i].Value() for i in tasks])
    print()

  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
