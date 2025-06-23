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

"""Code sample that encodes the product of a Boolean and an integer variable."""

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


def build_product_var(
    model: cp_model.CpModel, b: cp_model.IntVar, x: cp_model.IntVar, name: str
) -> cp_model.IntVar:
  """Builds the product of a Boolean variable and an integer variable."""
  p = model.new_int_var_from_domain(
      cp_model.Domain.from_flat_intervals(x.proto.domain).union_with(
          cp_model.Domain(0, 0)
      ),
      name,
  )
  model.add(p == x).only_enforce_if(b)
  model.add(p == 0).only_enforce_if(~b)
  return p


def bool_and_int_var_product_sample_sat():
  """Encoding of the product of two Boolean variables.

  p == x * y, which is the same as p <=> x and y
  """
  model = cp_model.CpModel()
  b = model.new_bool_var("b")
  x = model.new_int_var_from_domain(
      cp_model.Domain.from_values([1, 2, 3, 5, 6, 7, 9, 10]), "x"
  )
  p = build_product_var(model, b, x, "p")

  # Search for x and b values in increasing order.
  model.add_decision_strategy(
      [b, x], cp_model.CHOOSE_FIRST, cp_model.SELECT_MIN_VALUE
  )

  # Create a solver and solve.
  solver = cp_model.CpSolver()
  solution_printer = VarArraySolutionPrinter([x, b, p])
  solver.parameters.enumerate_all_solutions = True
  solver.parameters.search_branching = cp_model.FIXED_SEARCH
  solver.solve(model, solution_printer)


bool_and_int_var_product_sample_sat()
