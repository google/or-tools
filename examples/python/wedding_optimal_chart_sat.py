#
# Copyright 2018 Google.
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

import time
from ortools.sat.python import cp_model


class WeddingChartPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, seats, names, num_tables, num_guests):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__solution_count = 0
        self.__start_time = time.time()
        self.__seats = seats
        self.__names = names
        self.__num_tables = num_tables
        self.__num_guests = num_guests

    def on_solution_callback(self):
        current_time = time.time()
        objective = self.ObjectiveValue()
        print("Solution %i, time = %f s, objective = %i" %
              (self.__solution_count, current_time - self.__start_time,
               objective))
        self.__solution_count += 1

        for t in range(self.__num_tables):
            print("Table %d: " % t)
            for g in range(self.__num_guests):
                if self.Value(self.__seats[(t, g)]):
                    print("  " + self.__names[g])

    def num_solutions(self):
        return self.__solution_count


def BuildData():
    #
    # Data
    #

    # Easy problem (from the paper)
    # num_tables = 2
    # table_capacity = 10
    # min_known_neighbors = 1

    # Slightly harder problem (also from the paper)
    num_tables = 5
    table_capacity = 4
    min_known_neighbors = 1

    # Connection matrix: who knows who, and how strong
    # is the relation
    C = [[1, 50, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
          0], [50, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
               0], [1, 1, 1, 50, 1, 1, 1, 1, 10, 0, 0, 0, 0, 0, 0, 0,
                    0], [1, 1, 50, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0],
         [1, 1, 1, 1, 1, 50, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
          0], [1, 1, 1, 1, 50, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
               0], [1, 1, 1, 1, 1, 1, 1, 50, 1, 0, 0, 0, 0, 0, 0, 0, 0],
         [1, 1, 1, 1, 1, 1, 50, 1, 1, 0, 0, 0, 0, 0, 0, 0,
          0], [1, 1, 10, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
               0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 50, 1, 1, 1, 1, 1, 1],
         [0, 0, 0, 0, 0, 0, 0, 0, 0, 50, 1, 1, 1, 1, 1, 1,
          1], [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
               1], [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1], [
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1
               ], [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1], [
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1
               ], [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]]

    # Names of the guests. B: Bride side, G: Groom side
    names = [
        "Deb (B)", "John (B)", "Martha (B)", "Travis (B)", "Allan (B)",
        "Lois (B)", "Jayne (B)", "Brad (B)", "Abby (B)", "Mary Helen (G)",
        "Lee (G)", "Annika (G)", "Carl (G)", "Colin (G)", "Shirley (G)",
        "DeAnn (G)", "Lori (G)"
    ]
    return num_tables, table_capacity, min_known_neighbors, C, names


def solve_with_discrete_model():
    num_tables, table_capacity, min_known_neighbors, C, names = BuildData()

    num_guests = len(C)

    all_tables = range(num_tables)
    all_guests = range(num_guests)

    # Create the cp model.
    model = cp_model.CpModel()

    #
    # Decision variables
    #
    seats = {}
    for t in all_tables:
        for g in all_guests:
            seats[(t, g)] = model.NewBoolVar("guest %i seats on table %i" % (g,
                                                                             t))

    colocated = {}
    for g1 in range(num_guests - 1):
        for g2 in range(g1 + 1, num_guests):
            colocated[(g1, g2)] = model.NewBoolVar(
                "guest %i seats with guest %i" % (g1, g2))

    same_table = {}
    for g1 in range(num_guests - 1):
        for g2 in range(g1 + 1, num_guests):
            for t in all_tables:
                same_table[(g1, g2, t)] = model.NewBoolVar(
                    "guest %i seats with guest %i on table %i" % (g1, g2, t))

    # Objective
    model.Maximize(
        sum(C[g1][g2] * colocated[g1, g2]
            for g1 in range(num_guests - 1) for g2 in range(g1 + 1, num_guests)
            if C[g1][g2] > 0))

    #
    # Constraints
    #

    # Everybody seats at one table.
    for g in all_guests:
        model.Add(sum(seats[(t, g)] for t in all_tables) == 1)

    # Tables have a max capacity.
    for t in all_tables:
        model.Add(sum(seats[(t, g)] for g in all_guests) <= table_capacity)

    # Link colocated with seats
    for g1 in range(num_guests - 1):
        for g2 in range(g1 + 1, num_guests):
            for t in all_tables:
                # Link same_table and seats.
                model.AddBoolOr([
                    seats[(t, g1)].Not(), seats[(t, g2)].Not(),
                    same_table[(g1, g2, t)]
                ])
                model.AddImplication(same_table[(g1, g2, t)], seats[(t, g1)])
                model.AddImplication(same_table[(g1, g2, t)], seats[(t, g2)])

            # Link colocated and same_table.
            model.Add(
                sum(same_table[(g1, g2, t)]
                    for t in all_tables) == colocated[(g1, g2)])

    # Min known neighbors rule.
    for g in all_guests:
        model.Add(
            sum(same_table[(g, g2, t)]
                for g2 in range(g + 1, num_guests)
                for t in all_tables
                if C[g][g2] > 0) +
            sum(same_table[(g1, g, t)]
                for g1 in range(g)
                for t in all_tables
                if C[g1][g] > 0)
            >= min_known_neighbors)

    # Symmetry breaking. First guest seats on the first table.
    model.Add(seats[(0, 0)] == 1)

    ### Solve model.
    solver = cp_model.CpSolver()
    solution_printer = WeddingChartPrinter(seats, names, num_tables, num_guests)
    solver.Solve(model, solution_printer)

    print("Statistics")
    print("  - conflicts    : %i" % solver.NumConflicts())
    print("  - branches     : %i" % solver.NumBranches())
    print("  - wall time    : %f s" % solver.WallTime())
    print("  - num solutions: %i" % solution_printer.num_solutions())


solve_with_discrete_model()
