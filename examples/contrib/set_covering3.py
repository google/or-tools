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

  Set covering in Google CP Solver.

  Problem from
  Katta G. Murty: 'Optimization Models for Decision Making', page 302f
  http://ioe.engin.umich.edu/people/fac/books/murty/opti_model/junior-7.pdf

  10 senators making a committee, where there must at least be one
  representative from each group:
  group:        senators:
  southern      1 2 3 4 5
  northern      6 7 8 9 10
  liberals      2 3 8 9 10
  conservative  1 5 6 7
  democrats     3 4 5 6 7 9
  republicans   1 2 8 10

  The objective is to minimize the number of senators.

  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/set_covering3_model.mzn (model)
              http://www.hakank.org/minizinc/set_covering3.mzn (data)
  * Comet   : http://www.hakank.org/comet/set_covering3.co
  * ECLiPSe : http://www.hakank.org/eclipse/set_covering3.ecl
  * SICStus : http://hakank.org/sicstus/set_covering3.pl
  * Gecode  : http://hakank.org/gecode/set_covering3.cpp


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""
from ortools.constraint_solver import pywrapcp


def main(unused_argv):

  # Create the solver.
  solver = pywrapcp.Solver("Set covering")

  #
  # data
  #
  num_groups = 6
  num_senators = 10

  # which group does a senator belong to?
  belongs = [
      [1, 1, 1, 1, 1, 0, 0, 0, 0, 0],  # 1 southern
      [0, 0, 0, 0, 0, 1, 1, 1, 1, 1],  # 2 northern
      [0, 1, 1, 0, 0, 0, 0, 1, 1, 1],  # 3 liberals
      [1, 0, 0, 0, 1, 1, 1, 0, 0, 0],  # 4 conservative
      [0, 0, 1, 1, 1, 1, 1, 0, 1, 0],  # 5 democrats
      [1, 1, 0, 0, 0, 0, 0, 1, 0, 1]  # 6 republicans
  ]

  #
  # declare variables
  #
  x = [solver.IntVar(0, 1, "x[%i]" % i) for i in range(num_senators)]

  #
  # constraints
  #

  # number of assigned senators (to minimize)
  z = solver.Sum(x)

  # ensure that each group is covered by at least
  # one senator
  for i in range(num_groups):
    solver.Add(
        solver.SumGreaterOrEqual(
            [x[j] * belongs[i][j] for j in range(num_senators)], 1))

  objective = solver.Minimize(z, 1)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x)
  solution.AddObjective(z)

  collector = solver.LastSolutionCollector(solution)
  solver.Solve(
      solver.Phase(x, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT),
      [collector, objective])

  print("z:", collector.ObjectiveValue(0))
  print("x:", [collector.Value(0, x[i]) for i in range(num_senators)])
  for j in range(num_senators):
    if collector.Value(0, x[j]) == 1:
      print("Senator", j + 1, "belongs to these groups:", end=" ")
      for i in range(num_groups):
        if belongs[i][j] == 1:
          print(i + 1, end=" ")
      print()

  print()
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main("cp sample")
