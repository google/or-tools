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

"""Solves a simple scheduling problem with a variable work load."""

# [START program]
# [START import]
import io

import pandas as pd

from ortools.sat.python import cp_model
# [END import]


# [START program_part1]
# [START data_model]
def create_data_model() -> tuple[pd.DataFrame, pd.DataFrame]:
    """Creates the two dataframes that describes the model."""

    capacity_str: str = """
  start_hour  capacity
     0            0
     2            0
     4            1
     6            3
     8            6
    10           12
    12            8
    14           12
    16           10
    18            4
    20            2
    22            0
  """

    tasks_str: str = """
  name  duration load  priority
   t1      60      3      2
   t2     180      2      1
   t3     240      5      3
   t4      90      4      2
   t5     120      3      1
   t6     300      3      3
   t7     120      1      2
   t8     100      5      2
   t9     110      2      1
   t10    300      5      3
   t11     90      4      2
   t12    120      3      1
   t13    250      3      3
   t14    120      1      2
   t15     40      5      3
   t16     70      4      2
   t17     90      8      1
   t18     40      3      3
   t19    120      5      2
   t20     60      3      2
   t21    180      2      1
   t22    240      5      3
   t23     90      4      2
   t24    120      3      1
   t25    300      3      3
   t26    120      1      2
   t27    100      5      2
   t28    110      2      1
   t29    300      5      3
   t30     90      4      2
  """

    capacity_df = pd.read_table(io.StringIO(capacity_str), sep=r"\s+")
    tasks_df = pd.read_table(io.StringIO(tasks_str), index_col=0, sep=r"\s+")
    return capacity_df, tasks_df
    # [END data_model]


def main() -> None:
    """Create the model and solves it."""
    # [START data]
    capacity_df, tasks_df = create_data_model()
    # [END data]
    # [END program_part1]

    # [START model]
    # Create the model.
    model = cp_model.CpModel()
    # [END model]

    # Get the max capacity from the capacity dataframe.
    max_capacity = capacity_df.capacity.max()
    print(f"Max capacity = {max_capacity}")
    print(f"#tasks = {len(tasks_df)}")

    minutes_per_period: int = 120
    horizon: int = 24 * 60

    # [START program_part2]
    # [START variables]
    # Variables
    starts = model.new_int_var_series(
        name="starts", lower_bounds=0, upper_bounds=horizon, index=tasks_df.index
    )
    performed = model.new_bool_var_series(name="performed", index=tasks_df.index)

    intervals = model.new_optional_fixed_size_interval_var_series(
        name="intervals",
        index=tasks_df.index,
        starts=starts,
        sizes=tasks_df.duration,
        are_present=performed,
    )
    # [END variables]

    # [START constraints]
    # Set up the profile. We use fixed (intervals, demands) to fill in the space
    # between the actual load profile and the max capacity.
    time_period_intervals = model.new_fixed_size_interval_var_series(
        name="time_period_intervals",
        index=capacity_df.index,
        starts=capacity_df.start_hour * minutes_per_period,
        sizes=minutes_per_period,
    )
    time_period_heights = max_capacity - capacity_df.capacity

    # Cumulative constraint.
    model.add_cumulative(
        intervals.to_list() + time_period_intervals.to_list(),
        tasks_df.load.to_list() + time_period_heights.to_list(),
        max_capacity,
    )
    # [END constraints]

    # [START objective]
    # Objective: maximize the value of performed intervals.
    # 1 is the max priority.
    max_priority = max(tasks_df.priority)
    model.maximize(sum(performed * (max_priority + 1 - tasks_df.priority)))
    # [END objective]

    # [START solve]
    # Create the solver and solve the model.
    solver = cp_model.CpSolver()
    solver.parameters.log_search_progress = True
    solver.parameters.num_workers = 8
    solver.parameters.max_time_in_seconds = 30.0
    status = solver.solve(model)
    # [END solve]

    # [START print_solution]
    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        start_values = solver.values(starts)
        performed_values = solver.boolean_values(performed)
        for task in tasks_df.index:
            if performed_values[task]:
                print(f"task {task} starts at {start_values[task]}")
            else:
                print(f"task {task} is not performed")
    elif status == cp_model.INFEASIBLE:
        print("No solution found")
    else:
        print("Something is wrong, check the status and the log of the solve")
    # [END print_solution]


if __name__ == "__main__":
    main()
# [END program_part2]
# [END program]
