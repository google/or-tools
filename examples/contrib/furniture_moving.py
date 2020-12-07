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

  Moving furnitures (scheduling) problem in Google CP Solver.

  Marriott & Stukey: 'Programming with constraints', page  112f

  The model implements an experimental decomposition of the
  global constraint cumulative.

  Compare with the following models:
  * ECLiPSE: http://www.hakank.org/eclipse/furniture_moving.ecl
  * MiniZinc: http://www.hakank.org/minizinc/furniture_moving.mzn
  * Comet: http://www.hakank.org/comet/furniture_moving.co
  * Choco: http://www.hakank.org/choco/FurnitureMoving.java
  * Gecode: http://www.hakank.org/gecode/furniture_moving.cpp
  * JaCoP: http://www.hakank.org/JaCoP/FurnitureMoving.java
  * SICStus: http://hakank.org/sicstus/furniture_moving.pl
  * Zinc: http://hakank.org/minizinc/furniture_moving.zinc


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


#
# Decompositon of cumulative.
#
# Inspired by the MiniZinc implementation:
# http://www.g12.csse.unimelb.edu.au/wiki/doku.php?id=g12:zinc:lib:minizinc:std:cumulative.mzn&s[]=cumulative
# The MiniZinc decomposition is discussed in the paper:
# A. Schutt, T. Feydy, P.J. Stuckey, and M. G. Wallace.
# 'Why cumulative decomposition is not as bad as it sounds.'
# Download:
# http://www.cs.mu.oz.au/%7Epjs/rcpsp/papers/cp09-cu.pdf
# http://www.cs.mu.oz.au/%7Epjs/rcpsp/cumu_lazyfd.pdf
#
#
# Parameters:
#
# s: start_times    assumption: array of IntVar
# d: durations      assumption: array of int
# r: resources      assumption: array of int
# b: resource limit assumption: IntVar or int
#
def my_cumulative(solver, s, d, r, b):

  # tasks = [i for i in range(len(s))]
  tasks = [i for i in range(len(s)) if r[i] > 0 and d[i] > 0]
  times_min = min([s[i].Min() for i in tasks])
  times_max = max([s[i].Max() + max(d) for i in tasks])
  for t in range(times_min, times_max + 1):
    bb = []
    for i in tasks:
      c1 = solver.IsLessOrEqualCstVar(s[i], t)  # s[i] <= t
      c2 = solver.IsGreaterCstVar(s[i] + d[i], t)  # t < s[i] + d[i]
      bb.append(c1 * c2 * r[i])
    solver.Add(solver.Sum(bb) <= b)

  # Somewhat experimental:
  # This constraint is needed to contrain the upper limit of b.
  if not isinstance(b, int):
    solver.Add(b <= sum(r))


def main():

  # Create the solver.
  solver = pywrapcp.Solver("Furniture moving")

  #
  # data
  #
  n = 4
  duration = [30, 10, 15, 15]
  demand = [3, 1, 3, 2]
  upper_limit = 160

  #
  # declare variables
  #
  start_times = [
      solver.IntVar(0, upper_limit, "start_times[%i]" % i) for i in range(n)
  ]
  end_times = [
      solver.IntVar(0, upper_limit * 2, "end_times[%i]" % i) for i in range(n)
  ]
  end_time = solver.IntVar(0, upper_limit * 2, "end_time")

  # number of needed resources, to be minimized
  num_resources = solver.IntVar(0, 10, "num_resources")

  #
  # constraints
  #
  for i in range(n):
    solver.Add(end_times[i] == start_times[i] + duration[i])

  solver.Add(end_time == solver.Max(end_times))

  my_cumulative(solver, start_times, duration, demand, num_resources)

  #
  # Some extra constraints to play with
  #

  # all tasks must end within an hour
  # solver.Add(end_time <= 60)

  # All tasks should start at time 0
  # for i in range(n):
  #    solver.Add(start_times[i] == 0)

  # limitation of the number of people
  # solver.Add(num_resources <= 3)

  #
  # objective
  #
  # objective = solver.Minimize(end_time, 1)
  objective = solver.Minimize(num_resources, 1)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(start_times)
  solution.Add(end_times)
  solution.Add(end_time)
  solution.Add(num_resources)

  db = solver.Phase(start_times, solver.CHOOSE_FIRST_UNBOUND,
                    solver.ASSIGN_MIN_VALUE)

  #
  # result
  #
  solver.NewSearch(db, [objective])
  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print("num_resources:", num_resources.Value())
    print("start_times  :", [start_times[i].Value() for i in range(n)])
    print("duration     :", [duration[i] for i in range(n)])
    print("end_times    :", [end_times[i].Value() for i in range(n)])
    print("end_time     :", end_time.Value())
    print()

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main()
