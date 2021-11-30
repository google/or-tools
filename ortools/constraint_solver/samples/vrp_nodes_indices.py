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
"""Sample to better understand Node/Index relation.

This script generate few markdown tables to better understand
the relation between nodes and indices.

Things to notice:
* Since we have two duplicates (node 5 and node 4) solver need 2 extra indices to have an unique index for each vehicle start/stop and locations.
* Solver needs to "create" an index for a vehicle 1 start since solver need an unique start index per vehicle.
* All end nodes are moved to the end of the index list aka [15, 16, 17, 18].
* routing.Size() return the number of node which are not end nodes (here 15 aka [0-14])
note: using the two properties above, we know that any index in range(routing.Size()) is not a vehicle end node.

* Since end nodes are moved to the end, their respective "empty" node index are
reused so all locations indices are "shifted"
e.g. node 9 is mapped to index 6
* Same for start nodes which are moved to "empty" space
e.g. start node 7 mapped to index 4

Takeaway:
* Allways use routing.Start(), routing.End(), manager.IndexToNode() or manager.NodeToIndex().
* Location node is not necessarily equal to its index.
* To loop through ALL indices use manager.GetNumberOfIndices() (Python) or manager::num_indices() (C++)
"""

from ortools.constraint_solver import routing_enums_pb2
from ortools.constraint_solver import pywrapcp


def main():
    """Entry point of the program."""
    locations = 17
    starts = [5, 5, 7, 8]
    ends = [1, 2, 4, 4]
    vehicles = len(starts)
    assert len(starts) == len(ends)

    manager = pywrapcp.RoutingIndexManager(locations, vehicles, starts, ends)
    routing = pywrapcp.RoutingModel(manager)

    print('Starts/Ends:')
    header = '| |'
    separator = '|---|'
    v_starts = '| start |'
    v_ends = '| end |'
    for v in range(manager.GetNumberOfVehicles()):
        header += f' vehicle {v} |'
        separator += '---|'
        v_starts += f' {starts[v]} |'
        v_ends += f' {ends[v]} |'
    print(header)
    print(separator)
    print(v_starts)
    print(v_ends)

    print('\nNodes:')
    print('| locations | manager.GetNumberOfNodes | manager.GetNumberOfIndices | routing.nodes | routing.Size |')
    print('|---|---|---|---|---|')
    print(f'| {locations} | {manager.GetNumberOfNodes()} | {manager.GetNumberOfIndices()} | {routing.nodes()} | {routing.Size()} |')

    print('\nLocations:')
    print('| node | index | routing.IsStart | routing.IsEnd |')
    print('|---|---|---|---|')
    for node in range(manager.GetNumberOfNodes()):
        if node in starts or node in ends:
            continue
        index = manager.NodeToIndex(node)
        print(
            f'| {node} | {index} | {routing.IsStart(index)} | {routing.IsEnd(index)} |'
        )

    print('\nStart/End:')
    print('| vehicle | Start/end | node | index | routing.IsStart | routing.IsEnd |')
    print('|---|---|---|---|---|---|')
    for v in range(manager.GetNumberOfVehicles()):
        start_index = routing.Start(v)
        start_node = manager.IndexToNode(start_index)
        print(f'| {v} | start | {start_node} | {start_index} | {routing.IsStart(start_index)} | {routing.IsEnd(start_index)} |')
    for v in range(manager.GetNumberOfVehicles()):
        end_index = routing.End(v)
        end_node = manager.IndexToNode(end_index)
        print(f'| {v} | end  | {end_node} | {end_index} | {routing.IsStart(end_index)} | {routing.IsEnd(end_index)} |')


if __name__ == '__main__':
    main()
# [END program]
