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

"""solve a random Weighted Latency problem with the CP-SAT solver."""

import random
from typing import Sequence
from absl import app
from absl import flags

from google.protobuf import text_format
from ortools.sat.python import cp_model

_NUM_NODES = flags.DEFINE_integer("num_nodes", 12, "Number of nodes to visit.")
_GRID_SIZE = flags.DEFINE_integer("grid_size", 20, "Size of the grid where nodes are.")
_PROFIT_RANGE = flags.DEFINE_integer("profit_range", 50, "Range of profit.")
_SEED = flags.DEFINE_integer("seed", 0, "Random seed.")
_PARAMS = flags.DEFINE_string(
    "params", "num_search_workers:16, max_time_in_seconds:5", "Sat solver parameters."
)
_PROTO_FILE = flags.DEFINE_string(
    "proto_file", "", "If not empty, output the proto to this file."
)


def build_model():
    """Create the nodes and the profit."""
    random.seed(_SEED.value)
    x = []
    y = []
    x.append(random.randint(0, _GRID_SIZE.value))
    y.append(random.randint(0, _GRID_SIZE.value))
    for _ in range(_NUM_NODES.value):
        x.append(random.randint(0, _GRID_SIZE.value))
        y.append(random.randint(0, _GRID_SIZE.value))

    profits = []
    profits.append(0)
    for _ in range(_NUM_NODES.value):
        profits.append(random.randint(1, _PROFIT_RANGE.value))
    sum_of_profits = sum(profits)
    profits = [p / sum_of_profits for p in profits]

    return x, y, profits


def solve_with_cp_sat(x, y, profits):
    """Solves the problem with the CP-SAT solver."""
    model = cp_model.CpModel()

    # because of the manhattan distance, the sum of distances is bounded by this.
    horizon = _GRID_SIZE.value * 2 * _NUM_NODES.value
    times = [
        model.new_int_var(0, horizon, f"x_{i}") for i in range(_NUM_NODES.value + 1)
    ]

    # Node 0 is the start node.
    model.add(times[0] == 0)

    # Create the circuit constraint.
    arcs = []
    for i in range(_NUM_NODES.value + 1):
        for j in range(_NUM_NODES.value + 1):
            if i == j:
                continue
            # We use a manhattan distance between nodes.
            distance = abs(x[i] - x[j]) + abs(y[i] - y[j])
            lit = model.new_bool_var(f"{i}_to_{j}")
            arcs.append((i, j, lit))

            # add transitions between nodes.
            if i == 0:
                # Initial transition
                model.add(times[j] == distance).only_enforce_if(lit)
            elif j != 0:
                # We do not care for the last transition.
                model.add(times[j] == times[i] + distance).only_enforce_if(lit)
    model.add_circuit(arcs)

    model.minimize(cp_model.LinearExpr.weighted_sum(times, profits))

    if _PROTO_FILE.value:
        model.export_to_file(_PROTO_FILE.value)

    # Solve model.
    solver = cp_model.CpSolver()
    if _PARAMS.value:
        text_format.Parse(_PARAMS.value, solver.parameters)
    solver.parameters.log_search_progress = True
    solver.solve(model)


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")

    x, y, profits = build_model()
    solve_with_cp_sat(x, y, profits)
    # TODO(user): Implement routing model.


if __name__ == "__main__":
    app.run(main)
