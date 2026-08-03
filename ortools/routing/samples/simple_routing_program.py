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
"""Vehicle Routing example."""

# [START import]

from ortools.routing import enums_pb2
from ortools.routing.python import routing

# [END import]


def main() -> None:
    """Entry point of the program."""
    # Instantiate the data problem.
    # [START data]
    num_locations = 5
    num_vehicles = 1
    depot = 0
    # [END data]

    # Create the routing index manager.
    # [START index_manager]
    manager = routing.IndexManager(num_locations, num_vehicles, depot)
    # [END index_manager]

    # Create Routing routing.
    # [START routing_model]
    routing_model = routing.Model(manager)
    # [END routing_model]

    # Create and register a transit callback.
    # [START transit_callback]
    def distance_callback(from_index: int, to_index: int) -> int:
        """Returns the absolute difference between the two nodes."""
        # Convert from routing variable Index to user NodeIndex.
        from_node = int(manager.index_to_node(from_index))
        to_node = int(manager.index_to_node(to_index))
        return abs(to_node - from_node)

    transit_callback_index = routing_model.register_transit_callback(distance_callback)
    # [END transit_callback]

    # Define cost of each arc.
    # [START arc_cost]
    routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_callback_index)
    # [END arc_cost]

    # Setting first solution heuristic.
    # [START parameters]
    search_parameters = routing.default_routing_search_parameters()
    search_parameters.first_solution_strategy = (
        enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC
    )  # pylint: disable=no-member
    # [END parameters]

    # Solve the problem.
    # [START solve]
    assignment = routing_model.solve_with_parameters(search_parameters)
    # [END solve]

    # Print solution on console.
    # [START print_solution]
    print(f"Objective: {assignment.objective_value()}")
    index = routing_model.start(0)
    plan_output = "Route for vehicle 0:\n"
    route_distance = 0
    while not routing_model.is_end(index):
        plan_output += f"{manager.index_to_node(index)} -> "
        previous_index = index
        index = assignment.value(routing_model.next_var(index))
        route_distance += routing_model.get_arc_cost_for_vehicle(
            previous_index, index, 0
        )
    plan_output += f"{manager.index_to_node(index)}\n"
    plan_output += f"Distance of the route: {route_distance}m\n"
    print(plan_output)
    # [END print_solution]


if __name__ == "__main__":
    main()
# [END program]
