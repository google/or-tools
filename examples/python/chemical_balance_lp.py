# Copyright 2010-2021 Google LLC
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

# We are trying to group items in equal sized groups.
# Each item has a color and a value. We want the sum of values of each group to
# be as close to the average as possible.
# Furthermore, if one color is an a group, at least k items with this color must
# be in that group.


from ortools.linear_solver import pywraplp

import math

# Data

max_quantities = [["N_Total", 1944], ["P2O5", 1166.4], ["K2O", 1822.5],
                  ["CaO", 1458], ["MgO", 486], ["Fe", 9.7], ["B", 2.4]]

chemical_set = [["A", 0, 0, 510, 540, 0, 0, 0], ["B", 110, 0, 0, 0, 160, 0, 0],
                ["C", 61, 149, 384, 0, 30, 1,
                 0.2], ["D", 148, 70, 245, 0, 15, 1,
                        0.2], ["E", 160, 158, 161, 0, 10, 1, 0.2]]

num_products = len(max_quantities)
all_products = range(num_products)

num_sets = len(chemical_set)
all_sets = range(num_sets)

# Model

max_set = [
    min(max_quantities[q][1] / chemical_set[s][q + 1] for q in all_products
        if chemical_set[s][q + 1] != 0.0) for s in all_sets
]

solver = pywraplp.Solver("chemical_set_lp",
                         pywraplp.Solver.GLOP_LINEAR_PROGRAMMING)

set_vars = [solver.NumVar(0, max_set[s], "set_%i" % s) for s in all_sets]

epsilon = solver.NumVar(0, 1000, "epsilon")

for p in all_products:
    solver.Add(
        sum(chemical_set[s][p + 1] * set_vars[s]
            for s in all_sets) <= max_quantities[p][1])
    solver.Add(
        sum(chemical_set[s][p + 1] * set_vars[s]
            for s in all_sets) >= max_quantities[p][1] - epsilon)

solver.Minimize(epsilon)

print(("Number of variables = %d" % solver.NumVariables()))
print(("Number of constraints = %d" % solver.NumConstraints()))

result_status = solver.Solve()

# The problem has an optimal solution.
assert result_status == pywraplp.Solver.OPTIMAL

assert solver.VerifySolution(1e-7, True)

print(("Problem solved in %f milliseconds" % solver.wall_time()))

# The objective value of the solution.
print(("Optimal objective value = %f" % solver.Objective().Value()))

for s in all_sets:
    print(
        "  %s = %f" % (chemical_set[s][0], set_vars[s].solution_value()),
        end=" ")
    print()
for p in all_products:
    name = max_quantities[p][0]
    max_quantity = max_quantities[p][1]
    quantity = sum(
        set_vars[s].solution_value() * chemical_set[s][p + 1] for s in all_sets)
    print("%s: %f out of %f" % (name, quantity, max_quantity))
