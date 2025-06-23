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

"""Code sample that encodes the product of two Boolean variables."""


from ortools.sat.python import cp_model


def boolean_product_sample_sat():
  """Encoding of the product of two Boolean variables.

  p == x * y, which is the same as p <=> x and y
  """
  model = cp_model.CpModel()
  x = model.new_bool_var("x")
  y = model.new_bool_var("y")
  p = model.new_bool_var("p")

  # x and y implies p, rewrite as not(x and y) or p.
  model.add_bool_or(~x, ~y, p)

  # p implies x and y, expanded into two implications.
  model.add_implication(p, x)
  model.add_implication(p, y)

  # Create a solver and solve.
  solver = cp_model.CpSolver()
  solution_printer = cp_model.VarArraySolutionPrinter([x, y, p])
  solver.parameters.enumerate_all_solutions = True
  solver.solve(model, solution_printer)


boolean_product_sample_sat()
