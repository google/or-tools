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
"""Simple Travelling Salesman Problem.

A description of the problem can be found here:
http://en.wikipedia.org/wiki/Travelling_salesperson_problem.
"""

# [START import]
from ortools.routing import enums_pb2
from ortools.routing import parameters_pb2
from ortools.routing.python import model

FirstSolutionStrategy = enums_pb2.FirstSolutionStrategy
RoutingSearchStatus = enums_pb2.RoutingSearchStatus
RoutingSearchParameters = parameters_pb2.RoutingSearchParameters

# [END import]


# [START data_model]
def create_data_model():
    """Stores the data for the problem."""
    data = {}
    # Locations in block units
    locations = [
        # fmt:off
      (4, 4),  # depot
      (2, 0), (8, 0),  # locations to visit
      (0, 1), (1, 1),
      (5, 2), (7, 2),
      (3, 3), (6, 3),
      (5, 5), (8, 5),
      (1, 6), (2, 6),
      (3, 7), (6, 7),
      (0, 8), (7, 8)
        # fmt:on
    ]
    # Convert locations in meters using a city block dimension of 114m x 80m.
    data["locations"] = [(l[0] * 114, l[1] * 80) for l in locations]
    data["num_vehicles"] = 1
    data["depot"] = 0
    return data
    # [END data_model]


# [START distance_callback]
def create_distance_callback(data, manager):
    """Creates callback to return distance between points."""
    distances_ = {}
    index_manager_ = manager
    # precompute distance between location to have distance callback in O(1)
    for from_counter, from_node in enumerate(data["locations"]):
        distances_[from_counter] = {}
        for to_counter, to_node in enumerate(data["locations"]):
            if from_counter == to_counter:
                distances_[from_counter][to_counter] = 0
            else:
                distances_[from_counter][to_counter] = abs(
                    from_node[0] - to_node[0]
                ) + abs(from_node[1] - to_node[1])

    def distance_callback(from_index, to_index):
        """Returns the manhattan distance between the two nodes."""
        # Convert from routing variable Index to distance matrix NodeIndex.
        from_node = index_manager_.index_to_node(from_index)
        to_node = index_manager_.index_to_node(to_index)
        return distances_[from_node][to_node]

    return distance_callback
    # [END distance_callback]


# [START solution_printer]
def print_solution(manager, routing, solution):
    """Prints assignment on console."""
    status = routing.status()
    print(f"Status: {RoutingSearchStatus.Value.Name(status)}")
    if (
        status != RoutingSearchStatus.ROUTING_OPTIMAL
        and status != RoutingSearchStatus.ROUTING_SUCCESS
    ):
        print("No solution found!")
        return
    print(f"Objective: {solution.objective_value()}")
    index = routing.start(0)
    plan_output = "Route for vehicle 0:\n"
    route_distance = 0
    while not routing.is_end(index):
        plan_output += f" {manager.index_to_node(index)} ->"
        previous_index = index
        index = solution.value(routing.next_var(index))
        route_distance += routing.get_arc_cost_for_vehicle(previous_index, index, 0)
    plan_output += f" {manager.index_to_node(index)}\n"
    plan_output += f"Distance of the route: {route_distance}m\n"
    print(plan_output)
    # [END solution_printer]


def main():
    """Entry point of the program."""
    # Instantiate the data problem.
    # [START data]
    data = create_data_model()
    # [END data]

    # Create the routing index manager.
    # [START index_manager]
    manager = model.RoutingIndexManager(
        len(data["locations"]), data["num_vehicles"], data["depot"]
    )
    # [END index_manager]

    # Create Routing Model.
    # [START model]
    routing = model.RoutingModel(manager)
    # [END model]

    # Create and register a transit callback.
    # [START transit_callback]
    distance_callback = create_distance_callback(data, manager)
    transit_callback_index = routing.register_transit_callback(distance_callback)
    # [END transit_callback]

    # Define cost of each arc.
    # [START arc_cost]
    routing.set_arc_cost_evaluator_of_all_vehicles(transit_callback_index)
    # [END arc_cost]

    # Setting first solution heuristic.
    # [START parameters]
    search_parameters = model.default_routing_search_parameters()
    search_parameters.first_solution_strategy = FirstSolutionStrategy.PATH_CHEAPEST_ARC
    # [END parameters]

    # Solve the problem.
    # [START solve]
    #solution = routing.solve()
    solution = routing.solve_with_parameters(search_parameters)
    # [END solve]

    # Print solution on console.
    # [START print_solution]
    print_solution(manager, routing, solution)
    # [END print_solution]


if __name__ == "__main__":
    main()
# [END program]
