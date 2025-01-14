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

"""Code sample to demonstrate how to build a NoOverlap constraint."""

from ortools.sat.python import cp_model


def no_overlap_sample_sat():
    """No overlap sample with fixed activities."""
    model = cp_model.CpModel()
    horizon = 21  # 3 weeks.

    # Task 0, duration 2.
    start_0 = model.new_int_var(0, horizon, "start_0")
    duration_0 = 2  # Python cp/sat code accepts integer variables or constants.
    end_0 = model.new_int_var(0, horizon, "end_0")
    task_0 = model.new_interval_var(start_0, duration_0, end_0, "task_0")
    # Task 1, duration 4.
    start_1 = model.new_int_var(0, horizon, "start_1")
    duration_1 = 4  # Python cp/sat code accepts integer variables or constants.
    end_1 = model.new_int_var(0, horizon, "end_1")
    task_1 = model.new_interval_var(start_1, duration_1, end_1, "task_1")

    # Task 2, duration 3.
    start_2 = model.new_int_var(0, horizon, "start_2")
    duration_2 = 3  # Python cp/sat code accepts integer variables or constants.
    end_2 = model.new_int_var(0, horizon, "end_2")
    task_2 = model.new_interval_var(start_2, duration_2, end_2, "task_2")

    # Weekends.
    weekend_0 = model.new_interval_var(5, 2, 7, "weekend_0")
    weekend_1 = model.new_interval_var(12, 2, 14, "weekend_1")
    weekend_2 = model.new_interval_var(19, 2, 21, "weekend_2")

    # No Overlap constraint.
    model.add_no_overlap([task_0, task_1, task_2, weekend_0, weekend_1, weekend_2])

    # Makespan objective.
    obj = model.new_int_var(0, horizon, "makespan")
    model.add_max_equality(obj, [end_0, end_1, end_2])
    model.minimize(obj)

    # Solve model.
    solver = cp_model.CpSolver()
    status = solver.solve(model)

    if status == cp_model.OPTIMAL:
        # Print out makespan and the start times for all tasks.
        print(f"Optimal Schedule Length: {solver.objective_value}")
        print(f"Task 0 starts at {solver.value(start_0)}")
        print(f"Task 1 starts at {solver.value(start_1)}")
        print(f"Task 2 starts at {solver.value(start_2)}")
    else:
        print(f"Solver exited with nonoptimal status: {status}")


no_overlap_sample_sat()
