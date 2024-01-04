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

"""MaxFlow and MinCostFlow examples."""

from typing import Sequence
from absl import app
from ortools.graph.python import max_flow
from ortools.graph.python import min_cost_flow


def max_flow_api():
    """MaxFlow simple interface example."""
    print("MaxFlow on a simple network.")
    tails = [0, 0, 0, 0, 1, 2, 3, 3, 4]
    heads = [1, 2, 3, 4, 3, 4, 4, 5, 5]
    capacities = [5, 8, 5, 3, 4, 5, 6, 6, 4]
    expected_total_flow = 10
    smf = max_flow.SimpleMaxFlow()
    for i in range(0, len(tails)):
        smf.add_arc_with_capacity(tails[i], heads[i], capacities[i])
    if smf.solve(0, 5) == smf.OPTIMAL:
        print("Total flow", smf.optimal_flow(), "/", expected_total_flow)
        for i in range(smf.num_arcs()):
            print(
                "From source %d to target %d: %d / %d"
                % (smf.tail(i), smf.head(i), smf.flow(i), smf.capacity(i))
            )
        print("Source side min-cut:", smf.get_source_side_min_cut())
        print("Sink side min-cut:", smf.get_sink_side_min_cut())
    else:
        print("There was an issue with the max flow input.")


def min_cost_flow_api():
    """MinCostFlow simple interface example.

    Note that this example is actually a linear sum assignment example and will
    be more efficiently solved with the pywrapgraph.LinearSumAssignment class.
    """
    print("MinCostFlow on 4x4 matrix.")
    num_sources = 4
    num_targets = 4
    costs = [[90, 75, 75, 80], [35, 85, 55, 65], [125, 95, 90, 105], [45, 110, 95, 115]]
    expected_cost = 275
    smcf = min_cost_flow.SimpleMinCostFlow()
    for source in range(0, num_sources):
        for target in range(0, num_targets):
            smcf.add_arc_with_capacity_and_unit_cost(
                source, num_sources + target, 1, costs[source][target]
            )
    for node in range(0, num_sources):
        smcf.set_node_supply(node, 1)
        smcf.set_node_supply(num_sources + node, -1)
    status = smcf.solve()
    if status == smcf.OPTIMAL:
        print("Total flow", smcf.optimal_cost(), "/", expected_cost)
        for i in range(0, smcf.num_arcs()):
            if smcf.flow(i) > 0:
                print(
                    "From source %d to target %d: cost %d"
                    % (smcf.tail(i), smcf.head(i) - num_sources, smcf.unit_cost(i))
                )
    else:
        print("There was an issue with the min cost flow input.")


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")
    max_flow_api()
    min_cost_flow_api()


if __name__ == "__main__":
    app.run(main)
