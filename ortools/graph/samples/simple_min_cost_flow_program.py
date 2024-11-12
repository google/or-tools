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
"""From Bradley, Hax and Maganti, 'Applied Mathematical Programming', figure 8.1."""
# [START import]
import numpy as np

from ortools.graph.python import min_cost_flow
# [END import]


def main():
    """MinCostFlow simple interface example."""
    # [START solver]
    # Instantiate a SimpleMinCostFlow solver.
    smcf = min_cost_flow.SimpleMinCostFlow()
    # [END solver]

    # [START data]
    # Define four parallel arrays: sources, destinations, capacities,
    # and unit costs between each pair. For instance, the arc from node 0
    # to node 1 has a capacity of 15.
    start_nodes = np.array([0, 0, 1, 1, 1, 2, 2, 3, 4])
    end_nodes = np.array([1, 2, 2, 3, 4, 3, 4, 4, 2])
    capacities = np.array([15, 8, 20, 4, 10, 15, 4, 20, 5])
    unit_costs = np.array([4, 4, 2, 2, 6, 1, 3, 2, 3])

    # Define an array of supplies at each node.
    supplies = [20, 0, 0, -5, -15]
    # [END data]

    # [START constraints]
    # Add arcs, capacities and costs in bulk using numpy.
    all_arcs = smcf.add_arcs_with_capacity_and_unit_cost(
        start_nodes, end_nodes, capacities, unit_costs
    )

    # Add supply for each nodes.
    smcf.set_nodes_supplies(np.arange(0, len(supplies)), supplies)
    # [END constraints]

    # [START solve]
    # Find the min cost flow.
    status = smcf.solve()
    # [END solve]

    # [START print_solution]
    if status != smcf.OPTIMAL:
        print("There was an issue with the min cost flow input.")
        print(f"Status: {status}")
        exit(1)
    print(f"Minimum cost: {smcf.optimal_cost()}")
    print("")
    print(" Arc    Flow / Capacity Cost")
    solution_flows = smcf.flows(all_arcs)
    costs = solution_flows * unit_costs
    for arc, flow, cost in zip(all_arcs, solution_flows, costs):
        print(
            f"{smcf.tail(arc):1} -> "
            f"{smcf.head(arc)}  {flow:3}  / {smcf.capacity(arc):3}       {cost}"
        )
    # [END print_solution]


if __name__ == "__main__":
    main()
# [END program]
