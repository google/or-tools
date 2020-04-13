#
# Copyright 2018 Turadg Aleahmad
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

from ortools.constraint_solver import pywrapcp
from ortools.constraint_solver import solver_parameters_pb2
"""Finding an optimal wedding seating chart.

From
Meghan L. Bellows and J. D. Luc Peterson
"Finding an optimal seating chart for a wedding"
http://www.improbable.com/news/2012/Optimal-seating-chart.pdf
http://www.improbable.com/2012/02/12/finding-an-optimal-seating-chart-for-a-wedding

Every year, millions of brides (not to mention their mothers, future
mothers-in-law, and occasionally grooms) struggle with one of the
most daunting tasks during the wedding-planning process: the
seating chart. The guest responses are in, banquet hall is booked,
menu choices have been made. You think the hard parts are over,
but you have yet to embark upon the biggest headache of them all.
In order to make this process easier, we present a mathematical
formulation that models the seating chart problem. This model can
be solved to find the optimal arrangement of guests at tables.
At the very least, it can provide a starting point and hopefully
minimize stress and arguments.

Adapted from
https://github.com/google/or-tools/blob/master/examples/csharp/wedding_optimal_chart.cs
"""


def main():
  # Instantiate a CP solver.
  parameters = pywrapcp.Solver.DefaultSolverParameters()
  solver = pywrapcp.Solver("WeddingOptimalChart", parameters)

  #
  # Data
  #

  # Easy problem (from the paper)
  # n = 2  # number of tables
  # a = 10 # maximum number of guests a table can seat
  # b = 1  # minimum number of people each guest knows at their table

  # Slightly harder problem (also from the paper)
  n = 5  # number of tables
  a = 4  # maximum number of guests a table can seat
  b = 1  # minimum number of people each guest knows at their table

  # Connection matrix: who knows who, and how strong
  # is the relation
  C = [[1, 50, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0],
       [50, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0],
       [1, 1, 1, 50, 1, 1, 1, 1, 10, 0, 0, 0, 0, 0, 0, 0, 0],
       [1, 1, 50, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0],
       [1, 1, 1, 1, 1, 50, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0],
       [1, 1, 1, 1, 50, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0],
       [1, 1, 1, 1, 1, 1, 1, 50, 1, 0, 0, 0, 0, 0, 0, 0, 0],
       [1, 1, 1, 1, 1, 1, 50, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0],
       [1, 1, 10, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0],
       [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 50, 1, 1, 1, 1, 1, 1],
       [0, 0, 0, 0, 0, 0, 0, 0, 0, 50, 1, 1, 1, 1, 1, 1, 1],
       [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1],
       [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1],
       [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1],
       [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1],
       [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1],
       [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]]

  # Names of the guests. B: Bride side, G: Groom side
  names = [
      "Deb (B)", "John (B)", "Martha (B)", "Travis (B)", "Allan (B)",
      "Lois (B)", "Jayne (B)", "Brad (B)", "Abby (B)", "Mary Helen (G)",
      "Lee (G)", "Annika (G)", "Carl (G)", "Colin (G)", "Shirley (G)",
      "DeAnn (G)", "Lori (G)"
  ]

  m = len(C)

  NRANGE = range(n)
  MRANGE = range(m)

  #
  # Decision variables
  #
  tables = [solver.IntVar(0, n - 1, "x[%i]" % i) for i in MRANGE]

  z = solver.Sum([
      C[j][k] * (tables[j] == tables[k])
      for j in MRANGE
      for k in MRANGE
      if j < k
  ])

  #
  # Constraints
  #
  for i in NRANGE:
    minGuests = [(tables[j] == i) * (tables[k] == i)
                 for j in MRANGE
                 for k in MRANGE
                 if j < k and C[j][k] > 0]
    solver.Add(solver.Sum(minGuests) >= b)

    maxGuests = [tables[j] == i for j in MRANGE]
    solver.Add(solver.Sum(maxGuests) <= a)

  # Symmetry breaking
  solver.Add(tables[0] == 0)

  #
  # Objective
  #
  objective = solver.Maximize(z, 1)

  #
  # Search
  #
  db = solver.Phase(tables, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db, [objective])

  while solver.NextSolution():
    print("z:", z)
    print("Table: ")
    for j in MRANGE:
      print(tables[j].Value(), " ")
    print()

    for i in NRANGE:
      print("Table %d: " % i)
      for j in MRANGE:
        if tables[j].Value() == i:
          print(names[j] + " ")
      print()

    print()

  solver.EndSearch()

  print()
  print("Solutions: %d" % solver.Solutions())
  print("WallTime: %dms" % solver.WallTime())
  print("Failures: %d" % solver.Failures())
  print("Branches: %d" % solver.Branches())


if __name__ == "__main__":
  main()
