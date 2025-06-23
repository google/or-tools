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

"""Minimal solver python binary."""

from collections.abc import Sequence

from absl import app
from absl import flags

from ortools.linear_solver.python import model_builder

_INPUT = flags.DEFINE_string("input", "", "Input file to load and solve.")
_PARAMS = flags.DEFINE_string(
    "params", "", "Solver parameters in string format."
)
_SOLVER = flags.DEFINE_string(
    "solver", "sat", "Solver type to solve the model with."
)


def main(argv: Sequence[str]) -> None:
  """Load a model and solves it."""
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")

  model = model_builder.ModelBuilder()

  # Load MPS or proto file.
  if _INPUT.value.endswith(".mps"):
    if not model.import_from_mps_file(_INPUT.value):
      print(f"Cannot import MPS file: '{_INPUT.value}'")
      return
  elif not model.import_from_proto_file(_INPUT.value):
    print(f"Cannot import Proto file: '{_INPUT.value}'")
    return

  # Create solver.
  solver = model_builder.ModelSolver(_SOLVER.value)
  if not solver.solver_is_supported():
    print(f"Cannot create solver with name '{_SOLVER.value}'")
    return

  # Set parameters.
  if _PARAMS.value:
    solver.set_solver_specific_parameters(_PARAMS.value)

  # Enable the output of the solver.
  solver.enable_output(True)

  # And solve.
  solver.solve(model)


if __name__ == "__main__":
  app.run(main)
