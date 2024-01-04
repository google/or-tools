#!/usr/bin/env python3
# Copyright 2010-2024 Google LLC
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

"""Solves the Stell Mill Slab problem with 4 different techniques."""

# overloaded sum() clashes with pytype.

import collections
import time

from absl import app
from absl import flags
from google.protobuf import text_format
from ortools.sat.python import cp_model


_PROBLEM = flags.DEFINE_integer("problem", 2, "Problem id to solve.")
_BREAK_SYMMETRIES = flags.DEFINE_boolean(
    "break_symmetries", True, "Break symmetries between equivalent orders."
)
_SOLVER = flags.DEFINE_string(
    "solver", "sat_column", "Method used to solve: sat, sat_table, sat_column."
)
_PARAMS = flags.DEFINE_string(
    "params",
    "max_time_in_seconds:20,num_workers:8,log_search_progress:true",
    "CP-SAT parameters.",
)


def build_problem(problem_id):
    """Build problem data."""
    capacities = None
    num_colors = None
    num_slabs = None
    orders = None

    if problem_id == 0:
        capacities = [
            # fmt:off
        0, 12, 14, 17, 18, 19, 20, 23, 24, 25, 26, 27, 28, 29, 30, 32, 35, 39, 42, 43, 44,
            # fmt:on
        ]
        num_colors = 88
        num_slabs = 111
        orders = [  # (size, color)
            # fmt:off
        (4, 1), (22, 2), (9, 3), (5, 4), (8, 5), (3, 6), (3, 4), (4, 7),
        (7, 4), (7, 8), (3, 6), (2, 6), (2, 4), (8, 9), (5, 10), (7, 11),
        (4, 7), (7, 11), (5, 10), (7, 11), (8, 9), (3, 1), (25, 12), (14, 13),
        (3, 6), (22, 14), (19, 15), (19, 15), (22, 16), (22, 17), (22, 18),
        (20, 19), (22, 20), (5, 21), (4, 22), (10, 23), (26, 24), (17, 25),
        (20, 26), (16, 27), (10, 28), (19, 29), (10, 30), (10, 31), (23, 32),
        (22, 33), (26, 34), (27, 35), (22, 36), (27, 37), (22, 38), (22, 39),
        (13, 40), (14, 41), (16, 27), (26, 34), (26, 42), (27, 35), (22, 36),
        (20, 43), (26, 24), (22, 44), (13, 45), (19, 46), (20, 47), (16, 48),
        (15, 49), (17, 50), (10, 28), (20, 51), (5, 52), (26, 24), (19, 53),
        (15, 54), (10, 55), (10, 56), (13, 57), (13, 58), (13, 59), (12, 60),
        (12, 61), (18, 62), (10, 63), (18, 64), (16, 65), (20, 66), (12, 67),
        (6, 68), (6, 68), (15, 69), (15, 70), (15, 70), (21, 71), (30, 72),
        (30, 73), (30, 74), (30, 75), (23, 76), (15, 77), (15, 78), (27, 79),
        (27, 80), (27, 81), (27, 82), (27, 83), (27, 84), (27, 79), (27, 85),
        (27, 86), (10, 87), (3, 88),
            # fmt:on
        ]
    elif problem_id == 1:
        capacities = [0, 17, 44]
        num_colors = 23
        num_slabs = 30
        orders = [  # (size, color)
            # fmt:off
        (4, 1), (22, 2), (9, 3), (5, 4), (8, 5), (3, 6), (3, 4), (4, 7), (7, 4),
        (7, 8), (3, 6), (2, 6), (2, 4), (8, 9), (5, 10), (7, 11), (4, 7), (7, 11),
        (5, 10), (7, 11), (8, 9), (3, 1), (25, 12), (14, 13), (3, 6), (22, 14),
        (19, 15), (19, 15), (22, 16), (22, 17), (22, 18), (20, 19), (22, 20),
        (5, 21), (4, 22), (10, 23),
            # fmt:on
        ]
    elif problem_id == 2:
        capacities = [0, 17, 44]
        num_colors = 15
        num_slabs = 20
        orders = [  # (size, color)
            # fmt:off
        (4, 1), (22, 2), (9, 3), (5, 4), (8, 5), (3, 6), (3, 4), (4, 7), (7, 4),
        (7, 8), (3, 6), (2, 6), (2, 4), (8, 9), (5, 10), (7, 11), (4, 7), (7, 11),
        (5, 10), (7, 11), (8, 9), (3, 1), (25, 12), (14, 13), (3, 6), (22, 14),
        (19, 15), (19, 15),
            # fmt:on
        ]

    elif problem_id == 3:
        capacities = [0, 17, 44]
        num_colors = 8
        num_slabs = 10
        orders = [  # (size, color)
            # fmt:off
        (4, 1), (22, 2), (9, 3), (5, 4), (8, 5), (3, 6), (3, 4), (4, 7),
        (7, 4), (7, 8), (3, 6),
            # fmt:on
        ]

    return (num_slabs, capacities, num_colors, orders)


class SteelMillSlabSolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, orders, assign, load, loss):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__orders = orders
        self.__assign = assign
        self.__load = load
        self.__loss = loss
        self.__solution_count = 0
        self.__all_orders = range(len(orders))
        self.__all_slabs = range(len(assign[0]))
        self.__start_time = time.time()

    def on_solution_callback(self):
        """Called on each new solution."""
        current_time = time.time()
        objective = sum(self.value(l) for l in self.__loss)
        print(
            "Solution %i, time = %f s, objective = %i"
            % (self.__solution_count, current_time - self.__start_time, objective)
        )
        self.__solution_count += 1
        orders_in_slab = [
            [o for o in self.__all_orders if self.value(self.__assign[o][s])]
            for s in self.__all_slabs
        ]
        for s in self.__all_slabs:
            if orders_in_slab[s]:
                line = "  - slab %i, load = %i, loss = %i, orders = [" % (
                    s,
                    self.value(self.__load[s]),
                    self.value(self.__loss[s]),
                )
                for o in orders_in_slab[s]:
                    line += "#%i(w%i, c%i) " % (
                        o,
                        self.__orders[o][0],
                        self.__orders[o][1],
                    )
                line += "]"
                print(line)


def steel_mill_slab(problem, break_symmetries):
    """Solves the Steel Mill Slab Problem."""
    ### Load problem.
    (num_slabs, capacities, num_colors, orders) = build_problem(problem)

    num_orders = len(orders)
    num_capacities = len(capacities)
    all_slabs = range(num_slabs)
    all_colors = range(num_colors)
    all_orders = range(len(orders))
    print(
        "Solving steel mill with %i orders, %i slabs, and %i capacities"
        % (num_orders, num_slabs, num_capacities - 1)
    )

    # Compute auxiliary data.
    widths = [x[0] for x in orders]
    colors = [x[1] for x in orders]
    max_capacity = max(capacities)
    loss_array = [
        min(x for x in capacities if x >= c) - c for c in range(max_capacity + 1)
    ]
    max_loss = max(loss_array)
    orders_per_color = [
        [o for o in all_orders if colors[o] == c + 1] for c in all_colors
    ]
    unique_color_orders = [
        o for o in all_orders if len(orders_per_color[colors[o] - 1]) == 1
    ]

    ### Model problem.

    # Create the model and the decision variables.
    model = cp_model.CpModel()
    assign = [
        [model.new_bool_var("assign_%i_to_slab_%i" % (o, s)) for s in all_slabs]
        for o in all_orders
    ]
    loads = [
        model.new_int_var(0, max_capacity, "load_of_slab_%i" % s) for s in all_slabs
    ]
    color_is_in_slab = [
        [model.new_bool_var("color_%i_in_slab_%i" % (c + 1, s)) for c in all_colors]
        for s in all_slabs
    ]

    # Compute load of all slabs.
    for s in all_slabs:
        model.add(sum(assign[o][s] * widths[o] for o in all_orders) == loads[s])

    # Orders are assigned to one slab.
    for o in all_orders:
        model.add_exactly_one(assign[o])

    # Redundant constraint (sum of loads == sum of widths).
    model.add(sum(loads) == sum(widths))

    # Link present_colors and assign.
    for c in all_colors:
        for s in all_slabs:
            for o in orders_per_color[c]:
                model.add_implication(assign[o][s], color_is_in_slab[s][c])
                model.add_implication(~color_is_in_slab[s][c], ~assign[o][s])

    # At most two colors per slab.
    for s in all_slabs:
        model.add(sum(color_is_in_slab[s]) <= 2)

    # Project previous constraint on unique_color_orders
    for s in all_slabs:
        model.add(sum(assign[o][s] for o in unique_color_orders) <= 2)

    # Symmetry breaking.
    for s in range(num_slabs - 1):
        model.add(loads[s] >= loads[s + 1])

    # Collect equivalent orders.
    width_to_unique_color_order = {}
    ordered_equivalent_orders = []
    for c in all_colors:
        colored_orders = orders_per_color[c]
        if not colored_orders:
            continue
        if len(colored_orders) == 1:
            o = colored_orders[0]
            w = widths[o]
            if w not in width_to_unique_color_order:
                width_to_unique_color_order[w] = [o]
            else:
                width_to_unique_color_order[w].append(o)
        else:
            local_width_to_order = {}
            for o in colored_orders:
                w = widths[o]
                if w not in local_width_to_order:
                    local_width_to_order[w] = []
                local_width_to_order[w].append(o)
            for _, os in local_width_to_order.items():
                if len(os) > 1:
                    for p in range(len(os) - 1):
                        ordered_equivalent_orders.append((os[p], os[p + 1]))
    for _, os in width_to_unique_color_order.items():
        if len(os) > 1:
            for p in range(len(os) - 1):
                ordered_equivalent_orders.append((os[p], os[p + 1]))

    # Create position variables if there are symmetries to be broken.
    if break_symmetries and ordered_equivalent_orders:
        print(
            "  - creating %i symmetry breaking constraints"
            % len(ordered_equivalent_orders)
        )
        positions = {}
        for p in ordered_equivalent_orders:
            if p[0] not in positions:
                positions[p[0]] = model.new_int_var(
                    0, num_slabs - 1, "position_of_slab_%i" % p[0]
                )
                model.add_map_domain(positions[p[0]], assign[p[0]])
            if p[1] not in positions:
                positions[p[1]] = model.new_int_var(
                    0, num_slabs - 1, "position_of_slab_%i" % p[1]
                )
                model.add_map_domain(positions[p[1]], assign[p[1]])
            # Finally add the symmetry breaking constraint.
            model.add(positions[p[0]] <= positions[p[1]])

    # Objective.
    obj = model.new_int_var(0, num_slabs * max_loss, "obj")
    losses = [model.new_int_var(0, max_loss, "loss_%i" % s) for s in all_slabs]
    for s in all_slabs:
        model.add_element(loads[s], loss_array, losses[s])
    model.add(obj == sum(losses))
    model.minimize(obj)

    ### Solve model.
    solver = cp_model.CpSolver()
    if _PARAMS.value:
        text_format.Parse(_PARAMS.value, solver.parameters)
    objective_printer = cp_model.ObjectiveSolutionPrinter()
    status = solver.solve(model, objective_printer)

    ### Output the solution.
    if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        print(
            "Loss = %i, time = %f s, %i conflicts"
            % (solver.objective_value, solver.wall_time, solver.num_conflicts)
        )
    else:
        print("No solution")


def collect_valid_slabs_dp(capacities, colors, widths, loss_array):
    """Collect valid columns (assign, loss) for one slab."""
    start_time = time.time()

    max_capacity = max(capacities)

    valid_assignment = collections.namedtuple("valid_assignment", "orders load colors")
    all_valid_assignments = [valid_assignment(orders=[], load=0, colors=[])]

    for order_id, new_color in enumerate(colors):
        new_width = widths[order_id]
        new_assignments = []
        for assignment in all_valid_assignments:
            if assignment.load + new_width > max_capacity:
                continue
            new_colors = list(assignment.colors)
            if new_color not in new_colors:
                new_colors.append(new_color)
            if len(new_colors) > 2:
                continue
            new_assignment = valid_assignment(
                orders=assignment.orders + [order_id],
                load=assignment.load + new_width,
                colors=new_colors,
            )
            new_assignments.append(new_assignment)
        all_valid_assignments.extend(new_assignments)

    print(
        "%i assignments created in %.2f s"
        % (len(all_valid_assignments), time.time() - start_time)
    )
    tuples = []
    for assignment in all_valid_assignments:
        solution = [0] * len(colors)
        for i in assignment.orders:
            solution[i] = 1
        solution.append(loss_array[assignment.load])
        solution.append(assignment.load)
        tuples.append(solution)

    return tuples


def steel_mill_slab_with_valid_slabs(problem, break_symmetries):
    """Solves the Steel Mill Slab Problem."""
    ### Load problem.
    (num_slabs, capacities, num_colors, orders) = build_problem(problem)

    num_orders = len(orders)
    num_capacities = len(capacities)
    all_slabs = range(num_slabs)
    all_colors = range(num_colors)
    all_orders = range(len(orders))
    print(
        "Solving steel mill with %i orders, %i slabs, and %i capacities"
        % (num_orders, num_slabs, num_capacities - 1)
    )

    # Compute auxiliary data.
    widths = [x[0] for x in orders]
    colors = [x[1] for x in orders]
    max_capacity = max(capacities)
    loss_array = [
        min(x for x in capacities if x >= c) - c for c in range(max_capacity + 1)
    ]
    max_loss = max(loss_array)

    ### Model problem.

    # Create the model and the decision variables.
    model = cp_model.CpModel()
    assign = [
        [model.new_bool_var("assign_%i_to_slab_%i" % (o, s)) for s in all_slabs]
        for o in all_orders
    ]
    loads = [model.new_int_var(0, max_capacity, "load_%i" % s) for s in all_slabs]
    losses = [model.new_int_var(0, max_loss, "loss_%i" % s) for s in all_slabs]

    unsorted_valid_slabs = collect_valid_slabs_dp(
        capacities, colors, widths, loss_array
    )
    # Sort slab by descending load/loss. Remove duplicates.
    valid_slabs = sorted(unsorted_valid_slabs, key=lambda c: 1000 * c[-1] + c[-2])

    for s in all_slabs:
        model.add_allowed_assignments(
            [assign[o][s] for o in all_orders] + [losses[s], loads[s]], valid_slabs
        )

    # Orders are assigned to one slab.
    for o in all_orders:
        model.add_exactly_one(assign[o])

    # Redundant constraint (sum of loads == sum of widths).
    model.add(sum(loads) == sum(widths))

    # Symmetry breaking.
    for s in range(num_slabs - 1):
        model.add(loads[s] >= loads[s + 1])

    # Collect equivalent orders.
    if break_symmetries:
        print("Breaking symmetries")
        width_to_unique_color_order = {}
        ordered_equivalent_orders = []
        orders_per_color = [
            [o for o in all_orders if colors[o] == c + 1] for c in all_colors
        ]
        for c in all_colors:
            colored_orders = orders_per_color[c]
            if not colored_orders:
                continue
            if len(colored_orders) == 1:
                o = colored_orders[0]
                w = widths[o]
                if w not in width_to_unique_color_order:
                    width_to_unique_color_order[w] = [o]
                else:
                    width_to_unique_color_order[w].append(o)
            else:
                local_width_to_order = {}
                for o in colored_orders:
                    w = widths[o]
                    if w not in local_width_to_order:
                        local_width_to_order[w] = []
                        local_width_to_order[w].append(o)
                for _, os in local_width_to_order.items():
                    if len(os) > 1:
                        for p in range(len(os) - 1):
                            ordered_equivalent_orders.append((os[p], os[p + 1]))
        for _, os in width_to_unique_color_order.items():
            if len(os) > 1:
                for p in range(len(os) - 1):
                    ordered_equivalent_orders.append((os[p], os[p + 1]))

        # Create position variables if there are symmetries to be broken.
        if ordered_equivalent_orders:
            print(
                "  - creating %i symmetry breaking constraints"
                % len(ordered_equivalent_orders)
            )
            positions = {}
            for p in ordered_equivalent_orders:
                if p[0] not in positions:
                    positions[p[0]] = model.new_int_var(
                        0, num_slabs - 1, "position_of_slab_%i" % p[0]
                    )
                    model.add_map_domain(positions[p[0]], assign[p[0]])
                if p[1] not in positions:
                    positions[p[1]] = model.new_int_var(
                        0, num_slabs - 1, "position_of_slab_%i" % p[1]
                    )
                    model.add_map_domain(positions[p[1]], assign[p[1]])
                    # Finally add the symmetry breaking constraint.
                model.add(positions[p[0]] <= positions[p[1]])

    # Objective.
    model.minimize(sum(losses))

    print("Model created")

    ### Solve model.
    solver = cp_model.CpSolver()
    if _PARAMS.value:
        text_format.Parse(_PARAMS.value, solver.parameters)

    solution_printer = SteelMillSlabSolutionPrinter(orders, assign, loads, losses)
    status = solver.solve(model, solution_printer)

    ### Output the solution.
    if status == cp_model.OPTIMAL:
        print(
            "Loss = %i, time = %.2f s, %i conflicts"
            % (solver.objective_value, solver.wall_time, solver.num_conflicts)
        )
    else:
        print("No solution")


def steel_mill_slab_with_column_generation(problem):
    """Solves the Steel Mill Slab Problem."""
    ### Load problem.
    (num_slabs, capacities, _, orders) = build_problem(problem)

    num_orders = len(orders)
    num_capacities = len(capacities)
    all_orders = range(len(orders))
    print(
        "Solving steel mill with %i orders, %i slabs, and %i capacities"
        % (num_orders, num_slabs, num_capacities - 1)
    )

    # Compute auxiliary data.
    widths = [x[0] for x in orders]
    colors = [x[1] for x in orders]
    max_capacity = max(capacities)
    loss_array = [
        min(x for x in capacities if x >= c) - c for c in range(max_capacity + 1)
    ]

    ### Model problem.

    # Generate all valid slabs (columns)
    unsorted_valid_slabs = collect_valid_slabs_dp(
        capacities, colors, widths, loss_array
    )

    # Sort slab by descending load/loss. Remove duplicates.
    valid_slabs = sorted(unsorted_valid_slabs, key=lambda c: 1000 * c[-1] + c[-2])
    all_valid_slabs = range(len(valid_slabs))

    # create model and decision variables.
    model = cp_model.CpModel()
    selected = [model.new_bool_var("selected_%i" % i) for i in all_valid_slabs]

    for order_id in all_orders:
        model.add(
            sum(selected[i] for i, slab in enumerate(valid_slabs) if slab[order_id])
            == 1
        )

    # Redundant constraint (sum of loads == sum of widths).
    model.add(
        sum(selected[i] * valid_slabs[i][-1] for i in all_valid_slabs) == sum(widths)
    )

    # Objective.
    model.minimize(sum(selected[i] * valid_slabs[i][-2] for i in all_valid_slabs))

    print("Model created")

    ### Solve model.
    solver = cp_model.CpSolver()
    if _PARAMS.value:
        text_format.Parse(_PARAMS.value, solver.parameters)
    solution_printer = cp_model.ObjectiveSolutionPrinter()
    status = solver.solve(model, solution_printer)

    ### Output the solution.
    if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        print(
            "Loss = %i, time = %.2f s, %i conflicts"
            % (solver.objective_value, solver.wall_time, solver.num_conflicts)
        )
    else:
        print("No solution")


def main(_):
    if _SOLVER.value == "sat":
        steel_mill_slab(_PROBLEM.value, _BREAK_SYMMETRIES.value)
    elif _SOLVER.value == "sat_table":
        steel_mill_slab_with_valid_slabs(_PROBLEM.value, _BREAK_SYMMETRIES.value)
    elif _SOLVER.value == "sat_column":
        steel_mill_slab_with_column_generation(_PROBLEM.value)
    else:
        print(f"Unknown model {_SOLVER.value}")


if __name__ == "__main__":
    app.run(main)
