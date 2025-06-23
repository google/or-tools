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

"""Encodes a convex piecewise linear function."""


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


def earliness_tardiness_cost_sample_sat():
  """Encode the piecewise linear expression."""

  earliness_date = 5  # ed.
  earliness_cost = 8
  lateness_date = 15  # ld.
  lateness_cost = 12

  # Model.
  model = cp_model.CpModel()

  # Declare our primary variable.
  x = model.new_int_var(0, 20, "x")

  # Create the expression variable and implement the piecewise linear function.
  #
  #  \        /
  #   \______/
  #   ed    ld
  #
  large_constant = 1000
  expr = model.new_int_var(0, large_constant, "expr")

  # First segment.
  s1 = model.new_int_var(-large_constant, large_constant, "s1")
  model.add(s1 == earliness_cost * (earliness_date - x))

  # Second segment.
  s2 = 0

  # Third segment.
  s3 = model.new_int_var(-large_constant, large_constant, "s3")
  model.add(s3 == lateness_cost * (x - lateness_date))

  # Link together expr and x through s1, s2, and s3.
  model.add_max_equality(expr, [s1, s2, s3])

  # Search for x values in increasing order.
  model.add_decision_strategy(
      [x], cp_model.CHOOSE_FIRST, cp_model.SELECT_MIN_VALUE
  )

  # Create a solver and solve with a fixed search.
  solver = cp_model.CpSolver()

  # Force the solver to follow the decision strategy exactly.
  solver.parameters.search_branching = cp_model.FIXED_SEARCH
  # Enumerate all solutions.
  solver.parameters.enumerate_all_solutions = True

  # Search and print out all solutions.
  solution_printer = VarArraySolutionPrinter([x, expr])
  solver.solve(model, solution_printer)


earliness_tardiness_cost_sample_sat()
