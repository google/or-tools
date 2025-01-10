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

"""Code sample to demonstrates a simple Boolean constraint."""


from ortools.sat.python import cp_model


def bool_or_sample_sat():
    model = cp_model.CpModel()

    x = model.new_bool_var("x")
    y = model.new_bool_var("y")

    model.add_bool_or([x, y.negated()])
    # The [] is not mandatory.
    # ~y is equivalent to y.negated()
    model.add_bool_or(x, ~y)


bool_or_sample_sat()
