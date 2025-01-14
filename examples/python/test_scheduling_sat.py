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

"""Solves a test scheduling problem.

Tests must be run by an operator. Tests have a duration and a power consumption.

Operators draw power from power supplies. The mapping between operators and
power supplies is given.

Power supplies have a maximum power they can deliver.

Can we schedule the tests so that the power consumption of each power supply is
always below its maximum power, and the total makespan is minimized?
"""

from collections.abc import Sequence
import io
from typing import Dict, Tuple

from absl import app
from absl import flags
import pandas as pd

from google.protobuf import text_format
from ortools.sat.python import cp_model


_PARAMS = flags.DEFINE_string(
    "params",
    "num_search_workers:16,log_search_progress:true,max_time_in_seconds:45",
    "Sat solver parameters.",
)


def build_data() -> tuple[pd.DataFrame, pd.DataFrame, pd.DataFrame]:
    """Build the data frame."""
    tests_str = """
  Name Operator    TestTime    AveragePower
   T1     O1           300            200
   T2     O1           150             40
   T3     O2           100             65
   T4     O2           250            150
   T5     O3           210            140
  """

    operators_str = """
  Operator Supply
      O1      S1
      O2      S2
      O3      S2
  """

    supplies_str = """
  Supply  MaxAllowedPower
   S1        230
   S2        210
  """

    tests_data = pd.read_table(io.StringIO(tests_str), sep=r"\s+")
    operators_data = pd.read_table(io.StringIO(operators_str), sep=r"\s+")
    supplies_data = pd.read_table(io.StringIO(supplies_str), sep=r"\s+")

    return (tests_data, operators_data, supplies_data)


def solve(
    tests_data: pd.DataFrame,
    operator_data: pd.DataFrame,
    supplies_data: pd.DataFrame,
) -> None:
    """Solve the scheduling of tests problem."""

    # Parses data.
    operator_to_supply: Dict[str, str] = {}
    for _, row in operator_data.iterrows():
        operator_to_supply[row["Operator"]] = row["Supply"]

    supply_to_max_power: Dict[str, int] = {}
    for _, row in supplies_data.iterrows():
        supply_to_max_power[row["Supply"]] = row["MaxAllowedPower"]

    horizon = tests_data["TestTime"].sum()

    # OR-Tools model.
    model = cp_model.CpModel()

    # Create containers.
    tests_per_supply: Dict[str, Tuple[list[cp_model.IntervalVar], list[int]]] = {}
    test_supply: Dict[str, str] = {}
    test_starts: Dict[str, cp_model.IntVar] = {}
    test_durations: Dict[str, int] = {}
    test_powers: Dict[str, int] = {}
    all_ends = []

    # Creates intervals.
    for _, row in tests_data.iterrows():
        name: str = row["Name"]
        operator: str = row["Operator"]
        test_time: int = row["TestTime"]
        average_power: int = row["AveragePower"]
        supply: str = operator_to_supply[operator]

        start = model.new_int_var(0, horizon - test_time, f"start_{name}")
        interval = model.new_fixed_size_interval_var(
            start, test_time, f"interval_{name}"
        )

        # Bookkeeping.
        test_starts[name] = start
        test_durations[name] = test_time
        test_powers[name] = average_power
        test_supply[name] = supply
        if supply not in tests_per_supply.keys():
            tests_per_supply[supply] = ([], [])
        tests_per_supply[supply][0].append(interval)
        tests_per_supply[supply][1].append(average_power)
        all_ends.append(start + test_time)

    # Create supply cumulative constraints.
    for supply, (intervals, demands) in tests_per_supply.items():
        model.add_cumulative(intervals, demands, supply_to_max_power[supply])

    # Objective.
    makespan = model.new_int_var(0, horizon, "makespan")
    for end in all_ends:
        model.add(makespan >= end)
    model.minimize(makespan)

    # Solve model.
    solver = cp_model.CpSolver()
    if _PARAMS.value:
        text_format.Parse(_PARAMS.value, solver.parameters)
    status = solver.solve(model)

    # Report solution.
    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        print(f"Makespan = {solver.value(makespan)}")
        for name, start in test_starts.items():
            print(
                f"{name}: start:{solver.value(start)} duration:{test_durations[name]}"
                f" power:{test_powers[name]} on supply {test_supply[name]}"
            )


def main(argv: Sequence[str]) -> None:
    """Builds the data and solve the scheduling problem."""
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")

    tests_data, operators_data, supplies_data = build_data()
    print("Tests data")
    print(tests_data)
    print()
    print("Operators data")
    print(operators_data)
    print()
    print("Supplies data")
    print(supplies_data)

    solve(tests_data, operators_data, supplies_data)


if __name__ == "__main__":
    app.run(main)
