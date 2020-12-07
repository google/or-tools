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

  Crew allocation problem  in Google CP Solver.

  From Gecode example crew
  examples/crew.cc
  '''
  * Example: Airline crew allocation
  *
  * Assign 20 flight attendants to 10 flights. Each flight needs a certain
  * number of cabin crew, and they have to speak certain languages.
  * Every cabin crew member has two flights off after an attended flight.
  *
  '''

  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/crew.mzn
  * Comet   : http://www.hakank.org/comet/crew.co
  * ECLiPSe : http://hakank.org/eclipse/crew.ecl
  * SICStus : http://hakank.org/sicstus/crew.pl


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def main(sols=1):

  # Create the solver.
  solver = pywrapcp.Solver("Crew")

  #
  # data
  #
  names = [
      "Tom", "David", "Jeremy", "Ron", "Joe", "Bill", "Fred", "Bob", "Mario",
      "Ed", "Carol", "Janet", "Tracy", "Marilyn", "Carolyn", "Cathy", "Inez",
      "Jean", "Heather", "Juliet"
  ]

  num_persons = len(names)  # number of persons

  attributes = [
      #  steward, hostess, french, spanish, german
      [1, 0, 0, 0, 1],  # Tom     = 1
      [1, 0, 0, 0, 0],  # David   = 2
      [1, 0, 0, 0, 1],  # Jeremy  = 3
      [1, 0, 0, 0, 0],  # Ron     = 4
      [1, 0, 0, 1, 0],  # Joe     = 5
      [1, 0, 1, 1, 0],  # Bill    = 6
      [1, 0, 0, 1, 0],  # Fred    = 7
      [1, 0, 0, 0, 0],  # Bob     = 8
      [1, 0, 0, 1, 1],  # Mario   = 9
      [1, 0, 0, 0, 0],  # Ed      = 10
      [0, 1, 0, 0, 0],  # Carol   = 11
      [0, 1, 0, 0, 0],  # Janet   = 12
      [0, 1, 0, 0, 0],  # Tracy   = 13
      [0, 1, 0, 1, 1],  # Marilyn = 14
      [0, 1, 0, 0, 0],  # Carolyn = 15
      [0, 1, 0, 0, 0],  # Cathy   = 16
      [0, 1, 1, 1, 1],  # Inez    = 17
      [0, 1, 1, 0, 0],  # Jean    = 18
      [0, 1, 0, 1, 1],  # Heather = 19
      [0, 1, 1, 0, 0]  # Juliet  = 20
  ]

  # The columns are in the following order:
  # staff     : Overall number of cabin crew needed
  # stewards  : How many stewards are required
  # hostesses : How many hostesses are required
  # french    : How many French speaking employees are required
  # spanish   : How many Spanish speaking employees are required
  # german    : How many German speaking employees are required
  required_crew = [
      [4, 1, 1, 1, 1, 1],  # Flight 1
      [5, 1, 1, 1, 1, 1],  # Flight 2
      [5, 1, 1, 1, 1, 1],  # ..
      [6, 2, 2, 1, 1, 1],
      [7, 3, 3, 1, 1, 1],
      [4, 1, 1, 1, 1, 1],
      [5, 1, 1, 1, 1, 1],
      [6, 1, 1, 1, 1, 1],
      [6, 2, 2, 1, 1, 1],  # ...
      [7, 3, 3, 1, 1, 1]  # Flight 10
  ]

  num_flights = len(required_crew)  # number of flights

  #
  # declare variables
  #
  crew = {}
  for i in range(num_flights):
    for j in range(num_persons):
      crew[(i, j)] = solver.IntVar(0, 1, "crew[%i,%i]" % (i, j))
  crew_flat = [
      crew[(i, j)] for i in range(num_flights) for j in range(num_persons)
  ]

  # number of working persons
  num_working = solver.IntVar(1, num_persons, "num_working")

  #
  # constraints
  #

  # number of working persons
  solver.Add(num_working == solver.Sum([
      solver.IsGreaterOrEqualCstVar(
          solver.Sum([crew[(f, p)]
                      for f in range(num_flights)]), 1)
      for p in range(num_persons)
  ]))

  for f in range(num_flights):
    # size of crew
    tmp = [crew[(f, i)] for i in range(num_persons)]
    solver.Add(solver.Sum(tmp) == required_crew[f][0])

    # attributes and requirements
    for j in range(5):
      tmp = [attributes[i][j] * crew[(f, i)] for i in range(num_persons)]
      solver.Add(solver.Sum(tmp) >= required_crew[f][j + 1])

  # after a flight, break for at least two flights
  for f in range(num_flights - 2):
    for i in range(num_persons):
      solver.Add(crew[f, i] + crew[f + 1, i] + crew[f + 2, i] <= 1)

  # extra contraint: all must work at least two of the flights
  # for i in range(num_persons):
  #     [solver.Add(solver.Sum([crew[f,i] for f in range(num_flights)]) >= 2) ]

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(crew_flat)
  solution.Add(num_working)

  db = solver.Phase(crew_flat, solver.CHOOSE_FIRST_UNBOUND,
                    solver.ASSIGN_MIN_VALUE)

  #
  # result
  #
  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print("Solution #%i" % num_solutions)
    print("Number working:", num_working.Value())
    for i in range(num_flights):
      for j in range(num_persons):
        print(crew[i, j].Value(), end=" ")
      print()
    print()

    print("Flights:")
    for flight in range(num_flights):
      print("Flight", flight, "persons:", end=" ")
      for person in range(num_persons):
        if crew[flight, person].Value() == 1:
          print(names[person], end=" ")
      print()
    print()

    print("Crew:")
    for person in range(num_persons):
      print("%-10s flights" % names[person], end=" ")
      for flight in range(num_flights):
        if crew[flight, person].Value() == 1:
          print(flight, end=" ")
      print()
    print()

    if num_solutions >= sols:
      break
  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


num_solutions_to_show = 1
if __name__ == "__main__":
  if (len(sys.argv) > 1):
    num_solutions_to_show = int(sys.argv[1])

  main(num_solutions_to_show)
