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

"""Implements transition times and costs in a no_overlap constraint."""

from typing import Dict, List, Sequence, Tuple, Union

from ortools.sat.python import cp_model


def transitive_reduction_with_circuit_delays_and_penalties(
    model: cp_model.CpModel,
    starts: Sequence[cp_model.IntVar],
    durations: Sequence[int],
    presences: Sequence[Union[cp_model.IntVar, bool]],
    penalties: Dict[Tuple[int, int], int],
    delays: Dict[Tuple[int, int], int],
) -> Sequence[Tuple[cp_model.IntVar, int]]:
    """This method uses a circuit constraint to rank tasks.

    This method assumes that all starts are disjoint, meaning that all tasks have
    a strictly positive duration, and they appear in the same NoOverlap
    constraint.

    The extra node (with id 0) will be used to decide which task is first with
    its only outgoing arc, and which task is last with its only incoming arc.
    Each task i will be associated with id i + 1, and an arc between i + 1 and j +
    1 indicates that j is the immediate successor of i.

    The circuit constraint ensures there is at most 1 hamiltonian cycle of
    length > 1. If no such path exists, then no tasks are active.
    We also need to enforce that any hamiltonian cycle of size > 1 must contain
    the node 0. And thus, there is a self loop on node 0 iff the circuit is empty.

    Args:
      model: The CpModel to add the constraints to.
      starts: The array of starts variables of all tasks.
      durations: the durations of all tasks.
      presences: The array of presence variables of all tasks.
      penalties: the array of tuple (`tail_index`, `head_index`, `penalty`) that
        specifies that if task `tail_index` is the successor of the task
        `head_index`, then `penalty` must be added to the cost.
      delays: the array of tuple (`tail_index`, `head_index`, `delay`) that
        specifies that if task `tail_index` is the successor of the task
        `head_index`, then an extra `delay` must be added between the end of the
        first task and the start of the second task.

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

        # As there are no other constraints on the problem, we can add this
        # redundant constraint.
        model.add(starts[i] == 0).only_enforce_if(start_lit)

        # if node i is last.
        end_lit = model.new_bool_var(f"end_{i}")
        arcs.append((i + 1, 0, end_lit))

        for j in all_tasks:
            if i == j:
                arcs.append((i + 1, i + 1, ~presences[i]))
            else:
                literal = model.new_bool_var(f"arc_{i}_to_{j}")
                arcs.append((i + 1, j + 1, literal))

                # To perform the transitive reduction from precedences to successors,
                # we need to tie the starts of the tasks with 'literal'.
                # In a pure problem, the following inequality could be an equality.
                # It is not true in general.
                #
                # Note that we could use this literal to penalize the transition, add an
                # extra delay to the precedence.
                min_delay = 0
                key = (i, j)
                if key in delays:
                    min_delay = delays[key]
                model.add(
                    starts[j] >= starts[i] + durations[i] + min_delay
                ).only_enforce_if(literal)

                # Create the penalties.
                if key in penalties:
                    penalty_terms.append((literal, penalties[key]))

    # Manage the empty circuit
    empty = model.new_bool_var("empty")
    arcs.append((0, 0, empty))

    for i in all_tasks:
        model.add_implication(empty, ~presences[i])

    # Add the circuit constraint.
    model.add_circuit(arcs)

    return penalty_terms


def transitions_in_no_overlap_sample_sat():
    """Implement transitions in a NoOverlap constraint."""

    model = cp_model.CpModel()
    horizon = 40
    num_tasks = 4

    # Breaking the natural sequence induces a fixed penalty.
    penalties = {
        (1, 0): 10,
        (2, 0): 10,
        (3, 0): 10,
        (2, 1): 10,
        (3, 1): 10,
        (3, 2): 10,
    }

    # Switching from an odd to even or even to odd task indices induces a delay.
    delays = {
        (1, 0): 10,
        (0, 1): 10,
        (3, 0): 10,
        (0, 3): 10,
        (1, 2): 10,
        (2, 1): 10,
        (3, 2): 10,
        (2, 3): 10,
    }

    all_tasks = range(num_tasks)

    starts = []
    durations = []
    intervals = []
    presences = []

    # Creates intervals, all present. But the cost is robust w.r.t. optional
    # intervals.
    for t in all_tasks:
        start = model.new_int_var(0, horizon, f"start[{t}]")
        duration = 5
        presence = True
        interval = model.new_optional_fixed_size_interval_var(
            start, duration, presence, f"opt_interval[{t}]"
        )

        starts.append(start)
        durations.append(duration)
        intervals.append(interval)
        presences.append(presence)

    # Adds NoOverlap constraint.
    model.add_no_overlap(intervals)

    # Adds ranking constraint.
    penalty_terms = transitive_reduction_with_circuit_delays_and_penalties(
        model, starts, durations, presences, penalties, delays
    )

    # Minimize the sum of penalties,
    model.minimize(sum(var * penalty for var, penalty in penalty_terms))

    # In practise, only one penalty can happen. Thus the two even tasks are
    # together, same for the two odd tasks.
    # Because of the penalties, the optimal sequence is 0 -> 2 -> 1 -> 3
    # which induces one penalty and one delay.

    # Solves the model model.
    solver = cp_model.CpSolver()
    status = solver.solve(model)

    if status == cp_model.OPTIMAL:
        # Prints out the makespan and the start times and ranks of all tasks.
        print(f"Optimal cost: {solver.objective_value}")
        for t in all_tasks:
            if solver.value(presences[t]):
                print(f"Task {t} starts at {solver.value(starts[t])} ")
            else:
                print(f"Task {t} in not performed")
    else:
        print(f"Solver exited with nonoptimal status: {status}")


transitions_in_no_overlap_sample_sat()
