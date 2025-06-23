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

"""Code sample that solves a model and gets the infeasibility assumptions."""
# [START program]
# [START import]
from ortools.sat.python import cp_model

# [END import]


def main() -> None:
  """Showcases assumptions."""
  # Creates the model.
  # [START model]
  model = cp_model.CpModel()
  # [END model]

  # Creates the variables.
  # [START variables]
  x = model.new_int_var(0, 10, "x")
  y = model.new_int_var(0, 10, "y")
  z = model.new_int_var(0, 10, "z")
  a = model.new_bool_var("a")
  b = model.new_bool_var("b")
  c = model.new_bool_var("c")
  # [END variables]

  # Creates the constraints.
  # [START constraints]
  model.add(x > y).only_enforce_if(a)
  model.add(y > z).only_enforce_if(b)
  model.add(z > x).only_enforce_if(c)
  # [END constraints]

  # Add assumptions
  model.add_assumptions([a, b, c])

  # Creates a solver and solves.
  # [START solve]
  solver = cp_model.CpSolver()
  status = solver.solve(model)
  # [END solve]

  # Print solution.
  # [START print_solution]
  print(f"Status = {solver.status_name(status)}")
  if status == cp_model.INFEASIBLE:
    print(
        "sufficient_assumptions_for_infeasibility = "
        f"{solver.sufficient_assumptions_for_infeasibility()}"
    )
  # [END print_solution]


if __name__ == "__main__":
  main()
# [END program]
