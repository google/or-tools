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

"""Implements sequence constraints in a no_overlap constraint."""

from typing import Dict, List, Sequence, Tuple

from ortools.sat.python import cp_model


def sequence_constraints_with_circuit(
    model: cp_model.CpModel,
    starts: Sequence[cp_model.IntVar],
    durations: Sequence[int],
    task_types: Sequence[str],
    lengths: Sequence[cp_model.IntVar],
    cumuls: Sequence[cp_model.IntVar],
    sequence_length_constraints: Dict[str, Tuple[int, int]],
    sequence_cumul_constraints: Dict[str, Tuple[int, int, int]],
) -> Sequence[Tuple[cp_model.IntVar, int]]:
    """This method enforces constraints on sequences of tasks of the same type.

    This method assumes that all starts are disjoint, meaning that all tasks have
    a strictly positive duration, and they appear in the same NoOverlap
    constraint.

    The extra node (with id 0) will be used to decide which task is first with
    its only outgoing arc, and which task is last with its only incoming arc.
    Each task i will be associated with id i + 1, and an arc between i + 1 and j +
    1 indicates that j is the immediate successor of i.

    The circuit constraint ensures there is at most 1 hamiltonian cycle of
    length > 1. If no such path exists, then no tasks are active.
    In this simplified model, all tasks must be performed.

    Note that we do not enforce the minimum length constraint on the last sequence
    of tasks of the same type.

    Args:
      model: The CpModel to add the constraints to.
      starts: The array of starts variables of all tasks.
      durations: the durations of all tasks.
      task_types: The type of all tasks.
      lengths: The computed length of the current sequence for each task.
      cumuls: The computed cumul of the current sequence for each task.
      sequence_length_constraints: the array of tuple (`task_type`, (`length_min`,
        `length_max`)) that specifies the minimum and maximum length of the
        sequence of tasks of type `task_type`.
      sequence_cumul_constraints: the array of tuple (`task_type`, (`soft_max`,
        `linear_penalty`, `hard_max`)) that specifies that if the cumul of the
        sequence of tasks of type `task_type` is greater than `soft_max`, then
        `linear_penalty` must be added to the cost

    Returns:
      The list of pairs (Boolean variables, penalty) to be added to the objective.
    """

    num_tasks = len(starts)
    all_tasks = range(num_tasks)

    arcs: List[cp_model.ArcT] = []
    penalty_terms = []
    for i in all_tasks:
        # if node i is first.
        start_lit = model.new_bool_var(f"start_{i}")
        arcs.append((0, i + 1, start_lit))
        model.add(lengths[i] == 1).only_enforce_if(start_lit)
        model.add(cumuls[i] == durations[i]).only_enforce_if(start_lit)

        # As there are no other constraints on the problem, we can add this
        # redundant constraint. This is not valid in general.
        model.add(starts[i] == 0).only_enforce_if(start_lit)

        # if node i is last.
        end_lit = model.new_bool_var(f"end_{i}")
        arcs.append((i + 1, 0, end_lit))

        # Penalize the cumul of the last task w.r.t. the soft max
        soft_max, linear_penalty, hard_max = sequence_cumul_constraints[task_types[i]]
        if soft_max < hard_max:
            aux = model.new_int_var(0, hard_max - soft_max, f"aux_{i}")
            model.add_max_equality(aux, [0, cumuls[i] - soft_max])

            excess = model.new_int_var(0, hard_max - soft_max, f"excess_{i}")
            model.add(excess == aux).only_enforce_if(end_lit)
            model.add(excess == 0).only_enforce_if(~end_lit)
            penalty_terms.append((excess, linear_penalty))

        for j in all_tasks:
            if i == j:
                continue
            lit = model.new_bool_var(f"arc_{i}_to_{j}")
            arcs.append((i + 1, j + 1, lit))

            # To perform the transitive reduction from precedences to successors,
            # we need to tie the starts of the tasks with 'literal'.
            # In a non pure problem, the following equality must be an inequality.
            model.add(starts[j] == starts[i] + durations[i]).only_enforce_if(lit)

            # We add the constraints to incrementally maintain the length and the
            # cumul variables of the sequence.
            if task_types[i] == task_types[j]:  # Same task type.
                # Increase the length of the sequence by 1.
                model.add(lengths[j] == lengths[i] + 1).only_enforce_if(lit)

                # Make sure the length of the sequence is within the bounds of the task
                # type.
                type_length_max = sequence_length_constraints[task_types[j]][1]
                model.add(lengths[j] <= type_length_max).only_enforce_if(lit)

                # Increase the cumul of the sequence by the duration of the task.
                model.add(cumuls[j] == cumuls[i] + durations[j]).only_enforce_if(lit)

                # Make sure the cumul of the sequence is within the bounds.
                type_cumul_hard_max = sequence_cumul_constraints[task_types[j]][2]
                model.add(cumuls[j] <= type_cumul_hard_max).only_enforce_if(lit)

            else:
                # Switching task type. task[i] is the last task of the previous
                # sequence, task[j] is the first task of the new sequence.
                #
                # Reset the length to 1.
                model.add(lengths[j] == 1).only_enforce_if(lit)

                # Make sure the previous length is within bounds.
                type_length_min = sequence_length_constraints[task_types[i]][0]
                model.add(lengths[i] >= type_length_min).only_enforce_if(lit)

                # Reset the cumul to the duration of the task.
                # Note we do not check that the duration of the task is within bounds.
                model.add(cumuls[j] == durations[j]).only_enforce_if(lit)

                # Penalize the cumul of the previous task w.r.t. the soft max
                if soft_max < hard_max:
                    aux = model.new_int_var(0, hard_max - soft_max, f"aux_{i}")
                    model.add_max_equality(aux, [0, cumuls[i] - soft_max])

                    excess = model.new_int_var(0, hard_max - soft_max, f"excess_{i}")
                    model.add(excess == aux).only_enforce_if(lit)
                    model.add(excess == 0).only_enforce_if(~lit)
                    penalty_terms.append((excess, linear_penalty))

    # Add the circuit constraint.
    model.add_circuit(arcs)

    return penalty_terms


def sequences_in_no_overlap_sample_sat():
    """Implement cumul and length constraints in a NoOverlap constraint."""

    # Tasks (duration, type).
    tasks = [
        (5, "A"),
        (6, "A"),
        (7, "A"),
        (2, "A"),
        (3, "A"),
        (5, "B"),
        (2, "B"),
        (3, "B"),
        (1, "B"),
        (4, "B"),
        (3, "B"),
        (6, "B"),
        (2, "B"),
    ]

    # Sequence length constraints per task_types: (hard_min, hard_max)
    sequence_length_constraints = {
        "A": (1, 3),
        "B": (2, 2),
    }

    # Sequence accumulated durations constraints per task_types:
    #     (soft_max, linear_penalty, hard_max)
    sequence_cumul_constraints = {
        "A": (6, 1, 10),
        "B": (7, 0, 7),
    }

    model: cp_model.CpModel = cp_model.CpModel()
    horizon: int = sum(t[0] for t in tasks)

    num_tasks = len(tasks)
    all_tasks = range(num_tasks)

    starts = []
    durations = []
    intervals = []
    task_types = []

    # Creates intervals for each task.
    for duration, task_type in tasks:
        index = len(starts)
        start = model.new_int_var(0, horizon - duration, f"start[{index}]")
        interval = model.new_fixed_size_interval_var(
            start, duration, f"interval[{index}]"
        )

        starts.append(start)
        durations.append(duration)
        intervals.append(interval)
        task_types.append(task_type)

    # Create length variables for each task.
    lengths = []
    max_length = max(c[1] for c in sequence_length_constraints.values())
    for i in all_tasks:
        lengths.append(model.new_int_var(0, max_length, f"length_{i}"))

    # Create cumul variables for each task.
    cumuls = []
    max_cumul = max(c[2] for c in sequence_cumul_constraints.values())
    for i in all_tasks:
        cumuls.append(model.new_int_var(0, max_cumul, f"cumul_{i}"))

    # Adds NoOverlap constraint.
    model.add_no_overlap(intervals)

    # Adds the constraints on the lengths and cumuls of maximal sequences of
    # tasks of the same type.
    penalty_terms = sequence_constraints_with_circuit(
        model,
        starts,
        durations,
        task_types,
        lengths,
        cumuls,
        sequence_length_constraints,
        sequence_cumul_constraints,
    )

    # Minimize the sum of penalties,
    model.minimize(sum(var * penalty for var, penalty in penalty_terms))

    # Solves the model model.
    solver = cp_model.CpSolver()
    status = solver.solve(model)

    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        # Prints out the makespan and the start times and ranks of all tasks.
        if status == cp_model.OPTIMAL:
            print(f"Optimal cost: {solver.objective_value}")
        else:
            print(f"Feasible cost: {solver.objective_value}")

        to_sort = []
        for t in all_tasks:
            to_sort.append((solver.value(starts[t]), t))
        to_sort.sort()
        for start, t in to_sort:
            print(
                f"Task {t} of type {task_types[t]} with duration"
                f" {durations[t]} starts at {start}, length ="
                f" {solver.value(lengths[t])}, cumul = {solver.value(cumuls[t])} "
            )
    else:
        print(f"Solver exited with the following status: {status}")


sequences_in_no_overlap_sample_sat()
