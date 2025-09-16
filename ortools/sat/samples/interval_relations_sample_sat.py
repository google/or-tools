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

# [START program]
"""Builds temporal relations between intervals."""

from ortools.sat.python import cp_model


def interval_relations_sample_sat():
    """Showcases how to build temporal relations between intervals."""
    model = cp_model.CpModel()
    horizon = 100

    # An interval can be created from three 1-var affine expressions.
    start_var = model.new_int_var(0, horizon, "start")
    duration = 10  # Python CP-SAT code accept integer variables or constants.
    end_var = model.new_int_var(0, horizon, "end")
    interval_var = model.new_interval_var(start_var, duration, end_var, "interval")

    # If the size is fixed, a simpler version uses the start expression and the
    # size.
    fixed_size_start_var = model.new_int_var(0, horizon, "fixed_start")
    fixed_size_duration = 10
    fixed_size_interval_var = model.new_fixed_size_interval_var(
        fixed_size_start_var,
        fixed_size_duration,
        "fixed_size_interval_var",
    )

    # An optional interval can be created from three 1-var affine expressions and
    # a literal.
    opt_start_var = model.new_int_var(0, horizon, "opt_start")
    opt_duration = model.new_int_var(2, 6, "opt_size")
    opt_end_var = model.new_int_var(0, horizon, "opt_end")
    opt_presence_var = model.new_bool_var("opt_presence")
    opt_interval_var = model.new_optional_interval_var(
        opt_start_var, opt_duration, opt_end_var, opt_presence_var, "opt_interval"
    )

    # If the size is fixed, a simpler version uses the start expression, the
    # size, and the presence literal.
    opt_fixed_size_start_var = model.new_int_var(0, horizon, "opt_fixed_start")
    opt_fixed_size_duration = 10
    opt_fixed_size_presence_var = model.new_bool_var("opt_fixed_presence")
    opt_fixed_size_interval_var = model.new_optional_fixed_size_interval_var(
        opt_fixed_size_start_var,
        opt_fixed_size_duration,
        opt_fixed_size_presence_var,
        "opt_fixed_size_interval_var",
    )

    # Simple precedence between two non optional intervals.
    model.add(interval_var.start_expr() >= fixed_size_interval_var.end_expr())

    # Synchronize start between two intervals (one optional, one not)
    model.add(
        interval_var.start_expr() == opt_interval_var.start_expr()
    ).only_enforce_if(opt_presence_var)

    # Exact delay between two optional intervals.
    exact_delay: int = 5
    model.add(
        opt_interval_var.start_expr()
        == opt_fixed_size_interval_var.end_expr() + exact_delay
    ).only_enforce_if(opt_presence_var, opt_fixed_size_presence_var)


interval_relations_sample_sat()
# [END program]
