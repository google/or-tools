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

"""Code sample to demonstrates how to detect if two intervals overlap."""

from ortools.sat.python import cp_model


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, variables: list[cp_model.IntVar]):
    cp_model.CpSolverSolutionCallback.__init__(self)
    self.__variables = variables

  def on_solution_callback(self) -> None:
    for v in self.__variables:
      print(f"{v}={self.value(v)}", end=" ")
    print()


def overlapping_interval_sample_sat():
  """Create the overlapping Boolean variables and enumerate all states."""
  model = cp_model.CpModel()

  horizon = 7

  # First interval.
  start_var_a = model.new_int_var(0, horizon, "start_a")
  duration_a = 3
  end_var_a = model.new_int_var(0, horizon, "end_a")
  unused_interval_var_a = model.new_interval_var(
      start_var_a, duration_a, end_var_a, "interval_a"
  )

  # Second interval.
  start_var_b = model.new_int_var(0, horizon, "start_b")
  duration_b = 2
  end_var_b = model.new_int_var(0, horizon, "end_b")
  unused_interval_var_b = model.new_interval_var(
      start_var_b, duration_b, end_var_b, "interval_b"
  )

  # a_after_b Boolean variable.
  a_after_b = model.new_bool_var("a_after_b")
  model.add(start_var_a >= end_var_b).only_enforce_if(a_after_b)
  model.add(start_var_a < end_var_b).only_enforce_if(~a_after_b)

  # b_after_a Boolean variable.
  b_after_a = model.new_bool_var("b_after_a")
  model.add(start_var_b >= end_var_a).only_enforce_if(b_after_a)
  model.add(start_var_b < end_var_a).only_enforce_if(~b_after_a)

  # Result Boolean variable.
  a_overlaps_b = model.new_bool_var("a_overlaps_b")

  # Option a: using only clauses
  model.add_bool_or(a_after_b, b_after_a, a_overlaps_b)
  model.add_implication(a_after_b, ~a_overlaps_b)
  model.add_implication(b_after_a, ~a_overlaps_b)

  # Option b: using an exactly one constraint.
  # model.add_exactly_one(a_after_b, b_after_a, a_overlaps_b)

  # Search for start values in increasing order for the two intervals.
  model.add_decision_strategy(
      [start_var_a, start_var_b],
      cp_model.CHOOSE_FIRST,
      cp_model.SELECT_MIN_VALUE,
  )

  # Create a solver and solve with a fixed search.
  solver = cp_model.CpSolver()

  # Force the solver to follow the decision strategy exactly.
  solver.parameters.search_branching = cp_model.FIXED_SEARCH
  # Enumerate all solutions.
  solver.parameters.enumerate_all_solutions = True

  # Search and print out all solutions.
  solution_printer = VarArraySolutionPrinter(
      [start_var_a, start_var_b, a_overlaps_b]
  )
  solver.solve(model, solution_printer)


overlapping_interval_sample_sat()
