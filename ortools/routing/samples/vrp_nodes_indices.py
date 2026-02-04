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
"""Sample to better understand Node/Index relation.

This script generate few markdown tables to better understand
the relation between nodes and indices.

Things to notice:
* Since we have two duplicates (node 5 and node 4) solver need 2 extra indices
to have an unique index for each vehicle start/stop and locations.
* Solver needs to "create" an index for a vehicle 1 start since solver need an
unique start index per vehicle.
* All end nodes are moved to the end of the index list aka [15, 16, 17, 18].
* routing.size() return the number of node which are not end nodes (here 15 aka
[0-14])
note: using the two properties above, we know that any index in
range(routing.size()) is not a vehicle end node.

* Since end nodes are moved to the end, their respective "empty" node index are
reused so all locations indices are "shifted"
e.g. node 9 is mapped to index 6
* Same for start nodes which are moved to "empty" space
e.g. start node 7 mapped to index 4

Takeaway:
* Always use routing.start(), routing.end(), manager.index_to_node() or
manager.node_to_index().
* Location node is not necessarily equal to its index.
* To loop through ALL indices use manager.num_indices() (Python) or
manager::num_indices() (C++)
"""

from ortools.routing.python import routing


def main():
    """Entry point of the program."""
    locations = 17
    starts = [5, 5, 7, 8]
    ends = [1, 2, 4, 4]
    vehicles = len(starts)
    assert len(starts) == len(ends)

    manager = routing.IndexManager(locations, vehicles, starts, ends)
    routing_model = routing.Model(manager)

    print("Starts/Ends:")
    header = "| |"
    separator = "|---|"
    v_starts = "| start |"
    v_ends = "| end |"
    for v in range(manager.num_vehicles()):
        header += f" vehicle {v} |"
        separator += "---|"
        v_starts += f" {starts[v]} |"
        v_ends += f" {ends[v]} |"
    print(header)
    print(separator)
    print(v_starts)
    print(v_ends)

    print("\nNodes:")
    print(
        "| locations | manager.num_nodes | manager.num_indices |"
        " routing.nodes | routing.size |"
    )
    print("|---|---|---|---|---|")
    print(
        f"| {locations} | {manager.num_nodes()} |"
        f" {manager.num_indices()} | {routing_model.nodes()} |"
        f" {routing_model.size()} |"
    )

    print("\nLocations:")
    print("| node | index | routing.is_start | routing.is_end |")
    print("|---|---|---|---|")
    for node in range(manager.num_nodes()):
        if node in starts or node in ends:
            continue
        index = manager.node_to_index(node)
        print(
            f"| {node} | {index} | {routing_model.is_start(index)} |"
            f" {routing_model.is_end(index)} |"
        )

    print("\nStart/End:")
    print(
        "| vehicle | Start/end | node | index | routing.is_start |" " routing.is_end |"
    )
    print("|---|---|---|---|---|---|")
    for v in range(manager.num_vehicles()):
        start_index = routing_model.start(v)
        start_node = manager.index_to_node(start_index)
        print(
            f"| {v} | start | {start_node} | {start_index} |"
            f" {routing_model.is_start(start_index)} |"
            f" {routing_model.is_end(start_index)} |"
        )
    for v in range(manager.num_vehicles()):
        end_index = routing_model.end(v)
        end_node = manager.index_to_node(end_index)
        print(
            f"| {v} | end  | {end_node} | {end_index} |"
            f" {routing_model.is_start(end_index)} |"
            f" {routing_model.is_end(end_index)} |"
        )


if __name__ == "__main__":
    main()
# [END program]
