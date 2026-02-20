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
"""Code sample to demonstrates how to rank intervals."""

from ortools.sat.python import cp_model


def rank_tasks(
    model: cp_model.CpModel,
    starts: list[cp_model.IntVar],
    presences: list[cp_model.BoolVarT],
    ranks: list[cp_model.IntVar],
) -> None:
    """This method adds constraints and variables to links tasks and ranks.

    This method assumes that all starts are disjoint, meaning that all tasks have
    a strictly positive duration, and they appear in the same NoOverlap
    constraint.

    Args:
      model: The CpModel to add the constraints to.
      starts: The array of starts variables of all tasks.
      presences: The array of presence variables or constants of all tasks.
      ranks: The array of rank variables of all tasks.
    """

    num_tasks = len(starts)
    all_tasks = range(num_tasks)

    # Creates precedence variables between pairs of intervals.
    precedences: dict[tuple[int, int], cp_model.BoolVarT] = {}
    for i in all_tasks:
        for j in all_tasks:
            if i == j:
                precedences[(i, j)] = presences[i]
            else:
                prec = model.new_bool_var(f"{i} before {j}")
                precedences[(i, j)] = prec
                model.add(starts[i] < starts[j]).only_enforce_if(prec)

    # Treats optional intervals.
    for i in range(num_tasks - 1):
        for j in range(i + 1, num_tasks):
            tmp_array: list[cp_model.BoolVarT] = [
                precedences[(i, j)],
                precedences[(j, i)],
            ]
            if not cp_model.object_is_a_true_literal(presences[i]):
                tmp_array.append(~presences[i])
                # Makes sure that if i is not performed, all precedences are false.
                model.add_implication(~presences[i], ~precedences[(i, j)])
                model.add_implication(~presences[i], ~precedences[(j, i)])
            if not cp_model.object_is_a_true_literal(presences[j]):
                tmp_array.append(~presences[j])
                # Makes sure that if j is not performed, all precedences are false.
                model.add_implication(~presences[j], ~precedences[(i, j)])
                model.add_implication(~presences[j], ~precedences[(j, i)])
            # The following bool_or will enforce that for any two intervals:
            #    i precedes j or j precedes i or at least one interval is not
            #        performed.
            model.add_bool_or(tmp_array)
            # Redundant constraint: it propagates early that at most one precedence
            # is true.
            model.add_implication(precedences[(i, j)], ~precedences[(j, i)])
            model.add_implication(precedences[(j, i)], ~precedences[(i, j)])

    # Links precedences and ranks.
    for i in all_tasks:
        model.add(ranks[i] == sum(precedences[(j, i)] for j in all_tasks) - 1)


def ranking_sample_sat() -> None:
    """Ranks tasks in a NoOverlap constraint."""

    model = cp_model.CpModel()
    horizon = 100
    num_tasks = 4
    all_tasks = range(num_tasks)

    starts = []
    ends = []
    intervals = []
    presences: list[cp_model.BoolVarT] = []
    ranks = []

    # Creates intervals, half of them are optional.
    for t in all_tasks:
        start = model.new_int_var(0, horizon, f"start[{t}]")
        duration = t + 1
        end = model.new_int_var(0, horizon, f"end[{t}]")
        if t < num_tasks // 2:
            interval = model.new_interval_var(start, duration, end, f"interval[{t}]")
            presence = model.new_constant(1)
        else:
            presence = model.new_bool_var(f"presence[{t}]")
            interval = model.new_optional_interval_var(
                start, duration, end, presence, f"o_interval[{t}]"
            )
        starts.append(start)
        ends.append(end)
        intervals.append(interval)
        presences.append(presence)

        # Ranks = -1 if and only if the tasks is not performed.
        ranks.append(model.new_int_var(-1, num_tasks - 1, f"rank[{t}]"))

    # Adds NoOverlap constraint.
    model.add_no_overlap(intervals)

    # Adds ranking constraint.
    rank_tasks(model, starts, presences, ranks)

    # Adds a constraint on ranks.
    model.add(ranks[0] < ranks[1])

    # Creates makespan variable.
    makespan = model.new_int_var(0, horizon, "makespan")
    for t in all_tasks:
        model.add(ends[t] <= makespan).only_enforce_if(presences[t])

    # Minimizes makespan - fixed gain per tasks performed.
    # As the fixed cost is less that the duration of the last interval,
    # the solver will not perform the last interval.
    model.minimize(2 * makespan - 7 * sum(presences[t] for t in all_tasks))

    # Solves the model model.
    solver = cp_model.CpSolver()
    status = solver.solve(model)

    if status == cp_model.OPTIMAL:
        # Prints out the makespan and the start times and ranks of all tasks.
        print(f"Optimal cost: {solver.objective_value}")
        print(f"Makespan: {solver.value(makespan)}")
        for t in all_tasks:
            if solver.value(presences[t]):
                print(
                    f"Task {t} starts at {solver.value(starts[t])} "
                    f"with rank {solver.value(ranks[t])}"
                )
            else:
                print(
                    f"Task {t} in not performed and ranked at {solver.value(ranks[t])}"
                )
    else:
        print(f"Solver exited with nonoptimal status: {status}")


ranking_sample_sat()
# [END program]
