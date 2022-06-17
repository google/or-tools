#!/usr/bin/env python3
# Copyright 2010-2022 Google LLC
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
from ortools.graph.python import min_cost_flow

import numpy as np
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
    smcf.add_arcs_with_capacity_and_unit_cost(start_nodes, end_nodes, capacities, unit_costs)

    # Add node supply.
    for count, supply in enumerate(supplies):
        smcf.set_node_supply(count, supply)
    # [END constraints]

    # [START solve]
    # Find the min cost flow.
    status = smcf.solve()
    # [END solve]

    # [START print_solution]
    if status != smcf.OPTIMAL:
        print('There was an issue with the min cost flow input.')
        print(f'Status: {status}')
        exit(1)
    print('Minimum cost: ', smcf.optimal_cost())
    print('')
    print(' Arc   Flow / Capacity  Cost')
    for i in range(smcf.num_arcs()):
        cost = smcf.flow(i) * smcf.unit_cost(i)
        print(
            '%1s -> %1s    %3s   / %3s   %3s' %
            (smcf.tail(i), smcf.head(i), smcf.flow(i), smcf.capacity(i), cost))
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program]
