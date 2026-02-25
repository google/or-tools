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

"""MIP example that solves an assignment problem."""
# [START program]
# [START import]
import io

import pandas as pd

from ortools.linear_solver.python import model_builder

# [END import]


def main():
    # Data
    # [START data_model]
    data_str = """
  worker  task  cost
      w1    t1    90
      w1    t2    80
      w1    t3    75
      w1    t4    70
      w2    t1    35
      w2    t2    85
      w2    t3    55
      w2    t4    65
      w3    t1   125
      w3    t2    95
      w3    t3    90
      w3    t4    95
      w4    t1    45
      w4    t2   110
      w4    t3    95
      w4    t4   115
      w5    t1    50
      w5    t2   110
      w5    t3    90
      w5    t4   100
  """

    data = pd.read_table(io.StringIO(data_str), sep=r"\s+")
    # [END data_model]

    # Create the model.
    # [START model]
    model = model_builder.Model()
    # [END model]

    # Variables
    # [START variables]
    # x[i, j] is an array of 0-1 variables, which will be 1
    # if worker i is assigned to task j.
    x = model.new_bool_var_series(name="x", index=data.index)
    # [END variables]

    # Constraints
    # [START constraints]
    # Each worker is assigned to at most 1 task.
    for unused_name, tasks in data.groupby("worker"):
        model.add(x[tasks.index].sum() <= 1)

    # Each task is assigned to exactly one worker.
    for unused_name, workers in data.groupby("task"):
        model.add(x[workers.index].sum() == 1)
    # [END constraints]

    # Objective
    # [START objective]
    model.minimize(data.cost.dot(x))
    # [END objective]

    # [START solve]
    # Create the solver with the CP-SAT backend, and solve the model.
    solver = model_builder.Solver("sat")
    if not solver.solver_is_supported():
        return
    status = solver.solve(model)
    # [END solve]

    # Print solution.
    # [START print_solution]
    if (
        status == model_builder.SolveStatus.OPTIMAL
        or status == model_builder.SolveStatus.FEASIBLE
    ):
        print(f"Total cost = {solver.objective_value}\n")
        selected = data.loc[solver.values(x).loc[lambda x: x == 1].index]
        for unused_index, row in selected.iterrows():
            print(f"{row.task} assigned to {row.worker} with a cost of {row.cost}")
    else:
        print("No solution found.")
    # [END print_solution]


if __name__ == "__main__":
    main()
# [END program]
