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

# [START program]
"""Assignment with teams of workers."""
# [START import]
from ortools.graph.python import min_cost_flow

# [END import]


def main():
    """Solving an Assignment with teams of worker."""
    # [START solver]
    smcf = min_cost_flow.SimpleMinCostFlow()
    # [END solver]

    # [START data]
    # Define the directed graph for the flow.
    team_a = [1, 3, 5]
    team_b = [2, 4, 6]

    start_nodes = (
        # fmt: off
      [0, 0]
      + [11, 11, 11]
      + [12, 12, 12]
      + [1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6]
      + [7, 8, 9, 10]
        # fmt: on
    )
    end_nodes = (
        # fmt: off
      [11, 12]
      + team_a
      + team_b
      + [7, 8, 9, 10, 7, 8, 9, 10, 7, 8, 9, 10, 7, 8, 9, 10, 7, 8, 9, 10, 7, 8, 9, 10]
      + [13, 13, 13, 13]
        # fmt: on
    )
    capacities = (
        # fmt: off
      [2, 2]
      + [1, 1, 1]
      + [1, 1, 1]
      + [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1]
      + [1, 1, 1, 1]
        # fmt: on
    )
    costs = (
        # fmt: off
      [0, 0]
      + [0, 0, 0]
      + [0, 0, 0]
      + [90, 76, 75, 70, 35, 85, 55, 65, 125, 95, 90, 105, 45, 110, 95, 115, 60, 105, 80, 75, 45, 65, 110, 95]
      + [0, 0, 0, 0]
        # fmt: on
    )

    source = 0
    sink = 13
    tasks = 4
    # Define an array of supplies at each node.
    supplies = [tasks, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -tasks]
    # [END data]

    # [START constraints]
    # Add each arc.
    for i in range(0, len(start_nodes)):
        smcf.add_arc_with_capacity_and_unit_cost(
            start_nodes[i], end_nodes[i], capacities[i], costs[i]
        )

    # Add node supplies.
    for i in range(0, len(supplies)):
        smcf.set_node_supply(i, supplies[i])
    # [END constraints]

    # [START solve]
    # Find the minimum cost flow between node 0 and node 10.
    status = smcf.solve()
    # [END solve]

    # [START print_solution]
    if status == smcf.OPTIMAL:
        print("Total cost = ", smcf.optimal_cost())
        print()
        for arc in range(smcf.num_arcs()):
            # Can ignore arcs leading out of source or intermediate, or into sink.
            if (
                smcf.tail(arc) != source
                and smcf.tail(arc) != 11
                and smcf.tail(arc) != 12
                and smcf.head(arc) != sink
            ):

                # Arcs in the solution will have a flow value of 1.
                # There start and end nodes give an assignment of worker to task.
                if smcf.flow(arc) > 0:
                    print(
                        "Worker %d assigned to task %d.  Cost = %d"
                        % (smcf.tail(arc), smcf.head(arc), smcf.unit_cost(arc))
                    )
    else:
        print("There was an issue with the min cost flow input.")
        print(f"Status: {status}")
    # [END print_solution]


if __name__ == "__main__":
    main()
# [END program]
