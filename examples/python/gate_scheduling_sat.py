#!/usr/bin/env python3
# Copyright 2010-2024 Google LLC
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

"""Gate Scheduling problem.

We have a set of jobs to perform (duration, width).
We have two parallel machines that can perform this job.
One machine can only perform one job at a time.
At any point in time, the sum of the width of the two active jobs does not
exceed a max_width.

The objective is to minimize the max end time of all jobs.
"""

from absl import app

from ortools.sat.colab import visualization
from ortools.sat.python import cp_model


def main(_) -> None:
    """Solves the gate scheduling problem."""
    model = cp_model.CpModel()

    jobs = [
        [3, 3],  # [duration, width]
        [2, 5],
        [1, 3],
        [3, 7],
        [7, 3],
        [2, 2],
        [2, 2],
        [5, 5],
        [10, 2],
        [4, 3],
        [2, 6],
        [1, 2],
        [6, 8],
        [4, 5],
        [3, 7],
    ]

    max_width = 10

    horizon = sum(t[0] for t in jobs)
    num_jobs = len(jobs)
    all_jobs = range(num_jobs)

    intervals = []
    intervals0 = []
    intervals1 = []
    performed = []
    starts = []
    ends = []
    demands = []

    for i in all_jobs:
        # Create main interval.
        start = model.new_int_var(0, horizon, f"start_{i}")
        duration = jobs[i][0]
        end = model.new_int_var(0, horizon, f"end_{i}")
        interval = model.new_interval_var(start, duration, end, f"interval_{i}")
        starts.append(start)
        intervals.append(interval)
        ends.append(end)
        demands.append(jobs[i][1])

        # Create an optional copy of interval to be executed on machine 0.
        performed_on_m0 = model.new_bool_var(f"perform_{i}_on_m0")
        performed.append(performed_on_m0)
        start0 = model.new_int_var(0, horizon, f"start_{i}_on_m0")
        end0 = model.new_int_var(0, horizon, f"end_{i}_on_m0")
        interval0 = model.new_optional_interval_var(
            start0, duration, end0, performed_on_m0, f"interval_{i}_on_m0"
        )
        intervals0.append(interval0)

        # Create an optional copy of interval to be executed on machine 1.
        start1 = model.new_int_var(0, horizon, f"start_{i}_on_m1")
        end1 = model.new_int_var(0, horizon, f"end_{i}_on_m1")
        interval1 = model.new_optional_interval_var(
            start1,
            duration,
            end1,
            ~performed_on_m0,
            f"interval_{i}_on_m1",
        )
        intervals1.append(interval1)

        # We only propagate the constraint if the tasks is performed on the machine.
        model.add(start0 == start).only_enforce_if(performed_on_m0)
        model.add(start1 == start).only_enforce_if(~performed_on_m0)

    # Width constraint (modeled as a cumulative)
    model.add_cumulative(intervals, demands, max_width)

    # Choose which machine to perform the jobs on.
    model.add_no_overlap(intervals0)
    model.add_no_overlap(intervals1)

    # Objective variable.
    makespan = model.new_int_var(0, horizon, "makespan")
    model.add_max_equality(makespan, ends)
    model.minimize(makespan)

    # Symmetry breaking.
    model.add(performed[0] == 0)

    # Solve model.
    solver = cp_model.CpSolver()
    solver.solve(model)

    # Output solution.
    if visualization.RunFromIPython():
        output = visualization.SvgWrapper(solver.objective_value, max_width, 40.0)
        output.AddTitle(f"Makespan = {solver.objective_value}")
        color_manager = visualization.ColorManager()
        color_manager.SeedRandomColor(0)

        for i in all_jobs:
            performed_machine = 1 - solver.value(performed[i])
            start_of_task = solver.value(starts[i])
            d_x = jobs[i][0]
            d_y = jobs[i][1]
            s_y = performed_machine * (max_width - d_y)
            output.AddRectangle(
                start_of_task,
                s_y,
                d_x,
                d_y,
                color_manager.RandomColor(),
                "black",
                f"j{i}",
            )

        output.AddXScale()
        output.AddYScale()
        output.Display()
    else:
        print("Solution")
        print(f"  - makespan = {solver.objective_value}")
        for i in all_jobs:
            performed_machine = 1 - solver.value(performed[i])
            start_of_task = solver.value(starts[i])
            print(
                f"  - Job {i} starts at {start_of_task} on machine"
                f" {performed_machine}"
            )
        print(solver.response_stats())


if __name__ == "__main__":
    app.run(main)
