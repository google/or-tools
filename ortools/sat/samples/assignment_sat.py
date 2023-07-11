#!/usr/bin/env python3
# Copyright 2010-2022 Google LLC
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

"""Solve a simple assignment problem."""
# [START program]
# [START import]
import io

import pandas as pd

from ortools.sat.python import cp_model
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

    # Model
    # [START model]
    model = cp_model.CpModel()
    # [END model]

    # Variables
    # [START variables]
    x = model.NewBoolVarSeries(name="x", index=data.index)
    # [END variables]

    # Constraints
    # [START constraints]
    # Each worker is assigned to at most one task.
    for unused_name, tasks in data.groupby("worker"):
        model.AddAtMostOne(x[tasks.index])

    # Each task is assigned to exactly one worker.
    for unused_name, workers in data.groupby("task"):
        model.AddExactlyOne(x[workers.index])
    # [END constraints]

    # Objective
    # [START objective]
    model.Minimize(data.cost.dot(x))
    # [END objective]

    # Solve
    # [START solve]
    solver = cp_model.CpSolver()
    status = solver.Solve(model)
    # [END solve]

    # Print solution.
    # [START print_solution]
    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        print(f"Total cost = {solver.ObjectiveValue()}\n")
        selected = data.loc[solver.BooleanValues(x).loc[lambda x: x].index]
        for unused_index, row in selected.iterrows():
            print(f"{row.task} assigned to {row.worker} with a cost of {row.cost}")
    else:
        print("No solution found.")
    # [END print_solution]


if __name__ == "__main__":
    main()
# [END program]
