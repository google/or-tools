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

"""Simple model with a reified constraint."""

from ortools.sat.python import cp_model


def reified_sample_sat():
  """Showcase creating a reified constraint."""
  model = cp_model.CpModel()

  x = model.new_bool_var("x")
  y = model.new_bool_var("y")
  b = model.new_bool_var("b")

  # First version using a half-reified bool and.
  model.add_bool_and(x, ~y).only_enforce_if(b)

  # Second version using implications.
  model.add_implication(b, x)
  model.add_implication(b, ~y)

  # Third version using bool or.
  model.add_bool_or(~b, x)
  model.add_bool_or(~b, ~y)


reified_sample_sat()
