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

"""Simple VRP with special locations which need to be visited at end of the route."""

# [START import]
from typing import Any, Dict

from ortools.constraint_solver.python import constraint_solver
from ortools.routing import enums_pb2
from ortools.routing.python import routing

# [END import]


def create_data_model() -> Dict[str, Any]:
    """Stores the data for the problem."""
    data = {}
    # Special location don't consume token, while regular one consume one
    data["tokens"] = [
        0,  # 0 depot
        0,  # 1 special node
        0,  # 2 special node
        0,  # 3 special node
        0,  # 4 special node
        0,  # 5 special node
        -1,  # 6
        -1,  # 7
        -1,  # 8
        -1,  # 9
        -1,  # 10
        -1,  # 11
        -1,  # 12
        -1,  # 13
        -1,  # 14
        -1,  # 15
        -1,  # 16
        -1,  # 17
        -1,  # 18
    ]
    # just need to be big enough, not a limiting factor
    data["vehicle_tokens"] = [20, 20, 20, 20]
    data["num_vehicles"] = 4
    data["depot"] = 0
    return data


def print_solution(
    manager: routing.IndexManager,
    routing_model: routing.Model,
    solution: constraint_solver.Assignment,
) -> None:
    """Prints solution on console."""
    print(f"Objective: {solution.objective_value()}")
    token_dimension = routing_model.get_dimension_or_die("Token")
    total_distance = 0
    total_token = 0
    for vehicle_id in range(manager.num_vehicles()):
        if not routing_model.is_vehicle_used(solution, vehicle_id):
            continue
        plan_output = f"Route for vehicle {vehicle_id}:\n"
        index = routing_model.start(vehicle_id)
        total_token += solution.value(token_dimension.cumul_var(index))
        route_distance = 0
        while not routing_model.is_end(index):
            node_index = manager.index_to_node(index)
            token_var = token_dimension.cumul_var(index)
            route_token = solution.value(token_var)
            plan_output += f" {node_index} Token({route_token}) -> "
            previous_index = index
            index = solution.value(routing_model.next_var(index))
            route_distance += routing_model.get_arc_cost_for_vehicle(
                previous_index, index, vehicle_id
            )
        node_index = manager.index_to_node(index)
        token_var = token_dimension.cumul_var(index)
        route_token = solution.value(token_var)
        plan_output += f" {node_index} Token({route_token})\n"
        plan_output += f"Distance of the route: {route_distance}m\n"
        total_distance += route_distance
        print(plan_output)
    print(f"Total distance of all routes: {total_distance}m")
    print(f"Total token of all routes: {total_token}")


def main() -> None:
    """Solve the CVRP problem."""
    # Instantiate the data problem.
    data = create_data_model()

    # Create the routing index manager.
    manager = routing.IndexManager(
        len(data["tokens"]), data["num_vehicles"], data["depot"]
    )

    # Create Routing routing.
    routing_model = routing.Model(manager)

    # Create and register a transit callback.
    def distance_callback(from_index: int, to_index: int) -> int:
        """Returns the distance between the two nodes."""
        del from_index
        del to_index
        return 10

    transit_callback_index = routing_model.register_transit_callback(distance_callback)

    routing_model.add_dimension(
        transit_callback_index,
        0,  # null slack
        3000,  # maximum distance per vehicle
        True,  # start cumul to zero
        "distance",
    )
    distance_dimension = routing_model.get_dimension_or_die("distance")
    distance_dimension.set_global_span_cost_coefficient(100)

    # Define cost of each arc.
    routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_callback_index)

    # Add Token constraint.
    def token_callback(from_index: int) -> int:
        """Returns the number of token consumed by the node."""
        # Convert from routing variable Index to tokens NodeIndex.
        from_node = manager.index_to_node(from_index)
        return data["tokens"][from_node]

    token_callback_index = routing_model.register_unary_transit_callback(token_callback)
    routing_model.add_dimension_with_vehicle_capacity(
        token_callback_index,
        0,  # null capacity slack
        data["vehicle_tokens"],  # vehicle maximum tokens
        False,  # start cumul to zero
        "Token",
    )
    # Add constraint: special node can only be visited if token remaining is zero
    token_dimension = routing_model.get_dimension_or_die("Token")
    for node in range(1, 6):
        index = manager.node_to_index(node)
        routing_model.solver.add(token_dimension.cumul_var(index) == 0)

    # Instantiate route start and end times to produce feasible times.
    # [START depot_start_end_times]
    for i in range(manager.num_vehicles()):
        routing_model.add_variable_minimized_by_finalizer(
            token_dimension.cumul_var(routing_model.start(i))
        )
        routing_model.add_variable_minimized_by_finalizer(
            token_dimension.cumul_var(routing_model.end(i))
        )
    # [END depot_start_end_times]

    # Setting first solution heuristic.
    search_parameters = routing.default_routing_search_parameters()
    search_parameters.first_solution_strategy = (
        enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC
    )
    search_parameters.local_search_metaheuristic = (
        enums_pb2.LocalSearchMetaheuristic.GUIDED_LOCAL_SEARCH
    )
    search_parameters.time_limit.seconds = 1

    # Solve the problem.
    solution = routing_model.solve_with_parameters(search_parameters)

    # Print solution on console.
    # [START print_solution]
    if solution:
        print_solution(manager, routing_model, solution)
    else:
        print("No solution found !")
    # [END print_solution]


if __name__ == "__main__":
    main()
