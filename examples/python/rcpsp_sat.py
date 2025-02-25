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

"""Sat based solver for the RCPSP problems (see rcpsp.proto).

Introduction to the problem:
   https://www.projectmanagement.ugent.be/research/project_scheduling/rcpsp

Data use in flags:
  http://www.om-db.wi.tum.de/psplib/data.html
"""

import collections
import time
from typing import Optional

from absl import app
from absl import flags

from google.protobuf import text_format
from ortools.sat.python import cp_model
from ortools.scheduling import rcpsp_pb2
from ortools.scheduling.python import rcpsp

_INPUT = flags.DEFINE_string("input", "", "Input file to parse and solve.")
_OUTPUT_PROTO = flags.DEFINE_string(
    "output_proto", "", "Output file to write the cp_model proto to."
)
_PARAMS = flags.DEFINE_string("params", "", "Sat solver parameters.")
_USE_INTERVAL_MAKESPAN = flags.DEFINE_bool(
    "use_interval_makespan",
    True,
    "Whether we encode the makespan using an interval or not.",
)
_HORIZON = flags.DEFINE_integer("horizon", -1, "Force horizon.")
_ADD_REDUNDANT_ENERGETIC_CONSTRAINTS = flags.DEFINE_bool(
    "add_redundant_energetic_constraints",
    False,
    "add redundant energetic constraints on the pairs of tasks extracted from"
    + " precedence graph.",
)
_DELAY_TIME_LIMIT = flags.DEFINE_float(
    "pairwise_delay_total_time_limit",
    120.0,
    "Total time limit when computing min delay between tasks."
    + " A non-positive time limit disable min delays computation.",
)
_PREEMPTIVE_LB_TIME_LIMIT = flags.DEFINE_float(
    "preemptive_lb_time_limit",
    0.0,
    "Time limit when computing a preemptive schedule lower bound."
    + " A non-positive time limit disable this computation.",
)


def print_problem_statistics(problem: rcpsp_pb2.RcpspProblem):
    """Display various statistics on the problem."""

    # Determine problem type.
    problem_type = (
        "Resource Investment Problem" if problem.is_resource_investment else "RCPSP"
    )

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
        problem_type += "/Max delay"
    # We print 2 less tasks as these are sentinel tasks that are not counted in
    # the description of the rcpsp models.
    if problem.is_consumer_producer:
        print(f"Solving {problem_type} with:")
        print(f"  - {num_resources} reservoir resources")
        print(f"  - {num_tasks} tasks")
    else:
        print(f"Solving {problem_type} with:")
        print(f"  - {num_resources} renewable resources")
        print(f"  - {num_tasks} tasks")
        if tasks_with_alternatives:
            print(f"    - {tasks_with_alternatives} tasks with alternative resources")
        if variable_duration_tasks:
            print(f"    - {variable_duration_tasks} tasks with variable durations")
        if tasks_with_delay:
            print(f"    - {tasks_with_delay} tasks with successor delays")


def analyse_dependency_graph(
    problem: rcpsp_pb2.RcpspProblem,
) -> tuple[list[tuple[int, int, list[int]]], dict[int, list[int]]]:
    """Analyses the dependency graph to improve the model.

    Args:
      problem: the protobuf of the problem to solve.

    Returns:
      a list of (task1, task2, in_between_tasks) with task2 and indirect successor
      of task1, and in_between_tasks being the list of all tasks after task1 and
      before task2.
    """

    num_nodes = len(problem.tasks)
    print(f"Analysing the dependency graph over {num_nodes} nodes")

    ins = collections.defaultdict(list)
    outs = collections.defaultdict(list)
    after = collections.defaultdict(set)
    before = collections.defaultdict(set)

    # Build the transitive closure of the precedences.
    # This algorithm has the wrong complexity (n^4), but is OK for the psplib
    # as the biggest example has 120 nodes.
    for n in range(num_nodes):
        for s in problem.tasks[n].successors:
            ins[s].append(n)
            outs[n].append(s)

            for a in list(after[s]) + [s]:
                for b in list(before[n]) + [n]:
                    after[b].add(a)
                    before[a].add(b)

    # Search for pair of tasks, containing at least two parallel branch between
    # them in the precedence graph.
    num_candidates = 0
    result: list[tuple[int, int, list[int]]] = []
    for source, start_outs in outs.items():
        if len(start_outs) <= 1:
            # Starting with the unique successor of source will be as good.
            continue
        for sink, end_ins in ins.items():
            if len(end_ins) <= 1:
                # Ending with the unique predecessor of sink will be as good.
                continue
            if sink == source:
                continue
            if sink not in after[source]:
                continue

            num_active_outgoing_branches = 0
            num_active_incoming_branches = 0
            for succ in outs[source]:
                if sink in after[succ]:
                    num_active_outgoing_branches += 1
            for pred in ins[sink]:
                if source in before[pred]:
                    num_active_incoming_branches += 1

            if num_active_outgoing_branches <= 1 or num_active_incoming_branches <= 1:
                continue

            common = after[source].intersection(before[sink])
            if len(common) <= 1:
                continue
            num_candidates += 1
            result.append((source, sink, common))

    # Sort entries lexicographically by (len(common), source, sink)
    def price(entry):
        return num_nodes * num_nodes * len(entry[2]) + num_nodes * entry[0] + entry[1]

    result.sort(key=price)
    print(f"  - created {len(result)} pairs of nodes to examine", flush=True)
    return result, after


def solve_rcpsp(
    problem: rcpsp_pb2.RcpspProblem,
    proto_file: str,
    params: str,
    active_tasks: set[int],
    source: int,
    sink: int,
    intervals_of_tasks: list[tuple[int, int, list[int]]],
    delays: dict[tuple[int, int], tuple[int, int]],
    in_main_solve: bool = False,
    initial_solution: Optional[rcpsp_pb2.RcpspAssignment] = None,
    lower_bound: int = 0,
) -> tuple[int, int, Optional[rcpsp_pb2.RcpspAssignment]]:
    """Parse and solve a given RCPSP problem in proto format.

    The model will only look at the tasks {source} + {sink} + active_tasks, and
    ignore all others.

    Args:
      problem: the description of the model to solve in protobuf format
      proto_file: the name of the file to export the CpModel proto to.
      params: the string representation of the parameters to pass to the sat
        solver.
      active_tasks: the set of active tasks to consider.
      source: the source task in the graph. Its end will be forced to 0.
      sink: the sink task of the graph. Its start is the makespan of the problem.
      intervals_of_tasks: a heuristic lists of (task1, task2, tasks) used to add
        redundant energetic equations to the model.
      delays: a list of (task1, task2, min_delays) used to add extended precedence
        constraints (start(task2) >= end(task1) + min_delay).
      in_main_solve: indicates if this is the main solve procedure.
      initial_solution: A valid assignment used to hint the search.
      lower_bound: A valid lower bound of the makespan objective.

    Returns:
      (lower_bound of the objective, best solution found, assignment)
    """
    # Create the model.
    model = cp_model.CpModel()
    model.name = problem.name

    num_resources = len(problem.resources)

    all_active_tasks = list(active_tasks)
    all_active_tasks.sort()
    all_resources = range(num_resources)

    horizon = problem.deadline if problem.deadline != -1 else problem.horizon
    if _HORIZON.value > 0:
        horizon = _HORIZON.value
    elif delays and in_main_solve and (source, sink) in delays:
        horizon = delays[(source, sink)][1]
    elif horizon == -1:  # Naive computation.
        horizon = sum(max(r.duration for r in t.recipes) for t in problem.tasks)
        if problem.is_rcpsp_max:
            for t in problem.tasks:
                for sd in t.successor_delays:
                    for rd in sd.recipe_delays:
                        for d in rd.min_delays:
                            horizon += abs(d)
    if in_main_solve:
        print(f"Horizon = {horizon}", flush=True)

    # Containers.
    task_starts = {}
    task_ends = {}
    task_durations = {}
    task_intervals = {}
    task_resource_to_energy = {}
    task_to_resource_demands = collections.defaultdict(list)

    task_to_presence_literals = collections.defaultdict(list)
    task_to_recipe_durations = collections.defaultdict(list)
    task_resource_to_fixed_demands = collections.defaultdict(dict)
    task_resource_to_max_energy = collections.defaultdict(int)

    resource_to_sum_of_demand_max = collections.defaultdict(int)

    # Create task variables.
    for t in all_active_tasks:
        task = problem.tasks[t]
        num_recipes = len(task.recipes)
        all_recipes = range(num_recipes)

        start_var = model.new_int_var(0, horizon, f"start_of_task_{t}")
        end_var = model.new_int_var(0, horizon, f"end_of_task_{t}")

        literals = []
        if num_recipes > 1:
            # Create one literal per recipe.
            literals = [model.new_bool_var(f"is_present_{t}_{r}") for r in all_recipes]

            # Exactly one recipe must be performed.
            model.add_exactly_one(literals)

        else:
            literals = [1]

        # Temporary data structure to fill in 0 demands.
        demand_matrix = collections.defaultdict(int)

        # Scan recipes and build the demand matrix and the vector of durations.
        for recipe_index, recipe in enumerate(task.recipes):
            task_to_recipe_durations[t].append(recipe.duration)
            for demand, resource in zip(recipe.demands, recipe.resources):
                demand_matrix[(resource, recipe_index)] = demand

        # Create the duration variable from the accumulated durations.
        duration_var = model.new_int_var_from_domain(
            cp_model.Domain.from_values(task_to_recipe_durations[t]),
            f"duration_of_task_{t}",
        )

        # Link the recipe literals and the duration_var.
        for r in range(num_recipes):
            model.add(duration_var == task_to_recipe_durations[t][r]).only_enforce_if(
                literals[r]
            )

        # Create the interval of the task.
        task_interval = model.new_interval_var(
            start_var, duration_var, end_var, f"task_interval_{t}"
        )

        # Store task variables.
        task_starts[t] = start_var
        task_ends[t] = end_var
        task_durations[t] = duration_var
        task_intervals[t] = task_interval
        task_to_presence_literals[t] = literals

        # Create the demand variable of the task for each resource.
        for res in all_resources:
            demands = [demand_matrix[(res, recipe)] for recipe in all_recipes]
            task_resource_to_fixed_demands[(t, res)] = demands
            demand_var = model.new_int_var_from_domain(
                cp_model.Domain.from_values(demands), f"demand_{t}_{res}"
            )
            task_to_resource_demands[t].append(demand_var)

            # Link the recipe literals and the demand_var.
            for r in all_recipes:
                model.add(demand_var == demand_matrix[(res, r)]).only_enforce_if(
                    literals[r]
                )

            resource_to_sum_of_demand_max[res] += max(demands)

        # Create the energy expression for (task, resource):
        for res in all_resources:
            task_resource_to_energy[(t, res)] = sum(
                literals[r]
                * task_to_recipe_durations[t][r]
                * task_resource_to_fixed_demands[(t, res)][r]
                for r in all_recipes
            )
            task_resource_to_max_energy[(t, res)] = max(
                task_to_recipe_durations[t][r]
                * task_resource_to_fixed_demands[(t, res)][r]
                for r in all_recipes
            )

    # Create makespan variable
    makespan = model.new_int_var(lower_bound, horizon, "makespan")
    makespan_size = model.new_int_var(1, horizon, "interval_makespan_size")
    interval_makespan = model.new_interval_var(
        makespan,
        makespan_size,
        model.new_constant(horizon + 1),
        "interval_makespan",
    )

    # Add precedences.
    if problem.is_rcpsp_max:
        # In RCPSP/Max problem, precedences are given and max delay (possible
        # negative) between the starts of two tasks.
        for task_id in all_active_tasks:
            task = problem.tasks[task_id]
            num_modes = len(task.recipes)

            for successor_index, next_id in enumerate(task.successors):
                delay_matrix = task.successor_delays[successor_index]
                num_next_modes = len(problem.tasks[next_id].recipes)
                for m1 in range(num_modes):
                    s1 = task_starts[task_id]
                    p1 = task_to_presence_literals[task_id][m1]
                    if next_id == sink:
                        delay = delay_matrix.recipe_delays[m1].min_delays[0]
                        model.add(s1 + delay <= makespan).only_enforce_if(p1)
                    else:
                        for m2 in range(num_next_modes):
                            delay = delay_matrix.recipe_delays[m1].min_delays[m2]
                            s2 = task_starts[next_id]
                            p2 = task_to_presence_literals[next_id][m2]
                            model.add(s1 + delay <= s2).only_enforce_if([p1, p2])
    else:
        # Normal dependencies (task ends before the start of successors).
        for t in all_active_tasks:
            for n in problem.tasks[t].successors:
                if n == sink:
                    model.add(task_ends[t] <= makespan)
                elif n in active_tasks:
                    model.add(task_ends[t] <= task_starts[n])

    # Containers for resource investment problems.
    capacities = []  # Capacity variables for all resources.
    max_cost = 0  # Upper bound on the investment cost.

    # Create resources.
    for res in all_resources:
        resource = problem.resources[res]
        c = resource.max_capacity
        if c == -1:
            print(f"No capacity: {resource}")
            c = resource_to_sum_of_demand_max[res]

        # RIP problems have only renewable resources, and no makespan.
        if problem.is_resource_investment or resource.renewable:
            intervals = [task_intervals[t] for t in all_active_tasks]
            demands = [task_to_resource_demands[t][res] for t in all_active_tasks]

            if problem.is_resource_investment:
                capacity = model.new_int_var(0, c, f"capacity_of_{res}")
                model.add_cumulative(intervals, demands, capacity)
                capacities.append(capacity)
                max_cost += c * resource.unit_cost
            else:  # Standard renewable resource.
                if _USE_INTERVAL_MAKESPAN.value:
                    intervals.append(interval_makespan)
                    demands.append(c)

                model.add_cumulative(intervals, demands, c)
        else:  # Non empty non renewable resource. (single mode only)
            if problem.is_consumer_producer:
                reservoir_starts = []
                reservoir_demands = []
                for t in all_active_tasks:
                    if task_resource_to_fixed_demands[(t, res)][0]:
                        reservoir_starts.append(task_starts[t])
                        reservoir_demands.append(
                            task_resource_to_fixed_demands[(t, res)][0]
                        )
                model.add_reservoir_constraint(
                    reservoir_starts,
                    reservoir_demands,
                    resource.min_capacity,
                    resource.max_capacity,
                )
            else:  # No producer-consumer. We just sum the demands.
                model.add(
                    cp_model.LinearExpr.sum(
                        [task_to_resource_demands[t][res] for t in all_active_tasks]
                    )
                    <= c
                )

    # Objective.
    if problem.is_resource_investment:
        objective = model.new_int_var(0, max_cost, "capacity_costs")
        model.add(
            objective
            == sum(
                problem.resources[i].unit_cost * capacities[i]
                for i in range(len(capacities))
            )
        )
    else:
        objective = makespan

    model.minimize(objective)

    # Add min delay constraints.
    if delays is not None:
        for (local_start, local_end), (min_delay, _) in delays.items():
            if local_start == source and local_end in active_tasks:
                model.add(task_starts[local_end] >= min_delay)
            elif local_start in active_tasks and local_end == sink:
                model.add(makespan >= task_ends[local_start] + min_delay)
            elif local_start in active_tasks and local_end in active_tasks:
                model.add(task_starts[local_end] >= task_ends[local_start] + min_delay)

    problem_is_single_mode = True
    for t in all_active_tasks:
        if len(task_to_presence_literals[t]) > 1:
            problem_is_single_mode = False
            break

    # Add sentinels.
    task_starts[source] = 0
    task_ends[source] = 0
    task_to_presence_literals[0].append(True)
    task_starts[sink] = makespan
    task_to_presence_literals[sink].append(True)

    # For multi-mode problems, add a redundant energetic constraint:
    # for every (start, end, in_between_tasks) extracted from the precedence
    # graph, it add the energetic relaxation:
    #   (start_var('end') - end_var('start')) * capacity_max >=
    #        sum of linearized energies of all tasks from 'in_between_tasks'
    if (
        not problem.is_resource_investment
        and not problem.is_consumer_producer
        and _ADD_REDUNDANT_ENERGETIC_CONSTRAINTS.value
        and in_main_solve
        and not problem_is_single_mode
    ):
        added_constraints = 0
        ignored_constraits = 0
        for local_start, local_end, common in intervals_of_tasks:
            for res in all_resources:
                resource = problem.resources[res]
                if not resource.renewable:
                    continue
                c = resource.max_capacity
                if delays and (local_start, local_end) in delays:
                    min_delay, _ = delays[local_start, local_end]
                    sum_of_max_energies = sum(
                        task_resource_to_max_energy[(t, res)] for t in common
                    )
                    if sum_of_max_energies <= c * min_delay:
                        ignored_constraits += 1
                        continue
                model.add(
                    c * (task_starts[local_end] - task_ends[local_start])
                    >= sum(task_resource_to_energy[(t, res)] for t in common)
                )
                added_constraints += 1
        print(
            f"Added {added_constraints} redundant energetic constraints, and "
            + f"ignored {ignored_constraits} constraints.",
            flush=True,
        )

    # Add solution hint.
    if initial_solution:
        for t in all_active_tasks:
            model.add_hint(task_starts[t], initial_solution.start_of_task[t])
            if len(task_to_presence_literals[t]) > 1:
                selected = initial_solution.selected_recipe_of_task[t]
                model.add_hint(task_to_presence_literals[t][selected], 1)

    # Write model to file.
    if proto_file:
        print(f"Writing proto to{proto_file}")
        model.export_to_file(proto_file)

    # Solve model.
    solver = cp_model.CpSolver()

    # Parse user specified parameters.
    if params:
        text_format.Parse(params, solver.parameters)

    # Favor objective_shaving over objective_lb_search.
    if solver.parameters.num_workers >= 16 and solver.parameters.num_workers < 24:
        solver.parameters.ignore_subsolvers.append("objective_lb_search")
        solver.parameters.extra_subsolvers.append("objective_shaving")

    # Experimental: Specify the fact that the objective is a makespan
    solver.parameters.push_all_tasks_toward_start = True

    # Enable logging in the main solve.

    if in_main_solve:
        solver.parameters.log_search_progress = True
    #
    status = solver.solve(model)
    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        assignment = rcpsp_pb2.RcpspAssignment()
        for t, _ in enumerate(problem.tasks):
            if t in task_starts:
                assignment.start_of_task.append(solver.value(task_starts[t]))
                for r, recipe_literal in enumerate(task_to_presence_literals[t]):
                    if solver.boolean_value(recipe_literal):
                        assignment.selected_recipe_of_task.append(r)
                        break
            else:  # t is not an active task.
                assignment.start_of_task.append(0)
                assignment.selected_recipe_of_task.append(0)
        return (
            int(solver.best_objective_bound),
            int(solver.objective_value),
            assignment,
        )
    return -1, -1, None


def compute_delays_between_nodes(
    problem: rcpsp_pb2.RcpspProblem,
    task_intervals: list[tuple[int, int, list[int]]],
) -> tuple[
    dict[tuple[int, int], tuple[int, int]],
    Optional[rcpsp_pb2.RcpspAssignment],
    bool,
]:
    """Computes the min delays between all pairs of tasks in 'task_intervals'.

    Args:
      problem: The protobuf of the model.
      task_intervals: The output of the AnalysePrecedenceGraph().

    Returns:
      a list of (task1, task2, min_delay_between_task1_and_task2)
    """
    print("Computing the minimum delay between pairs of intervals")
    delays = {}
    if (
        problem.is_resource_investment
        or problem.is_consumer_producer
        or problem.is_rcpsp_max
        or _DELAY_TIME_LIMIT.value <= 0.0
    ):
        return delays, None, False

    time_limit = _DELAY_TIME_LIMIT.value
    complete_problem_assignment = None
    num_optimal_delays = 0
    num_delays_not_found = 0
    optimal_found = True
    for start_task, end_task, active_tasks in task_intervals:
        if time_limit <= 0:
            optimal_found = False
            print(f"  - #timeout ({_DELAY_TIME_LIMIT.value}s) reached", flush=True)
            break

        start_time = time.time()
        min_delay, feasible_delay, assignment = solve_rcpsp(
            problem,
            "",
            f"num_search_workers:16,max_time_in_seconds:{time_limit}",
            set(active_tasks),
            start_task,
            end_task,
            [],
            delays,
        )
        time_limit -= time.time() - start_time

        if min_delay != -1:
            delays[(start_task, end_task)] = min_delay, feasible_delay
            if start_task == 0 and end_task == len(problem.tasks) - 1:
                complete_problem_assignment = assignment
            if min_delay == feasible_delay:
                num_optimal_delays += 1
            else:
                optimal_found = False
        else:
            num_delays_not_found += 1
            optimal_found = False

    print(f"  - #optimal delays = {num_optimal_delays}", flush=True)
    if num_delays_not_found:
        print(f"  - #not computed delays = {num_delays_not_found}", flush=True)

    return delays, complete_problem_assignment, optimal_found


def accept_new_candidate(
    problem: rcpsp_pb2.RcpspProblem,
    after: dict[int, list[int]],
    demand_map: dict[tuple[int, int], int],
    current: list[int],
    candidate: int,
) -> bool:
    """Check if candidate is compatible with the tasks in current."""
    for c in current:
        if candidate in after[c] or c in after[candidate]:
            return False

    all_resources = range(len(problem.resources))
    for res in all_resources:
        resource = problem.resources[res]
        if not resource.renewable:
            continue
        if (
            sum(demand_map[(t, res)] for t in current) + demand_map[(candidate, res)]
            > resource.max_capacity
        ):
            return False

    return True


def compute_preemptive_lower_bound(
    problem: rcpsp_pb2.RcpspProblem,
    after: dict[int, list[int]],
    lower_bound: int,
) -> int:
    """Computes a preemtive lower bound for the makespan statically.

    For this, it breaks all intervals into a set of intervals of size one.
    Then it will try to assign all of them in a minimum number of configurations.
    This is a standard complete set covering using column generation approach
    where each column is a possible combination of itervals of size one.

    Args:
      problem: The probuf of the model.
      after: a task to list of task dict that contains all tasks after a given
        task.
      lower_bound: A valid lower bound of the problem. It can be 0.

    Returns:
      a valid lower bound of the problem.
    """
    # Check this is a single mode problem.
    if (
        problem.is_rcpsp_max
        or problem.is_resource_investment
        or problem.is_consumer_producer
    ):
        return lower_bound

    demand_map = collections.defaultdict(int)
    duration_map = {}
    all_active_tasks = list(range(1, len(problem.tasks) - 1))
    max_duration = 0
    sum_of_demands = 0

    for t in all_active_tasks:
        task = problem.tasks[t]
        if len(task.recipes) > 1:
            return 0
        recipe = task.recipes[0]
        duration_map[t] = recipe.duration
        for demand, resource in zip(recipe.demands, recipe.resources):
            demand_map[(t, resource)] = demand
            max_duration = max(max_duration, recipe.duration)
            sum_of_demands += demand

    print(
        f"Compute a bin-packing lower bound with {len(all_active_tasks)}"
        + " active tasks",
        flush=True,
    )
    all_combinations = []

    for t in all_active_tasks:
        new_combinations = [[t]]

        for c in all_combinations:
            if accept_new_candidate(problem, after, demand_map, c, t):
                new_combinations.append(c + [t])

        all_combinations.extend(new_combinations)

    print(f"  - created {len(all_combinations)} combinations")
    if len(all_combinations) > 5000000:
        return lower_bound  # Abort if too large.

    # solve the selection model.

    # TODO(user): a few possible improvements:
    # 1/  use "dominating" columns, i.e. if you can add a task to a column, then
    #     do not use that column.
    # 2/ Merge all task with exactly same demands into one.
    model = cp_model.CpModel()
    model.name = f"lower_bound_{problem.name}"

    vars_per_task = collections.defaultdict(list)
    all_vars = []
    for c in all_combinations:
        min_duration = max_duration
        for t in c:
            min_duration = min(min_duration, duration_map[t])
        count = model.new_int_var(0, min_duration, f"count_{c}")
        all_vars.append(count)
        for t in c:
            vars_per_task[t].append(count)

    # Each task must be performed.
    for t in all_active_tasks:
        model.add(sum(vars_per_task[t]) >= duration_map[t])

    # Objective
    objective_var = model.new_int_var(lower_bound, sum_of_demands, "objective_var")
    model.add(objective_var == sum(all_vars))

    model.minimize(objective_var)

    # solve model.
    solver = cp_model.CpSolver()
    solver.parameters.num_search_workers = 16
    solver.parameters.max_time_in_seconds = _PREEMPTIVE_LB_TIME_LIMIT.value
    status = solver.solve(model)
    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        status_str = "optimal" if status == cp_model.OPTIMAL else ""
        lower_bound = max(lower_bound, int(solver.best_objective_bound))
        print(f"  - {status_str} static lower bound = {lower_bound}", flush=True)

    return lower_bound


def main(_):
    rcpsp_parser = rcpsp.RcpspParser()
    rcpsp_parser.parse_file(_INPUT.value)

    problem = rcpsp_parser.problem()
    print_problem_statistics(problem)

    intervals_of_tasks, after = analyse_dependency_graph(problem)
    delays, initial_solution, optimal_found = compute_delays_between_nodes(
        problem, intervals_of_tasks
    )

    last_task = len(problem.tasks) - 1
    key = (0, last_task)
    lower_bound = delays[key][0] if key in delays else 0
    if not optimal_found and _PREEMPTIVE_LB_TIME_LIMIT.value > 0.0:
        lower_bound = compute_preemptive_lower_bound(problem, after, lower_bound)

    solve_rcpsp(
        problem=problem,
        proto_file=_OUTPUT_PROTO.value,
        params=_PARAMS.value,
        active_tasks=set(range(1, last_task)),
        source=0,
        sink=last_task,
        intervals_of_tasks=intervals_of_tasks,
        delays=delays,
        in_main_solve=True,
        initial_solution=initial_solution,
        lower_bound=lower_bound,
    )


if __name__ == "__main__":
    app.run(main)
