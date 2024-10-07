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

# [START program]
"""Linear assignment example."""
# [START import]
from ortools.graph.python import min_cost_flow
# [END import]


def main():
    """Solving an Assignment Problem with MinCostFlow."""
    # [START solver]
    # Instantiate a SimpleMinCostFlow solver.
    smcf = min_cost_flow.SimpleMinCostFlow()
    # [END solver]

    # [START data]
    # Define the directed graph for the flow.
    start_nodes = (
        [0, 0, 0, 0] + [1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4] + [5, 6, 7, 8]
    )
    end_nodes = (
        [1, 2, 3, 4] + [5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8] + [9, 9, 9, 9]
    )
    capacities = (
        [1, 1, 1, 1] + [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1] + [1, 1, 1, 1]
    )
    costs = (
        [0, 0, 0, 0]
        + [90, 76, 75, 70, 35, 85, 55, 65, 125, 95, 90, 105, 45, 110, 95, 115]
        + [0, 0, 0, 0]
    )

    source = 0
    sink = 9
    tasks = 4
    supplies = [tasks, 0, 0, 0, 0, 0, 0, 0, 0, -tasks]
    # [END data]

    # [START constraints]
    # Add each arc.
    for start_node, end_node, capacity, cost in zip(
        start_nodes, end_nodes, capacities, costs
    ):
        smcf.add_arc_with_capacity_and_unit_cost(start_node, end_node, capacity, cost)
    # Add node supplies.
    for idx, supply in enumerate(supplies):
        smcf.set_node_supply(idx, supply)
    # [END constraints]

    # [START solve]
    # Find the minimum cost flow between node 0 and node 10.
    status = smcf.solve()
    # [END solve]

    # [START print_solution]
    if status == smcf.OPTIMAL:
        print(f"Total cost = {smcf.optimal_cost()}")
        for arc in range(smcf.num_arcs()):
            # Can ignore arcs leading out of source or into sink.
            if smcf.tail(arc) != source and smcf.head(arc) != sink:

                # Arcs in the solution have a flow value of 1. Their start and end nodes
                # give an assignment of worker to task.
                if smcf.flow(arc) > 0:
                    print(
                        f"Worker {smcf.tail(arc)} assigned to task {smcf.head(arc)}. "
                        f"Cost = {smcf.unit_cost(arc)}"
                    )
    else:
        print("There was an issue with the min cost flow input.")
        print(f"Status: {status}")
    # [END print_solution]


if __name__ == "__main__":
    main()
# [END program]
