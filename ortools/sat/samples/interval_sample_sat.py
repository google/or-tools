#!/usr/bin/env python3
# Copyright 2010-2021 Google LLC
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
"""Code sample to demonstrates how to build an interval."""

from ortools.sat.python import cp_model


def IntervalSampleSat():
    """Showcases how to build interval variables."""
    model = cp_model.CpModel()
    horizon = 100

    # An interval can be created from three affine expressions.
    start_var = model.NewIntVar(0, horizon, 'start')
    duration = 10  # Python cp/sat code accept integer variables or constants.
    end_var = model.NewIntVar(0, horizon, 'end')
    interval_var = model.NewIntervalVar(start_var, duration, end_var + 2,
                                        'interval')

    print(f'interval = {repr(interval_var)}')

    # If the size is fixed, a simpler version uses the start expression and the
    # size.
    fixed_size_interval_var = model.NewFixedSizeIntervalVar(
        start_var, 10, 'fixed_size_interval_var')
    print(f'fixed_size_interval_var = {repr(fixed_size_interval_var)}')

    # A fixed interval can be created using the same API.
    fixed_interval = model.NewFixedSizeIntervalVar(5, 10, 'fixed_interval')
    print(f'fixed_interval = {repr(fixed_interval)}')


IntervalSampleSat()
