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

"""Magic sequence problem.

This models aims at building a sequence of numbers such that the number of
occurrences of i in this sequence is equal to the value of the ith number.
It uses an aggregated formulation of the count expression called
distribute().
"""
from __future__ import print_function


from ortools.constraint_solver import pywrapcp


def main():
  # Create the solver.
  solver = pywrapcp.Solver('magic sequence')

  size = 100
  all_values = list(range(0, size))
  all_vars = [solver.IntVar(0, size, 'vars_%d' % i) for i in all_values]

  solver.Add(solver.Distribute(all_vars, all_values, all_vars))
  solver.Add(solver.Sum(all_vars) == size)

  solver.NewSearch(solver.Phase(all_vars,
                                solver.CHOOSE_FIRST_UNBOUND,
                                solver.ASSIGN_MIN_VALUE))
  solver.NextSolution()
  print(all_vars)
  solver.EndSearch()


if __name__ == '__main__':
  main()
