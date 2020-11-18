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

  Car sequencing in Google CP Solver.

  This model is based on the car sequencing model in
  Pascal Van Hentenryck
  'The OPL Optimization Programming Language', page 184ff.


  Compare with the following models:
  * MiniZinc: http://hakank.org/minizinc/car.mzn
  * Comet: http://hakank.org/comet/car.co

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys

from ortools.constraint_solver import pywrapcp


def main(num_sol=3):

  # Create the solver.
  solver = pywrapcp.Solver("Car sequence")

  #
  # data
  #
  nbCars = 6
  nbOptions = 5
  nbSlots = 10

  Cars = list(range(nbCars))
  Options = list(range(nbOptions))
  Slots = list(range(nbSlots))

  #    car 0   1  2  3  4  5
  demand = [1, 1, 2, 2, 2, 2]

  option = [
      # car 0  1  2  3  4  5
      [1, 0, 0, 0, 1, 1],  # option 1
      [0, 0, 1, 1, 0, 1],  # option 2
      [1, 0, 0, 0, 1, 0],  # option 3
      [1, 1, 0, 1, 0, 0],  # option 4
      [0, 0, 1, 0, 0, 0]  # option 5
  ]

  capacity = [(1, 2), (2, 3), (1, 3), (2, 5), (1, 5)]

  optionDemand = [
      sum([demand[j] * option[i][j] for j in Cars]) for i in Options
  ]

  #
  # declare variables
  #
  slot = [solver.IntVar(0, nbCars - 1, "slot[%i]" % i) for i in Slots]
  setup = {}
  for i in Options:
    for j in Slots:
      setup[(i, j)] = solver.IntVar(0, 1, "setup[%i,%i]" % (i, j))
  setup_flat = [setup[i, j] for i in Options for j in Slots]

  #
  # constraints
  #
  for c in Cars:
    b = [solver.IsEqualCstVar(slot[s], c) for s in Slots]
    solver.Add(solver.Sum(b) == demand[c])

  for o in Options:
    for s in range(0, nbSlots - capacity[o][1] + 1):
      b = [setup[o, j] for j in range(s, s + capacity[o][1] - 1)]
      solver.Add(solver.Sum(b) <= capacity[o][0])

  for o in Options:
    for s in Slots:
      solver.Add(setup[(o, s)] == solver.Element(option[o], slot[s]))

  for o in Options:
    for i in range(optionDemand[o]):
      s_range = list(range(0, nbSlots - (i + 1) * capacity[o][1]))
      ss = [setup[o, s] for s in s_range]
      cc = optionDemand[o] - (i + 1) * capacity[o][0]
      if len(ss) > 0 and cc >= 0:
        solver.Add(solver.Sum(ss) >= cc)

  #
  # search and result
  #
  db = solver.Phase(slot + setup_flat, solver.CHOOSE_FIRST_UNBOUND,
                    solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    print("slot:%s" % ",".join([str(slot[i].Value()) for i in Slots]))
    print("setup:")
    for o in Options:
      print("%i/%i:" % (capacity[o][0], capacity[o][1]), end=" ")
      for s in Slots:
        print(setup[o, s].Value(), end=" ")
      print()
    print()
    num_solutions += 1

    if num_solutions >= num_sol:
      break

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


num_sol = 3
if __name__ == "__main__":
  if len(sys.argv) > 1:
    num_sol = int(sys.argv[1])
  main(num_sol)
