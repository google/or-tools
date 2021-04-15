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
"""Code sample to demonstrates how to build an optional interval."""

from ortools.sat.python import cp_model


def OptionalIntervalSampleSat():
    """Build an optional interval."""
    model = cp_model.CpModel()

    horizon = 100
    start_var = model.NewIntVar(0, horizon, 'start')
    duration = 10  # Python cp/sat code accept integer variables or constants.
    end_var = model.NewIntVar(0, horizon, 'end')
    presence_var = model.NewBoolVar('presence')
    interval_var = model.NewOptionalIntervalVar(start_var, duration, end_var,
                                                presence_var, 'interval')

    print('start = %s, duration = %i, end = %s, presence = %s, interval = %s' %
          (start_var, duration, end_var, presence_var, interval_var))


OptionalIntervalSampleSat()
