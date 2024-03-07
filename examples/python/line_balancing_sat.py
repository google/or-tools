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

"""Reader and solver of the single assembly line balancing problem.

from https://assembly-line-balancing.de/salbp/:

The simple assembly line balancing problem (SALBP) is the basic optimization
problem in assembly line balancing research. Given is a set of tasks each of
which has a deterministic task time. The tasks are partially ordered by
precedence relations defining a precedence graph as depicted below.

It reads .alb files:
    https://assembly-line-balancing.de/wp-content/uploads/2017/01/format-ALB.pdf

and solves the corresponding problem.
"""

import collections
import re
from typing import Sequence

from absl import app
from absl import flags
from google.protobuf import text_format

from ortools.sat.python import cp_model

_INPUT = flags.DEFINE_string("input", "", "Input file to parse and solve.")
_PARAMS = flags.DEFINE_string("params", "", "Sat solver parameters.")
_OUTPUT_PROTO = flags.DEFINE_string(
    "output_proto", "", "Output file to write the cp_model proto to."
)
_MODEL = flags.DEFINE_string(
    "model", "boolean", "Model used: boolean, scheduling, greedy"
)


class SectionInfo(object):
    """Store model information for each section of the input file."""

    def __init__(self):
        self.value = None
        self.index_map = {}
        self.set_of_pairs = set()

    def __str__(self):
        if self.index_map:
            return f"SectionInfo(index_map={self.index_map})"
        elif self.set_of_pairs:
            return f"SectionInfo(set_of_pairs={self.set_of_pairs})"
        elif self.value is not None:
            return f"SectionInfo(value={self.value})"
        else:
            return "SectionInfo()"


def read_model(filename):
    """Reads a .alb file and returns the model."""

    current_info = SectionInfo()

    model = {}
    with open(filename, "r") as input_file:
        print(f"Reading model from '{filename}'")
        section_name = ""

        for line in input_file:
            stripped_line = line.strip()
            if not stripped_line:
                continue

            match_section_def = re.match(r"<([\w\s]+)>", stripped_line)
            if match_section_def:
                section_name = match_section_def.group(1)
                if section_name == "end":
                    continue

                current_info = SectionInfo()
                model[section_name] = current_info
                continue

            match_single_number = re.match(r"^([0-9]+)$", stripped_line)
            if match_single_number:
                current_info.value = int(match_single_number.group(1))
                continue

            match_key_value = re.match(r"^([0-9]+)\s+([0-9]+)$", stripped_line)
            if match_key_value:
                key = int(match_key_value.group(1))
                value = int(match_key_value.group(2))
                current_info.index_map[key] = value
                continue

            match_pair = re.match(r"^([0-9]+),([0-9]+)$", stripped_line)
            if match_pair:
                left = int(match_pair.group(1))
                right = int(match_pair.group(2))
                current_info.set_of_pairs.add((left, right))
                continue

            print(f"Unrecognized line '{stripped_line}'")

    return model


def print_stats(model):
    print("Model Statistics")
    for key, value in model.items():
        print(f"  - {key}: {value}")


def solve_model_greedily(model):
    """Compute a greedy solution."""
    print("Solving using a Greedy heuristics")

    num_tasks = model["number of tasks"].value
    all_tasks = range(1, num_tasks + 1)  # Tasks are 1 based in the data.
    precedences = model["precedence relations"].set_of_pairs
    durations = model["task times"].index_map
    cycle_time = model["cycle time"].value

    weights = collections.defaultdict(int)
    successors = collections.defaultdict(list)

    candidates = set(all_tasks)

    for before, after in precedences:
        weights[after] += 1
        successors[before].append(after)
        if after in candidates:
            candidates.remove(after)

    assignment = {}
    current_pod = 0
    residual_capacity = cycle_time

    while len(assignment) < num_tasks:
        if not candidates:
            print("error empty")
            break

        best = -1
        best_slack = cycle_time
        best_duration = 0

        for c in candidates:
            duration = durations[c]
            slack = residual_capacity - duration
            if slack < best_slack and slack >= 0:
                best_slack = slack
                best = c
                best_duration = duration

        if best == -1:
            current_pod += 1
            residual_capacity = cycle_time
            continue

        candidates.remove(best)
        assignment[best] = current_pod
        residual_capacity -= best_duration

        for succ in successors[best]:
            weights[succ] -= 1
            if weights[succ] == 0:
                candidates.add(succ)
                del weights[succ]

    print(f"  greedy solution uses {current_pod + 1} pods.")

    return assignment


def solve_boolean_model(model, hint):
    """solve the given model."""

    print("Solving using the Boolean model")
    # Model data
    num_tasks = model["number of tasks"].value
    all_tasks = range(1, num_tasks + 1)  # Tasks are 1 based in the model.
    durations = model["task times"].index_map
    precedences = model["precedence relations"].set_of_pairs
    cycle_time = model["cycle time"].value

    num_pods = max(p for _, p in hint.items()) + 1 if hint else num_tasks - 1
    all_pods = range(num_pods)

    model = cp_model.CpModel()

    # assign[t, p] indicates if task t is done on pod p.
    assign = {}
    # possible[t, p] indicates if task t is possible on pod p.
    possible = {}

    # Create the variables
    for t in all_tasks:
        for p in all_pods:
            assign[t, p] = model.new_bool_var(f"assign_{t}_{p}")
            possible[t, p] = model.new_bool_var(f"possible_{t}_{p}")

    # active[p] indicates if pod p is active.
    active = [model.new_bool_var(f"active_{p}") for p in all_pods]

    # Each task is done on exactly one pod.
    for t in all_tasks:
        model.add_exactly_one([assign[t, p] for p in all_pods])

    # Total tasks assigned to one pod cannot exceed cycle time.
    for p in all_pods:
        model.add(sum(assign[t, p] * durations[t] for t in all_tasks) <= cycle_time)

    # Maintain the possible variables:
    #   possible at pod p -> possible at any pod after p
    for t in all_tasks:
        for p in range(num_pods - 1):
            model.add_implication(possible[t, p], possible[t, p + 1])

    # Link possible and active variables.
    for t in all_tasks:
        for p in all_pods:
            model.add_implication(assign[t, p], possible[t, p])
            if p > 1:
                model.add_implication(assign[t, p], ~possible[t, p - 1])

    # Precedences.
    for before, after in precedences:
        for p in range(1, num_pods):
            model.add_implication(assign[before, p], ~possible[after, p - 1])

    # Link active variables with the assign one.
    for p in all_pods:
        all_assign_vars = [assign[t, p] for t in all_tasks]
        for a in all_assign_vars:
            model.add_implication(a, active[p])
        model.add_bool_or(all_assign_vars + [~active[p]])

    # Force pods to be contiguous. This is critical to get good lower bounds
    # on the objective, even if it makes feasibility harder.
    for p in range(1, num_pods):
        model.add_implication(~active[p - 1], ~active[p])
        for t in all_tasks:
            model.add_implication(~active[p], possible[t, p - 1])

    # Objective.
    model.minimize(sum(active))

    # add search hinting from the greedy solution.
    for t in all_tasks:
        model.add_hint(assign[t, hint[t]], 1)

    if _OUTPUT_PROTO.value:
        print(f"Writing proto to {_OUTPUT_PROTO.value}")
        model.export_to_file(_OUTPUT_PROTO.value)

    # solve model.
    solver = cp_model.CpSolver()
    if _PARAMS.value:
        text_format.Parse(_PARAMS.value, solver.parameters)
    solver.parameters.log_search_progress = True
    solver.solve(model)


def solve_scheduling_model(model, hint):
    """solve the given model using a cumutive model."""

    print("Solving using the scheduling model")
    # Model data
    num_tasks = model["number of tasks"].value
    all_tasks = range(1, num_tasks + 1)  # Tasks are 1 based in the data.
    durations = model["task times"].index_map
    precedences = model["precedence relations"].set_of_pairs
    cycle_time = model["cycle time"].value

    num_pods = max(p for _, p in hint.items()) + 1 if hint else num_tasks

    model = cp_model.CpModel()

    # pod[t] indicates on which pod the task is performed.
    pods = {}
    for t in all_tasks:
        pods[t] = model.new_int_var(0, num_pods - 1, f"pod_{t}")

    # Create the variables
    intervals = []
    demands = []
    for t in all_tasks:
        interval = model.new_fixed_size_interval_var(pods[t], 1, "")
        intervals.append(interval)
        demands.append(durations[t])

    # add terminating interval as the objective.
    obj_var = model.new_int_var(1, num_pods, "obj_var")
    obj_size = model.new_int_var(1, num_pods, "obj_duration")
    obj_interval = model.new_interval_var(
        obj_var, obj_size, num_pods + 1, "obj_interval"
    )
    intervals.append(obj_interval)
    demands.append(cycle_time)

    # Cumulative constraint.
    model.add_cumulative(intervals, demands, cycle_time)

    # Precedences.
    for before, after in precedences:
        model.add(pods[after] >= pods[before])

    # Objective.
    model.minimize(obj_var)

    # add search hinting from the greedy solution.
    for t in all_tasks:
        model.add_hint(pods[t], hint[t])

    if _OUTPUT_PROTO.value:
        print(f"Writing proto to{_OUTPUT_PROTO.value}")
        model.export_to_file(_OUTPUT_PROTO.value)

    # solve model.
    solver = cp_model.CpSolver()
    if _PARAMS.value:
        text_format.Parse(_PARAMS.value, solver.parameters)
    solver.parameters.log_search_progress = True
    solver.solve(model)


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")

    model = read_model(_INPUT.value)
    print_stats(model)
    greedy_solution = solve_model_greedily(model)

    if _MODEL.value == "boolean":
        solve_boolean_model(model, greedy_solution)
    elif _MODEL.value == "scheduling":
        solve_scheduling_model(model, greedy_solution)


if __name__ == "__main__":
    app.run(main)
