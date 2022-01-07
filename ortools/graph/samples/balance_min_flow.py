#!/usr/bin/env python3
# Copyright 2010-2021 Google LLC
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
"""Assignment with teams of workers."""
# [START import]
from ortools.graph import pywrapgraph
# [END import]


def main():
    """Solving an Assignment with teams of worker."""
    # [START solver]
    min_cost_flow = pywrapgraph.SimpleMinCostFlow()
    # [END solver]

    # [START data]
    # Define the directed graph for the flow.
    team_a = [1, 3, 5]
    team_b = [2, 4, 6]

    start_nodes = ([0, 0] + [11, 11, 11] + [12, 12, 12] + [
        1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6
    ] + [7, 8, 9, 10])
    end_nodes = ([11, 12] + team_a + team_b + [
        7, 8, 9, 10, 7, 8, 9, 10, 7, 8, 9, 10, 7, 8, 9, 10, 7, 8, 9, 10, 7, 8,
        9, 10
    ] + [13, 13, 13, 13])
    capacities = ([2, 2] + [1, 1, 1] + [1, 1, 1] + [
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    ] + [1, 1, 1, 1])
    costs = ([0, 0] + [0, 0, 0] + [0, 0, 0] + [
        90, 76, 75, 70, 35, 85, 55, 65, 125, 95, 90, 105, 45, 110, 95, 115, 60,
        105, 80, 75, 45, 65, 110, 95
    ] + [0, 0, 0, 0])

    source = 0
    sink = 13
    tasks = 4
    # Define an array of supplies at each node.
    supplies = [tasks, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -tasks]
    # [END data]

    # [START constraints]
    # Add each arc.
    for i in range(0, len(start_nodes)):
        min_cost_flow.AddArcWithCapacityAndUnitCost(start_nodes[i],
                                                    end_nodes[i], capacities[i],
                                                    costs[i])

    # Add node supplies.
    for i in range(0, len(supplies)):
        min_cost_flow.SetNodeSupply(i, supplies[i])
    # [END constraints]

    # [START solve]
    # Find the minimum cost flow between node 0 and node 10.
    status = min_cost_flow.Solve()
    # [END solve]

    # [START print_solution]
    if status == min_cost_flow.OPTIMAL:
        min_cost_flow.Solve()
        print('Total cost = ', min_cost_flow.OptimalCost())
        print()
        for arc in range(min_cost_flow.NumArcs()):
            # Can ignore arcs leading out of source or intermediate, or into sink.
            if (min_cost_flow.Tail(arc) != source and
                    min_cost_flow.Tail(arc) != 11 and
                    min_cost_flow.Tail(arc) != 12 and
                    min_cost_flow.Head(arc) != sink):

                # Arcs in the solution will have a flow value of 1.
                # There start and end nodes give an assignment of worker to task.
                if min_cost_flow.Flow(arc) > 0:
                    print('Worker %d assigned to task %d.  Cost = %d' %
                          (min_cost_flow.Tail(arc), min_cost_flow.Head(arc),
                           min_cost_flow.UnitCost(arc)))
    else:
        print('There was an issue with the min cost flow input.')
        print(f'Status: {status}')
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program]
