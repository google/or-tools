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
"""Sat based solver for the RCPSP problems (see rcpsp.proto)."""

import collections

from absl import app
from absl import flags
from google.protobuf import text_format
from ortools.data import pywraprcpsp
from ortools.sat.python import cp_model

FLAGS = flags.FLAGS

flags.DEFINE_string('input', '', 'Input file to parse and solve.')
flags.DEFINE_string('output_proto', '',
                    'Output file to write the cp_model proto to.')
flags.DEFINE_string('params', '', 'Sat solver parameters.')
flags.DEFINE_bool('use_interval_makespan', True,
                  'Whether we encode the makespan using an interval or not.')
flags.DEFINE_integer('horizon', -1, 'Force horizon.')
flags.DEFINE_bool(
    'use_main_interval_for_tasks', True,
    'Creates a main interval for each task, and use it in precedences')


def PrintProblemStatistics(problem):
    """Display various statistics on the problem."""

    # Determine problem type.
    problem_type = ('Resource Investment Problem'
                    if problem.is_resource_investment else 'RCPSP')

    num_resources = len(problem.resources)
    num_tasks = len(problem.tasks) - 2  # 2 sentinels.
    tasks_with_alternatives = 0
    variable_duration_tasks = 0
    tasks_with_delay = 0

    for task in problem.tasks:
        if len(task.recipes) > 1:
            tasks_with_alternatives += 1
            duration_0 = task.recipes[0].duration
            for recipe in task.recipes:
                if recipe.duration != duration_0:
                    variable_duration_tasks += 1
                    break
        if task.successor_delays:
            tasks_with_delay += 1

    if problem.is_rcpsp_max:
        problem_type += '/Max delay'
    # We print 2 less tasks as these are sentinel tasks that are not counted in
    # the description of the rcpsp models.
    if problem.is_consumer_producer:
        print(f'Solving {problem_type} with:')
        print(f'  - {num_resources} reservoir resources')
        print(f'  - {num_tasks} tasks')
    else:
        print(f'Solving {problem_type} with:')
        print(f'  - {num_resources} renewable resources')
        print(f'  - {num_tasks} tasks')
        if tasks_with_alternatives:
            print(
                f'    - {tasks_with_alternatives} tasks with alternative resources'
            )
        if variable_duration_tasks:
            print(
                f'    - {variable_duration_tasks} tasks with variable durations'
            )
        if tasks_with_delay:
            print(f'    - {tasks_with_delay} tasks with successor delays')


def SolveRcpsp(problem, proto_file, params):
    """Parse and solve a given RCPSP problem in proto format."""
    PrintProblemStatistics(problem)

    # Create the model.
    model = cp_model.CpModel()

    num_tasks = len(problem.tasks)
    num_resources = len(problem.resources)

    all_active_tasks = range(1, num_tasks - 1)
    all_resources = range(num_resources)

    horizon = problem.deadline if problem.deadline != -1 else problem.horizon
    if FLAGS.horizon > 0:
        horizon = FLAGS.horizon
    if horizon == -1:  # Naive computation.
        horizon = sum(max(r.duration for r in t.recipes) for t in problem.tasks)
        if problem.is_rcpsp_max:
            for t in problem.tasks:
                for sd in t.successor_delays:
                    for rd in sd.recipe_delays:
                        for d in rd.min_delays:
                            horizon += abs(d)
    print(f'  - horizon = {horizon}')

    # Containers used to build resources.
    intervals_per_resource = collections.defaultdict(list)
    demands_per_resource = collections.defaultdict(list)
    presences_per_resource = collections.defaultdict(list)
    starts_per_resource = collections.defaultdict(list)

    # Starts and ends for each task (shared between all alternatives)
    task_starts = {}
    task_ends = {}

    # Containers for per-recipe per task alternatives variables.
    presences_per_task = collections.defaultdict(list)
    durations_per_task = collections.defaultdict(list)

    one = model.NewConstant(1)

    # Create tasks variables.
    for t in all_active_tasks:
        task = problem.tasks[t]

        if len(task.recipes) == 1:
            # Create main and unique interval.
            recipe = task.recipes[0]
            task_starts[t] = model.NewIntVar(0, horizon, f'start_of_task_{t}')
            task_ends[t] = model.NewIntVar(0, horizon, f'end_of_task_{t}')
            interval = model.NewIntervalVar(task_starts[t], recipe.duration,
                                            task_ends[t], f'interval_{t}')

            # Store as a single alternative for later.
            presences_per_task[t].append(one)
            durations_per_task[t].append(recipe.duration)

            # Register the interval in resources specified by the demands.
            for i in range(len(recipe.demands)):
                demand = recipe.demands[i]
                res = recipe.resources[i]
                demands_per_resource[res].append(demand)
                if problem.resources[res].renewable:
                    intervals_per_resource[res].append(interval)
                else:
                    starts_per_resource[res].append(task_starts[t])
                    presences_per_resource[res].append(1)
        else:  # Multiple alternative recipes.
            all_recipes = range(len(task.recipes))

            start = model.NewIntVar(0, horizon, f'start_of_task_{t}')
            end = model.NewIntVar(0, horizon, f'end_of_task_{t}')

            # Store for precedences.
            task_starts[t] = start
            task_ends[t] = end

            # Create one optional interval per recipe.
            for r in all_recipes:
                recipe = task.recipes[r]
                is_present = model.NewBoolVar(f'is_present_{t}_{r}')
                interval = model.NewOptionalIntervalVar(start, recipe.duration,
                                                        end, is_present,
                                                        f'interval_{t}_{r}')

                # Store alternative variables.
                presences_per_task[t].append(is_present)
                durations_per_task[t].append(recipe.duration)

                # Register the interval in resources specified by the demands.
                for i in range(len(recipe.demands)):
                    demand = recipe.demands[i]
                    res = recipe.resources[i]
                    demands_per_resource[res].append(demand)
                    if problem.resources[res].renewable:
                        intervals_per_resource[res].append(interval)
                    else:
                        starts_per_resource[res].append(start)
                        presences_per_resource[res].append(is_present)

            # Exactly one alternative must be performed.
            model.Add(sum(presences_per_task[t]) == 1)

            # linear encoding of the duration.
            min_duration = min(durations_per_task[t])
            max_duration = max(durations_per_task[t])
            shifted = [x - min_duration for x in durations_per_task[t]]

            duration = model.NewIntVar(min_duration, max_duration,
                                       f'duration_of_task_{t}')
            model.Add(
                duration == min_duration +
                cp_model.LinearExpr.ScalProd(presences_per_task[t], shifted))

            # We do not create a 'main' interval. Instead, we link start, end, and
            # duration.
            model.Add(start + duration == end)

    # Create makespan variable
    makespan = model.NewIntVar(0, horizon, 'makespan')
    interval_makespan = model.NewIntervalVar(
        makespan, model.NewIntVar(1, horizon, 'interval_makespan_size'),
        model.NewConstant(horizon + 1), 'interval_makespan')

    # Add precedences.
    if problem.is_rcpsp_max:
        # In RCPSP/Max problem, precedences are given and max delay (possible
        # negative) between the starts of two tasks.
        for task_id in all_active_tasks:
            task = problem.tasks[task_id]
            num_modes = len(task.recipes)

            for successor_index in range(len(task.successors)):
                next_id = task.successors[successor_index]
                delay_matrix = task.successor_delays[successor_index]
                num_next_modes = len(problem.tasks[next_id].recipes)
                for m1 in range(num_modes):
                    s1 = task_starts[task_id]
                    p1 = presences_per_task[task_id][m1]
                    if next_id == num_tasks - 1:
                        delay = delay_matrix.recipe_delays[m1].min_delays[0]
                        model.Add(s1 + delay <= makespan).OnlyEnforceIf(p1)
                    else:
                        for m2 in range(num_next_modes):
                            delay = delay_matrix.recipe_delays[m1].min_delays[
                                m2]
                            s2 = task_starts[next_id]
                            p2 = presences_per_task[next_id][m2]
                            model.Add(s1 + delay <= s2).OnlyEnforceIf([p1, p2])
    else:
        # Normal dependencies (task ends before the start of successors).
        for t in all_active_tasks:
            for n in problem.tasks[t].successors:
                if n == num_tasks - 1:
                    model.Add(task_ends[t] <= makespan)
                else:
                    model.Add(task_ends[t] <= task_starts[n])

    # Containers for resource investment problems.
    capacities = []  # Capacity variables for all resources.
    max_cost = 0  # Upper bound on the investment cost.

    # Create resources.
    for r in all_resources:
        resource = problem.resources[r]
        c = resource.max_capacity
        if c == -1:
            c = sum(demands_per_resource[r])

        if problem.is_resource_investment:
            # RIP problems have only renewable resources.
            capacity = model.NewIntVar(0, c, f'capacity_of_{r}')
            model.AddCumulative(intervals_per_resource[r],
                                demands_per_resource[r], capacity)
            capacities.append(capacity)
            max_cost += c * resource.unit_cost
        elif resource.renewable:
            if intervals_per_resource[r]:
                if FLAGS.use_interval_makespan:
                    model.AddCumulative(
                        intervals_per_resource[r] + [interval_makespan],
                        demands_per_resource[r] + [c], c)
                else:
                    model.AddCumulative(intervals_per_resource[r],
                                        demands_per_resource[r], c)
        elif presences_per_resource[r]:  # Non empty non renewable resource.
            if problem.is_consumer_producer:
                model.AddReservoirConstraint(starts_per_resource[r],
                                             demands_per_resource[r],
                                             resource.min_capacity,
                                             resource.max_capacity)
            else:
                model.Add(
                    sum(presences_per_resource[r][i] *
                        demands_per_resource[r][i]
                        for i in range(len(presences_per_resource[r]))) <= c)

    # Objective.
    if problem.is_resource_investment:
        objective = model.NewIntVar(0, max_cost, 'capacity_costs')
        model.Add(objective == sum(problem.resources[i].unit_cost *
                                   capacities[i]
                                   for i in range(len(capacities))))
    else:
        objective = makespan

    model.Minimize(objective)

    if proto_file:
        print(f'Writing proto to{proto_file}')
        with open(proto_file, 'w') as text_file:
            text_file.write(str(model))

    # Solve model.
    solver = cp_model.CpSolver()
    if params:
        text_format.Parse(params, solver.parameters)
    solver.parameters.log_search_progress = True
    solver.Solve(model)


def main(_):
    rcpsp_parser = pywraprcpsp.RcpspParser()
    rcpsp_parser.ParseFile(FLAGS.input)
    SolveRcpsp(rcpsp_parser.Problem(), FLAGS.output_proto, FLAGS.params)


if __name__ == '__main__':
    app.run(main)
