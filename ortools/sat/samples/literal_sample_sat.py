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
"""Code sample to demonstrate Boolean variable and literals."""

from ortools.sat.python import cp_model


def LiteralSampleSat():
    model = cp_model.CpModel()
    x = model.NewBoolVar('x')
    not_x = x.Not()
    print(x)
    print(not_x)


LiteralSampleSat()
