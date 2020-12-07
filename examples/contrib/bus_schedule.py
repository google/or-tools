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

  Bus scheduling in Google CP Solver.


  Problem from Taha "Introduction to Operations Research", page 58.

  This is a slightly more general model than Taha's.

  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/bus_scheduling.mzn
  * Comet   : http://www.hakank.org/comet/bus_schedule.co
  * ECLiPSe : http://www.hakank.org/eclipse/bus_schedule.ecl
  * Gecode  : http://www.hakank.org/gecode/bus_schedule.cpp
  * Tailor/Essence'  : http://www.hakank.org/tailor/bus_schedule.eprime
  * SICStus: http://hakank.org/sicstus/bus_schedule.pl

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""
import sys
from ortools.constraint_solver import pywrapcp


def main(num_buses_check=0):

  # Create the solver.
  solver = pywrapcp.Solver("Bus scheduling")

  # data
  time_slots = 6
  demands = [8, 10, 7, 12, 4, 4]
  max_num = sum(demands)

  # declare variables
  x = [solver.IntVar(0, max_num, "x%i" % i) for i in range(time_slots)]
  num_buses = solver.IntVar(0, max_num, "num_buses")

  #
  # constraints
  #
  solver.Add(num_buses == solver.Sum(x))

  # Meet the demands for this and the next time slot
  for i in range(time_slots - 1):
    solver.Add(x[i] + x[i + 1] >= demands[i])

  # The demand "around the clock"
  solver.Add(x[time_slots - 1] + x[0] == demands[time_slots - 1])

  if num_buses_check > 0:
    solver.Add(num_buses == num_buses_check)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x)
  solution.Add(num_buses)

  collector = solver.AllSolutionCollector(solution)
  cargs = [collector]

  # objective
  if num_buses_check == 0:
    objective = solver.Minimize(num_buses, 1)
    cargs.extend([objective])

  solver.Solve(
      solver.Phase(x, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE),
      cargs)

  num_solutions = collector.SolutionCount()
  num_buses_check_value = 0
  for s in range(num_solutions):
    print("x:", [collector.Value(s, x[i]) for i in range(len(x))], end=" ")
    num_buses_check_value = collector.Value(s, num_buses)
    print(" num_buses:", num_buses_check_value)

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())
  print()
  if num_buses_check == 0:
    return num_buses_check_value


if __name__ == "__main__":
  print("Check for minimun number of buses")
  num_buses_check = main()
  print("... got ", num_buses_check, "buses")
  print("All solutions:")
  main(num_buses_check)
