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
from ortools.scheduling import pywraprcpsp
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

    # Containers.
    task_starts = {}
    task_ends = {}
    task_durations = {}
    task_intervals = {}
    task_to_resource_demands = collections.defaultdict(list)

    task_to_presence_literals = collections.defaultdict(list)
    task_to_recipe_durations = collections.defaultdict(list)
    task_resource_to_fixed_demands = collections.defaultdict(dict)

    resource_to_sum_of_demand_max = collections.defaultdict(int)

    # Create task variables.
    for t in all_active_tasks:
        task = problem.tasks[t]
        num_recipes = len(task.recipes)
        all_recipes = range(num_recipes)

        start_var = model.NewIntVar(0, horizon, f'start_of_task_{t}')
        end_var = model.NewIntVar(0, horizon, f'end_of_task_{t}')

        # Create one literal per recipe.
        literals = [
            model.NewBoolVar(f'is_present_{t}_{r}') for r in all_recipes
        ]

        # Exactly one recipe must be performed.
        model.Add(cp_model.LinearExpr.Sum(literals) == 1)

        # Temporary data structure to fill in 0 demands.
        demand_matrix = collections.defaultdict(int)

        # Scan recipes and build the demand matrix and the vector of durations.
        for recipe_index, recipe in enumerate(task.recipes):
            task_to_recipe_durations[t].append(recipe.duration)
            for demand, resource in zip(recipe.demands, recipe.resources):
                demand_matrix[(resource, recipe_index)] = demand

        # Create the duration variable from the accumulated durations.
        duration_var = model.NewIntVarFromDomain(
            cp_model.Domain.FromValues(task_to_recipe_durations[t]),
            f'duration_of_task_{t}')

        # linear encoding of the duration (link recipe literals and duration).
        min_duration = min(task_to_recipe_durations[t])
        shifted = [x - min_duration for x in task_to_recipe_durations[t]]
        model.Add(duration_var == min_duration +
                  cp_model.LinearExpr.ScalProd(literals, shifted))

        # Create the interval of the task.
        task_interval = model.NewIntervalVar(start_var, duration_var, end_var,
                                             f'task_interval_{t}')

        # Store task variables.
        task_starts[t] = start_var
        task_ends[t] = end_var
        task_durations[t] = duration_var
        task_intervals[t] = task_interval
        task_to_presence_literals[t] = literals

        # Create the demand variable of the task for each resource.
        for resource in all_resources:
            demands = [
                demand_matrix[(resource, recipe)] for recipe in all_recipes
            ]
            task_resource_to_fixed_demands[(t, resource)] = demands
            demand_var = model.NewIntVarFromDomain(
                cp_model.Domain.FromValues(demands), f'demand_{t}_{resource}')
            task_to_resource_demands[t].append(demand_var)

            # linear encoding of the demand per resource.
            min_demand = min(demands)
            shifted = [x - min_demand for x in demands]
            model.Add(demand_var == min_demand +
                      cp_model.LinearExpr.ScalProd(literals, shifted))
            resource_to_sum_of_demand_max[resource] += max(demands)

    # Create makespan variable
    makespan = model.NewIntVar(0, horizon, 'makespan')
    makespan_size = model.NewIntVar(1, horizon, 'interval_makespan_size')
    interval_makespan = model.NewIntervalVar(makespan, makespan_size,
                                             model.NewConstant(horizon + 1),
                                             'interval_makespan')

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
                    p1 = task_to_presence_literals[task_id][m1]
                    if next_id == num_tasks - 1:
                        delay = delay_matrix.recipe_delays[m1].min_delays[0]
                        model.Add(s1 + delay <= makespan).OnlyEnforceIf(p1)
                    else:
                        for m2 in range(num_next_modes):
                            delay = delay_matrix.recipe_delays[m1].min_delays[
                                m2]
                            s2 = task_starts[next_id]
                            p2 = task_to_presence_literals[next_id][m2]
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
            print(f'No capacity: {resource}')
            c = resource_to_sum_of_demand_max[r]

        # RIP problems have only renewable resources, and no makespan.
        if problem.is_resource_investment or resource.renewable:
            intervals = [task_intervals[t] for t in all_active_tasks]
            demands = [task_to_resource_demands[t][r] for t in all_active_tasks]

            if problem.is_resource_investment:
                capacity = model.NewIntVar(0, c, f'capacity_of_{r}')
                model.AddCumulative(intervals, demands, capacity)
                capacities.append(capacity)
                max_cost += c * resource.unit_cost
            else:  # Standard renewable resource.
                energies = []
                for t in all_active_tasks:
                    literals = task_to_presence_literals[t]
                    fixed_energies = [
                        task_resource_to_fixed_demands[(t, r)][index] *
                        task_to_recipe_durations[t][index]
                        for index in range(len(literals))
                    ]
                    min_energy = min(fixed_energies)
                    scaled_energies = [x - min_energy for x in fixed_energies]
                    energies.append(
                        min_energy +
                        cp_model.LinearExpr.ScalProd(literals, scaled_energies))

                if FLAGS.use_interval_makespan:
                    intervals.append(interval_makespan)
                    demands.append(c)
                    energies.append(c * makespan_size)
                model.AddCumulativeWithEnergy(intervals, demands, energies, c)
        else:  # Non empty non renewable resource. (single mode only)
            if problem.is_consumer_producer:
                reservoir_starts = []
                reservoir_demands = []
                for t in all_active_tasks:
                    if task_resource_to_fixed_demands[(t, r)][0]:
                        reservoir_starts.append(task_starts[t])
                        reservoir_demands.append(
                            task_resource_to_fixed_demands[(t, r)][0])
                model.AddReservoirConstraint(reservoir_starts,
                                             reservoir_demands,
                                             resource.min_capacity,
                                             resource.max_capacity)
            else:  # No producer-consumer. We just sum the demands.
                model.Add(
                    cp_model.LinearExpr.Sum([
                        task_to_resource_demands[t][r] for t in all_active_tasks
                    ]) <= c)

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
