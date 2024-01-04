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

"""Jobshop with maintenance tasks using the CP-SAT solver."""

import collections
from typing import Sequence
from absl import app
from ortools.sat.python import cp_model


class SolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__solution_count = 0

    def on_solution_callback(self):
        """Called at each new solution."""
        print(
            "Solution %i, time = %f s, objective = %i"
            % (self.__solution_count, self.wall_time, self.objective_value)
        )
        self.__solution_count += 1


def jobshop_with_maintenance():
    """Solves a jobshop with maintenance on one machine."""
    # Create the model.
    model = cp_model.CpModel()

    jobs_data = [  # task = (machine_id, processing_time).
        [(0, 3), (1, 2), (2, 2)],  # Job0
        [(0, 2), (2, 1), (1, 4)],  # Job1
        [(1, 4), (2, 3)],  # Job2
    ]

    machines_count = 1 + max(task[0] for job in jobs_data for task in job)
    all_machines = range(machines_count)

    # Computes horizon dynamically as the sum of all durations.
    horizon = sum(task[1] for job in jobs_data for task in job)

    # Named tuple to store information about created variables.
    task_type = collections.namedtuple("Task", "start end interval")
    # Named tuple to manipulate solution information.
    assigned_task_type = collections.namedtuple(
        "assigned_task_type", "start job index duration"
    )

    # Creates job intervals and add to the corresponding machine lists.
    all_tasks = {}
    machine_to_intervals = collections.defaultdict(list)

    for job_id, job in enumerate(jobs_data):
        for entry in enumerate(job):
            task_id, task = entry
            machine = task[0]
            duration = task[1]
            suffix = "_%i_%i" % (job_id, task_id)
            start_var = model.new_int_var(0, horizon, "start" + suffix)
            end_var = model.new_int_var(0, horizon, "end" + suffix)
            interval_var = model.new_interval_var(
                start_var, duration, end_var, "interval" + suffix
            )
            all_tasks[job_id, task_id] = task_type(
                start=start_var, end=end_var, interval=interval_var
            )
            machine_to_intervals[machine].append(interval_var)

    # Add maintenance interval (machine 0 is not available on time {4, 5, 6, 7}).
    machine_to_intervals[0].append(model.new_interval_var(4, 4, 8, "weekend_0"))

    # Create and add disjunctive constraints.
    for machine in all_machines:
        model.add_no_overlap(machine_to_intervals[machine])

    # Precedences inside a job.
    for job_id, job in enumerate(jobs_data):
        for task_id in range(len(job) - 1):
            model.add(
                all_tasks[job_id, task_id + 1].start >= all_tasks[job_id, task_id].end
            )

    # Makespan objective.
    obj_var = model.new_int_var(0, horizon, "makespan")
    model.add_max_equality(
        obj_var,
        [all_tasks[job_id, len(job) - 1].end for job_id, job in enumerate(jobs_data)],
    )
    model.minimize(obj_var)

    # Solve model.
    solver = cp_model.CpSolver()
    solution_printer = SolutionPrinter()
    status = solver.solve(model, solution_printer)

    # Output solution.
    if status == cp_model.OPTIMAL:
        # Create one list of assigned tasks per machine.
        assigned_jobs = collections.defaultdict(list)
        for job_id, job in enumerate(jobs_data):
            for task_id, task in enumerate(job):
                machine = task[0]
                assigned_jobs[machine].append(
                    assigned_task_type(
                        start=solver.value(all_tasks[job_id, task_id].start),
                        job=job_id,
                        index=task_id,
                        duration=task[1],
                    )
                )

        # Create per machine output lines.
        output = ""
        for machine in all_machines:
            # Sort by starting time.
            assigned_jobs[machine].sort()
            sol_line_tasks = "Machine " + str(machine) + ": "
            sol_line = "           "

            for assigned_task in assigned_jobs[machine]:
                name = "job_%i_%i" % (assigned_task.job, assigned_task.index)
                # add spaces to output to align columns.
                sol_line_tasks += "%-10s" % name
                start = assigned_task.start
                duration = assigned_task.duration

                sol_tmp = "[%i,%i]" % (start, start + duration)
                # add spaces to output to align columns.
                sol_line += "%-10s" % sol_tmp

            sol_line += "\n"
            sol_line_tasks += "\n"
            output += sol_line_tasks
            output += sol_line

        # Finally print the solution found.
        print("Optimal Schedule Length: %i" % solver.objective_value)
        print(output)
        print("Statistics")
        print("  - conflicts : %i" % solver.num_conflicts)
        print("  - branches  : %i" % solver.num_branches)
        print("  - wall time : %f s" % solver.wall_time)


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")
    jobshop_with_maintenance()


if __name__ == "__main__":
    app.run(main)
