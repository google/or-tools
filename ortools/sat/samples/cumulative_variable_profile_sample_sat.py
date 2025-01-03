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

"""Solves a scheduling problem with a min and max profile for the work load."""

import io

from absl import app
import pandas as pd

from ortools.sat.python import cp_model


def create_data_model() -> tuple[pd.DataFrame, pd.DataFrame, pd.DataFrame]:
    """Creates the dataframes that describes the model."""

    max_load_str: str = """
  start_hour  max_load
     0            0
     2            0
     4            3
     6            6
     8            8
    10           12
    12            8
    14           12
    16           10
    18            6
    20            4
    22            0
  """

    min_load_str: str = """
  start_hour  min_load
     0            0
     2            0
     4            0
     6            0
     8            3
    10            3
    12            1
    14            3
    16            3
    18            1
    20            1
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

    max_load_df = pd.read_table(io.StringIO(max_load_str), sep=r"\s+")
    min_load_df = pd.read_table(io.StringIO(min_load_str), sep=r"\s+")
    tasks_df = pd.read_table(io.StringIO(tasks_str), index_col=0, sep=r"\s+")
    return max_load_df, min_load_df, tasks_df
    # [END data_model]


def check_solution(
    tasks: list[tuple[int, int, int]],
    min_load_df: pd.DataFrame,
    max_load_df: pd.DataFrame,
    period_length: int,
    horizon: int,
) -> bool:
    """Checks the solution validity against the min and max load constraints."""
    minutes_per_hour = 60
    actual_load_profile = [0 for _ in range(horizon)]
    min_load_profile = [0 for _ in range(horizon)]
    max_load_profile = [0 for _ in range(horizon)]

    # The complexity of the checker is linear in the number of time points, and
    # should be improved.
    for task in tasks:
        for t in range(task[1]):
            actual_load_profile[task[0] + t] += task[2]
    for row in max_load_df.itertuples():
        for t in range(period_length):
            max_load_profile[row.start_hour * minutes_per_hour + t] = row.max_load
    for row in min_load_df.itertuples():
        for t in range(period_length):
            min_load_profile[row.start_hour * minutes_per_hour + t] = row.min_load

    for time in range(horizon):
        if actual_load_profile[time] > max_load_profile[time]:
            print(
                f"actual load {actual_load_profile[time]} at time {time} is greater"
                f" than max load {max_load_profile[time]}"
            )
            return False
        if actual_load_profile[time] < min_load_profile[time]:
            print(
                f"actual load {actual_load_profile[time]} at time {time} is"
                f" less than min load {min_load_profile[time]}"
            )
            return False
    return True


def main(_) -> None:
    """Create the model and solves it."""
    max_load_df, min_load_df, tasks_df = create_data_model()

    # Create the model.
    model = cp_model.CpModel()

    # Get the max capacity from the capacity dataframe.
    max_load = max_load_df.max_load.max()
    print(f"Max capacity = {max_load}")
    print(f"#tasks = {len(tasks_df)}")

    minutes_per_hour: int = 60
    horizon: int = 24 * 60

    # Variables
    starts = model.new_int_var_series(
        name="starts",
        lower_bounds=0,
        upper_bounds=horizon - tasks_df.duration,
        index=tasks_df.index,
    )
    performed = model.new_bool_var_series(name="performed", index=tasks_df.index)

    intervals = model.new_optional_fixed_size_interval_var_series(
        name="intervals",
        index=tasks_df.index,
        starts=starts,
        sizes=tasks_df.duration,
        are_present=performed,
    )

    # Set up the max profile. We use fixed (intervals, demands) to fill in the
    # space between the actual max load profile and the max capacity.
    time_period_max_intervals = model.new_fixed_size_interval_var_series(
        name="time_period_max_intervals",
        index=max_load_df.index,
        starts=max_load_df.start_hour * minutes_per_hour,
        sizes=minutes_per_hour * 2,
    )
    time_period_max_heights = max_load - max_load_df.max_load

    # Cumulative constraint for the max profile.
    model.add_cumulative(
        intervals.to_list() + time_period_max_intervals.to_list(),
        tasks_df.load.to_list() + time_period_max_heights.to_list(),
        max_load,
    )

    # Set up complemented intervals (from 0 to start, and from start + size to
    # horizon).
    prefix_intervals = model.new_optional_interval_var_series(
        name="prefix_intervals",
        index=tasks_df.index,
        starts=0,
        sizes=starts,
        ends=starts,
        are_present=performed,
    )

    suffix_intervals = model.new_optional_interval_var_series(
        name="suffix_intervals",
        index=tasks_df.index,
        starts=starts + tasks_df.duration,
        sizes=horizon - starts - tasks_df.duration,
        ends=horizon,
        are_present=performed,
    )

    # Set up the min profile. We use complemented intervals to maintain the
    # complement of the work load, and fixed intervals to enforce the min
    # number of active workers per time period.
    #
    # Note that this works only if the max load cumulative is also added to the
    # model.
    time_period_min_intervals = model.new_fixed_size_interval_var_series(
        name="time_period_min_intervals",
        index=min_load_df.index,
        starts=min_load_df.start_hour * minutes_per_hour,
        sizes=minutes_per_hour * 2,
    )
    time_period_min_heights = min_load_df.min_load

    # We take into account optional intervals. The actual capacity of the min load
    # cumulative is the sum of all the active demands.
    sum_of_demands = sum(tasks_df.load)
    complement_capacity = model.new_int_var(0, sum_of_demands, "complement_capacity")
    model.add(complement_capacity == performed.dot(tasks_df.load))

    # Cumulative constraint for the min profile.
    model.add_cumulative(
        prefix_intervals.to_list()
        + suffix_intervals.to_list()
        + time_period_min_intervals.to_list(),
        tasks_df.load.to_list()
        + tasks_df.load.to_list()
        + time_period_min_heights.to_list(),
        complement_capacity,
    )

    # Objective: maximize the value of performed intervals.
    # 1 is the max priority.
    max_priority = max(tasks_df.priority)
    model.maximize(sum(performed * (max_priority + 1 - tasks_df.priority)))

    # Create the solver and solve the model.
    solver = cp_model.CpSolver()
    # solver.parameters.log_search_progress = True  # Uncomment to see the logs.
    solver.parameters.num_workers = 16
    solver.parameters.max_time_in_seconds = 30.0
    status = solver.solve(model)

    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        start_values = solver.values(starts)
        performed_values = solver.boolean_values(performed)
        tasks: list[tuple[int, int, int]] = []
        for task in tasks_df.index:
            if performed_values[task]:
                print(
                    f'task {task} duration={tasks_df["duration"][task]} '
                    f'load={tasks_df["load"][task]} starts at {start_values[task]}'
                )
                tasks.append(
                    (start_values[task], tasks_df.duration[task], tasks_df.load[task])
                )
            else:
                print(f"task {task} is not performed")
        assert check_solution(
            tasks=tasks,
            min_load_df=min_load_df,
            max_load_df=max_load_df,
            period_length=2 * minutes_per_hour,
            horizon=horizon,
        )
    elif status == cp_model.INFEASIBLE:
        print("No solution found")
    else:
        print("Something is wrong, check the status and the log of the solve")


if __name__ == "__main__":
    app.run(main)
