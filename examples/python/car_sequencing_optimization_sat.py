#!/usr/bin/env python3
# Copyright 2010-2025 Google LLC
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

"""Solve the car sequencing problem as an optimization problem.

Problem Description: The Car Sequencing Problem with Optimization
-----------------------------------------------------------------

See https://en.wikipedia.org/wiki/Car_sequencing_problem for more details.

We are tasked with determining the optimal production sequence for a set of cars
on an assembly line. This is a classic and challenging combinatorial
optimization problem with the following characteristics:

Fixed Production Demand: There is a specific, non-negotiable number of cars of
different types (or 'classes') that must be produced. In our case, we have 6
distinct classes of cars, and we must produce exactly 5 of each, for a total of
30 'real' cars.

Diverse Car Configurations: Each car class is defined by a unique combination of
optional features. For example, 'Class 1' might require a sunroof (Option 1) and
a special engine (Option 4), while 'Class 3' only requires air conditioning
(Option 2).

Specialized Assembly Stations: The assembly line is composed of a series of
specialized stations. Each station is responsible for installing one specific
option. For example, there is one station for sunroofs, one for special engines,
and so on.

Capacity-Limited Stations: The core challenge of the problem lies here. The
stations cannot handle an unlimited, dense flow of cars requiring their specific
option. Their capacity is defined by a 'sliding window' constraint. For example,
the sunroof station might have a constraint of 'at most 1 car with a sunroof in
any sequence of 3 consecutive cars'. This means sequences like [Sunroof, No, No,
Sunroof] are valid, but [Sunroof, No, Sunroof, No] are not.

The Need for Spacing (Optimization): The combination of high demand for certain
options and tight capacity constraints may make it impossible to produce the 30
real cars consecutively. To create a valid sequence, we may need to insert
'dummy' or 'filler' cars into the production line. These dummy cars have no
options and therefore do not consume capacity at any station. They serve purely
as spacers to break up dense sequences of option-heavy cars.

The Goal: The objective is to find a production sequence that fulfills the
demand for all 30 real cars while using the minimum number of dummy cars. This
is equivalent to finding the shortest possible total production schedule (real
cars + dummy cars).

Modeling and Solution Approach with CP-SAT
------------------------------------------

To solve this problem, we use the CP-SAT solver from Google's OR-Tools library.
This is a constraint programming approach, which works by defining variables,
constraints, and an objective function.

1. Decision Variables
The fundamental decision the solver must make is: 'Which class of car should be
placed in each production slot?'
We define a large number of boolean variables: produces[c][s]. This variable is
True if a car of class c is scheduled in slot s, and False otherwise. We create
these for all car classes (including the dummy class) and for an extended number
of slots (30 real + a buffer of 20 for dummies).
We introduce a key integer variable: makespan. This variable represents the
total length of the 'meaningful' part of our schedule. It's the slot number
where the first dummy car appears, after which all subsequent cars are also
dummies.

2. Constraints (The Rules of the Game)
We translate the problem's rules into mathematical constraints that the solver
must obey:

One Car Per Slot: For every production slot s, exactly one car class can be
assigned. We enforce this using an AddExactlyOne constraint over all
produces[c][s] variables for that slot.

Fulfill Real Car Demand: The total number of times each real car class c appears
across all slots must equal its required demand (5 in our case). This is a
simple Add(sum(...) == 5) constraint.

Station Capacity (Sliding Window): This is the most critical constraint. For
each option (e.g., 'sunroof') and its capacity rule (e.g., '1 in 3'), we create
constraints for every possible sliding window. For every subsequence of 3 slots,
we sum up the produces variables corresponding to car classes that require that
option and constrain this sum to be less than or equal to 1.

Makespan Definition: This is the clever part of the model. We link our makespan
objective variable to the placement of dummy cars using logical equivalences for
each slot s:
(makespan <= s) is equivalent to (slot s contains a dummy car)
This ensures that if the solver chooses a makespan of 32, for example, it is
forced to place dummy cars in slots 32, 33, 34, and so on. Conversely, if the
solver is forced to place a dummy car in slot 32 to satisfy a capacity
constraint, the makespan must be at most 32.

3. The Objective Function

The objective is simple and directly tied to our goal:

Minimize makespan: By instructing the solver to find a solution with the
smallest possible value for the makespan variable, we are asking it to find the
shortest possible production schedule that satisfies all the rules. This
inherently minimizes the number of dummy cars used.

By defining the problem in this way, we let the CP-SAT solver explore the vast
search space of possible sequences efficiently, using its powerful constraint
propagation and search techniques to find an optimal arrangement that meets all
our complex requirements.
"""

from collections.abc import Sequence

from absl import app

from ortools.sat.python import cp_model


def solve_car_sequencing_optimization() -> None:
    """Solves the car sequencing problem with an optimization approach."""

    # --------------------
    # 1. Data
    # --------------------
    num_real_cars: int = 30
    max_dummy_cars: int = 20
    num_slots = num_real_cars + max_dummy_cars
    all_slots = range(num_slots)

    class_options = [
        # Options: 1  2  3  4  5
        [0, 0, 0, 0, 0],  # Class 0 (Dummy)
        [1, 0, 0, 1, 0],  # Class 1
        [0, 1, 0, 0, 1],  # Class 2
        [0, 1, 0, 0, 0],  # Class 3
        [0, 0, 1, 1, 0],  # Class 4
        [0, 0, 1, 0, 0],  # Class 5
        [0, 0, 0, 0, 1],  # Class 6
    ]
    num_classes = len(class_options)
    all_classes = range(num_classes)
    real_classes = range(1, num_classes)
    dummy_class = 0

    demands = [5, 5, 5, 5, 5, 5]

    capacity_constraints = [(1, 3), (1, 2), (1, 3), (2, 5), (1, 5)]
    num_options = len(capacity_constraints)
    all_options = range(num_options)

    classes_with_option = [
        [c for c in real_classes if class_options[c][o] == 1] for o in all_options
    ]

    # --------------------
    # 2. Model Creation
    # --------------------
    model = cp_model.CpModel()

    # --------------------
    # 3. Decision Variables
    # --------------------
    produces = {}
    for c in all_classes:
        for s in all_slots:
            produces[(c, s)] = model.new_bool_var(f"produces_c{c}_s{s}")

    makespan = model.new_int_var(num_real_cars, num_slots, "makespan")

    # --------------------
    # 4. Constraints
    # --------------------

    # Constraint 1: Only one car produced per slot.
    for s in all_slots:
        model.add_exactly_one([produces[(c, s)] for c in all_classes])

    # Constraint 2: Meet the demand of real cars.
    for i, c in enumerate(real_classes):
        model.add(sum(produces[(c, s)] for s in all_slots) == demands[i])

    # Constraint 3: Enforce the capacity constraints on options.
    for o in all_options:
        max_cars, subsequence_len = capacity_constraints[o]
        for start in range(num_slots - subsequence_len + 1):
            window = range(start, start + subsequence_len)
            cars_with_option_in_window = []
            for c in classes_with_option[o]:
                for s in window:
                    cars_with_option_in_window.append(produces[(c, s)])
            model.add(sum(cars_with_option_in_window) <= max_cars)

    # Constraint 4 (Link objective and dummy cars at the end of the schedule)
    for s in all_slots:
        makespan_le_s = model.new_bool_var(f"makespan_le_{s}")

        # Enforce makespan_le_s <=> (makespan <= s)
        model.add(makespan <= s).only_enforce_if(makespan_le_s)
        # Use ~ for negation
        model.add(makespan > s).only_enforce_if(~makespan_le_s)

        # Enforce makespan_le_s => produces[dummy_class, s]
        model.add_implication(makespan_le_s, produces[dummy_class, s])

    # --------------------
    # 5. Objective
    # --------------------
    model.minimize(makespan)

    # --------------------
    # 6. Solve and Print Solution
    # --------------------
    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = 30.0
    solver.parameters.num_search_workers = 1  # The problem is easy to solve.
    # solver.parameters.log_search_progress = True  # uncomment to see the log.

    status = solver.Solve(model)

    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        final_makespan = int(solver.ObjectiveValue())
        num_dummies_needed = final_makespan - num_real_cars

        print(
            f'\n{"Optimal" if status == cp_model.OPTIMAL else "Feasible"}'
            f" solution found with a makespan of {final_makespan}."
        )
        print(
            f"This requires the conceptual equivalent of {num_dummies_needed} dummy"
            " car(s) to be used as spacers."
        )

        sequence = [-1] * num_slots
        for s in all_slots:
            for c in all_classes:
                if solver.Value(produces[(c, s)]) == 1:
                    sequence[s] = c
                    break

        print("\nFull Production Sequence (Class 0 is dummy):")
        print("Slot:  | " + " | ".join(f"{i:2}" for i in range(num_slots)) + " |")
        print("-------|-" + "--|-" * num_slots)
        print("Class: | " + " | ".join(f"{c:2}" for c in sequence) + " |")

    elif status == cp_model.INFEASIBLE:
        print("\nNo solution found.")

    else:
        print(f"\nSomething went wrong. Solver status: {status}")

    print("\nSolver statistics:")
    print(solver.response_stats())


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")
    solve_car_sequencing_optimization()


if __name__ == "__main__":
    app.run(main)
