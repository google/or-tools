# Copyright 2010-2017 Google
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

"""This is the Golomb ruler problem.

This model aims at maximizing radar interferences in a minimum space.
It is known as the Golomb Ruler problem.

The idea is to put marks on a rule such that all differences
between all marks are all different. The objective is to minimize the length
of the rule.
"""

from ortools.constraint_solver import pywrapcp


# We disable the following warning because it is a false positive on constraints
# like: solver.Add(x == 0)
# pylint: disable=g-explicit-bool-comparison


def main():
  # Create the solver.
  solver = pywrapcp.Solver('golomb ruler')

  size = 8
  var_max = size * size
  all_vars = list(range(0, size))

  marks = [solver.IntVar(0, var_max, 'marks_%d' % i) for i in all_vars]

  objective = solver.Minimize(marks[size - 1], 1)

  solver.Add(marks[0] == 0)
  solver.Add(solver.AllDifferent([marks[j] - marks[i]
                                  for i in range(0, size - 1)
                                  for j in range(i + 1, size)]))

  solver.Add(marks[size - 1] - marks[size - 2] > marks[1] - marks[0])
  for i in range(0, size - 2):
    solver.Add(marks[i + 1] > marks[i])

  solution = solver.Assignment()
  solution.Add(marks[size - 1])
  collector = solver.AllSolutionCollector(solution)

  solver.Solve(solver.Phase(marks,
                            solver.CHOOSE_FIRST_UNBOUND,
                            solver.ASSIGN_MIN_VALUE),
               [objective, collector])
  for i in range(0, collector.SolutionCount()):
    obj_value = collector.Value(i, marks[size - 1])
    time = collector.WallTime(i)
    branches = collector.Branches(i)
    failures = collector.Failures(i)
    print(('Solution #%i: value = %i, failures = %i, branches = %i,'
           'time = %i ms') % (i, obj_value, failures, branches, time))
  time = solver.WallTime()
  branches = solver.Branches()
  failures = solver.Failures()
  print(('Total run : failures = %i, branches = %i, time = %i ms' %
         (failures, branches, time)))


if __name__ == '__main__':
  main()
