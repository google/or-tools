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

"""Code sample to demonstrates how to rank intervals using a circuit."""

from typing import List, Sequence


from ortools.sat.python import cp_model


def rank_tasks_with_circuit(
    model: cp_model.CpModel,
    starts: Sequence[cp_model.IntVar],
    durations: Sequence[int],
    presences: Sequence[cp_model.IntVar],
    ranks: Sequence[cp_model.IntVar],
) -> None:
  """This method uses a circuit constraint to rank tasks.

  This method assumes that all starts are disjoint, meaning that all tasks have
  a strictly positive duration, and they appear in the same NoOverlap
  constraint.

  To implement this ranking, we will create a dense graph with num_tasks + 1
  nodes.
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
    ranks: The array of rank variables of all tasks.
  """

  num_tasks = len(starts)
  all_tasks = range(num_tasks)

  arcs: List[cp_model.ArcT] = []
  for i in all_tasks:
    # if node i is first.
    start_lit = model.new_bool_var(f"start_{i}")
    arcs.append((0, i + 1, start_lit))
    model.add(ranks[i] == 0).only_enforce_if(start_lit)

    # As there are no other constraints on the problem, we can add this
    # redundant constraint.
    model.add(starts[i] == 0).only_enforce_if(start_lit)

    # if node i is last.
    end_lit = model.new_bool_var(f"end_{i}")
    arcs.append((i + 1, 0, end_lit))

    for j in all_tasks:
      if i == j:
        arcs.append((i + 1, i + 1, ~presences[i]))
        model.add(ranks[i] == -1).only_enforce_if(~presences[i])
      else:
        literal = model.new_bool_var(f"arc_{i}_to_{j}")
        arcs.append((i + 1, j + 1, literal))
        model.add(ranks[j] == ranks[i] + 1).only_enforce_if(literal)

        # To perform the transitive reduction from precedences to successors,
        # we need to tie the starts of the tasks with 'literal'.
        # In a pure problem, the following inequality could be an equality.
        # It is not true in general.
        #
        # Note that we could use this literal to penalize the transition, add an
        # extra delay to the precedence.
        model.add(starts[j] >= starts[i] + durations[i]).only_enforce_if(
            literal
        )

  # Manage the empty circuit
  empty = model.new_bool_var("empty")
  arcs.append((0, 0, empty))

  for i in all_tasks:
    model.add_implication(empty, ~presences[i])

  # Add the circuit constraint.
  model.add_circuit(arcs)


def ranking_sample_sat() -> None:
  """Ranks tasks in a NoOverlap constraint."""

  model = cp_model.CpModel()
  horizon = 100
  num_tasks = 4
  all_tasks = range(num_tasks)

  starts = []
  durations = []
  intervals = []
  presences = []
  ranks = []

  # Creates intervals, half of them are optional.
  for t in all_tasks:
    start = model.new_int_var(0, horizon, f"start[{t}]")
    duration = t + 1
    presence = model.new_bool_var(f"presence[{t}]")
    interval = model.new_optional_fixed_size_interval_var(
        start, duration, presence, f"opt_interval[{t}]"
    )
    if t < num_tasks // 2:
      model.add(presence == 1)

    starts.append(start)
    durations.append(duration)
    intervals.append(interval)
    presences.append(presence)

    # Ranks = -1 if and only if the tasks is not performed.
    ranks.append(model.new_int_var(-1, num_tasks - 1, f"rank[{t}]"))

  # Adds NoOverlap constraint.
  model.add_no_overlap(intervals)

  # Adds ranking constraint.
  rank_tasks_with_circuit(model, starts, durations, presences, ranks)

  # Adds a constraint on ranks.
  model.add(ranks[0] < ranks[1])

  # Creates makespan variable.
  makespan = model.new_int_var(0, horizon, "makespan")
  for t in all_tasks:
    model.add(starts[t] + durations[t] <= makespan).only_enforce_if(
        presences[t]
    )

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
