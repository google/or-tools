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

"""Solve a simple bin packing problem using a MIP solver."""
# [START program]
# [START import]
import io

import pandas as pd

from ortools.linear_solver.python import model_builder

# [END import]


# [START program_part1]
# [START data_model]
def create_data_model() -> tuple[pd.DataFrame, pd.DataFrame]:
  """Create the data for the example."""

  items_str = """
  item  weight
    i1      48
    i2      30
    i3      19
    i4      36
    i5      36
    i6      27
    i7      42
    i8      42
    i9      36
   i10      24
   i11      30
  """

  bins_str = """
  bin  capacity
   b1       100
   b2       100
   b3       100
   b4       100
   b5       100
   b6       100
   b7       100
  """

  items = pd.read_table(io.StringIO(items_str), index_col=0, sep=r"\s+")
  bins = pd.read_table(io.StringIO(bins_str), index_col=0, sep=r"\s+")
  return items, bins
  # [END data_model]


def main():
  # [START data]
  items, bins = create_data_model()
  # [END data]
  # [END program_part1]

  # [START model]
  # Create the model.
  model = model_builder.Model()
  # [END model]

  # [START program_part2]
  # [START variables]
  # Variables
  # x[i, j] = 1 if item i is packed in bin j.
  items_x_bins = pd.MultiIndex.from_product(
      [items.index, bins.index], names=["item", "bin"]
  )
  x = model.new_bool_var_series(name="x", index=items_x_bins)

  # y[j] = 1 if bin j is used.
  y = model.new_bool_var_series(name="y", index=bins.index)
  # [END variables]

  # [START constraints]
  # Constraints
  # Each item must be in exactly one bin.
  for unused_name, all_copies in x.groupby("item"):
    model.add(x[all_copies.index].sum() == 1)

  # The amount packed in each bin cannot exceed its capacity.
  for selected_bin in bins.index:
    items_in_bin = x.xs(selected_bin, level="bin")
    model.add(
        items_in_bin.dot(items.weight)
        <= bins.loc[selected_bin].capacity * y[selected_bin]
    )
  # [END constraints]

  # [START objective]
  # Objective: minimize the number of bins used.
  model.minimize(y.sum())
  # [END objective]

  # [START solve]
  # Create the solver with the CP-SAT backend, and solve the model.
  solver = model_builder.Solver("sat")
  if not solver.solver_is_supported():
    return
  status = solver.solve(model)
  # [END solve]

  # [START print_solution]
  if status == model_builder.SolveStatus.OPTIMAL:
    print(f"Number of bins used = {solver.objective_value}")

    x_values = solver.values(x)
    y_values = solver.values(y)
    active_bins = y_values.loc[lambda x: x == 1].index

    for b in active_bins:
      print(f"Bin {b}")
      items_in_bin = x_values.xs(b, level="bin").loc[lambda x: x == 1].index
      for item in items_in_bin:
        print(f"  Item {item} - weight {items.loc[item].weight}")
      print(
          f"  Packed items weight: {items.loc[items_in_bin].sum().to_string()}"
      )
      print()

    print(f"Total packed weight: {items.weight.sum()}")
    print()
    print(f"Time = {solver.wall_time} seconds")
  else:
    print("The problem does not have an optimal solution.")
  # [END print_solution]


if __name__ == "__main__":
  main()
# [END program_part2]
# [END program]
