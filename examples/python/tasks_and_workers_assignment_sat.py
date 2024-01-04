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

"""Tasks and workers to group assignment to average sum(cost) / #workers."""

from typing import Sequence
from absl import app
from ortools.sat.python import cp_model


class ObjectivePrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__solution_count = 0

    def on_solution_callback(self):
        print(
            "Solution %i, time = %f s, objective = %i"
            % (self.__solution_count, self.wall_time, self.objective_value)
        )
        self.__solution_count += 1


def tasks_and_workers_assignment_sat():
    """solve the assignment problem."""
    model = cp_model.CpModel()

    # CP-SAT solver is integer only.
    task_cost = [24, 10, 7, 2, 11, 16, 1, 13, 9, 27]
    num_tasks = len(task_cost)
    num_workers = 3
    num_groups = 2
    all_workers = range(num_workers)
    all_groups = range(num_groups)
    all_tasks = range(num_tasks)

    # Variables

    ## x_ij = 1 if worker i is assigned to group j
    x = {}
    for i in all_workers:
        for j in all_groups:
            x[i, j] = model.new_bool_var("x[%i,%i]" % (i, j))

    ## y_kj is 1 if task k is assigned to group j
    y = {}
    for k in all_tasks:
        for j in all_groups:
            y[k, j] = model.new_bool_var("x[%i,%i]" % (k, j))

    # Constraints

    # Each task k is assigned to a group and only one.
    for k in all_tasks:
        model.add(sum(y[k, j] for j in all_groups) == 1)

    # Each worker i is assigned to a group and only one.
    for i in all_workers:
        model.add(sum(x[i, j] for j in all_groups) == 1)

    # Cost per group
    sum_of_costs = sum(task_cost)
    averages = []
    num_workers_in_group = []
    scaled_sum_of_costs_in_group = []
    scaling = 1000  # We introduce scaling to deal with floating point average.
    for j in all_groups:
        n = model.new_int_var(1, num_workers, "num_workers_in_group_%i" % j)
        model.add(n == sum(x[i, j] for i in all_workers))
        c = model.new_int_var(0, sum_of_costs * scaling, "sum_of_costs_of_group_%i" % j)
        model.add(c == sum(y[k, j] * task_cost[k] * scaling for k in all_tasks))
        a = model.new_int_var(0, sum_of_costs * scaling, "average_cost_of_group_%i" % j)
        model.add_division_equality(a, c, n)

        averages.append(a)
        num_workers_in_group.append(n)
        scaled_sum_of_costs_in_group.append(c)

    # All workers are assigned.
    model.add(sum(num_workers_in_group) == num_workers)

    # Objective.
    obj = model.new_int_var(0, sum_of_costs * scaling, "obj")
    model.add_max_equality(obj, averages)
    model.minimize(obj)

    # Solve and print out the solution.
    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = 60 * 60 * 2
    objective_printer = ObjectivePrinter()
    status = solver.solve(model, objective_printer)
    print(solver.response_stats())

    if status == cp_model.OPTIMAL:
        for j in all_groups:
            print("Group %i" % j)
            for i in all_workers:
                if solver.boolean_value(x[i, j]):
                    print("  - worker %i" % i)
            for k in all_tasks:
                if solver.boolean_value(y[k, j]):
                    print("  - task %i with cost %i" % (k, task_cost[k]))
            print(
                "  - sum_of_costs = %i"
                % (solver.value(scaled_sum_of_costs_in_group[j]) // scaling)
            )
            print("  - average cost = %f" % (solver.value(averages[j]) * 1.0 / scaling))


tasks_and_workers_assignment_sat()


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")
    tasks_and_workers_assignment_sat()


if __name__ == "__main__":
    app.run(main)
