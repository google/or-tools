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

"""Implements a step function."""

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


def step_function_sample_sat():
  """Encode the step function."""

  # Model.
  model = cp_model.CpModel()

  # Declare our primary variable.
  x = model.new_int_var(0, 20, "x")

  # Create the expression variable and implement the step function
  # Note it is not defined for x == 2.
  #
  #        -               3
  # -- --      ---------   2
  #                        1
  #      -- ---            0
  # 0 ================ 20
  #
  expr = model.new_int_var(0, 3, "expr")

  # expr == 0 on [5, 6] U [8, 10]
  b0 = model.new_bool_var("b0")
  model.add_linear_expression_in_domain(
      x, cp_model.Domain.from_intervals([(5, 6), (8, 10)])
  ).only_enforce_if(b0)
  model.add(expr == 0).only_enforce_if(b0)

  # expr == 2 on [0, 1] U [3, 4] U [11, 20]
  b2 = model.new_bool_var("b2")
  model.add_linear_expression_in_domain(
      x, cp_model.Domain.from_intervals([(0, 1), (3, 4), (11, 20)])
  ).only_enforce_if(b2)
  model.add(expr == 2).only_enforce_if(b2)

  # expr == 3 when x == 7
  b3 = model.new_bool_var("b3")
  model.add(x == 7).only_enforce_if(b3)
  model.add(expr == 3).only_enforce_if(b3)

  # At least one bi is true. (we could use an exactly one constraint).
  model.add_bool_or(b0, b2, b3)

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


step_function_sample_sat()
