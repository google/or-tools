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
"""Vehicles Routing Problem (VRP)."""

# [START import]
from ortools.constraint_solver.python import constraint_solver
from ortools.routing import enums_pb2
from ortools.routing import parameters_pb2
from ortools.routing.python import routing

# [END import]


# [START solution_printer]
def print_solution(
    manager: routing.IndexManager,
    routing_model: routing.Model,
    solution: constraint_solver.Assignment,
) -> None:
    """Prints solution on console."""
    print(f"Objective: {solution.objective_value()}")
    max_route_distance = 0
    for vehicle_id in range(manager.num_vehicles()):
        if not routing_model.is_vehicle_used(solution, vehicle_id):
            continue
        index = routing_model.start(vehicle_id)
        plan_output = f"Route for vehicle {vehicle_id}:\n"
        route_distance = 0
        while not routing_model.is_end(index):
            plan_output += f" {manager.index_to_node(index)} -> "
            previous_index = index
            index = solution.value(routing_model.next_var(index))
            route_distance += routing_model.get_arc_cost_for_vehicle(
                previous_index, index, vehicle_id
            )
        plan_output += f"{manager.index_to_node(index)}\n"
        plan_output += f"Distance of the route: {route_distance}m\n"
        print(plan_output)
        max_route_distance = max(route_distance, max_route_distance)
    print(f"Maximum of the route distances: {max_route_distance}m")
    # [END solution_printer]


def main() -> None:
    """Solve the CVRP problem."""
    # Instantiate the data problem.
    # [START data]
    num_locations = 20
    num_vehicles = 5
    depot = 0
    # [END data]

    # Create the routing index manager.
    # [START index_manager]
    manager = routing.IndexManager(num_locations, num_vehicles, depot)
    # [END index_manager]

    # Create Routing Model.
    # [START routing_model]
    routing_model = routing.Model(manager)

    # [END routing_model]

    # Create and register a transit callback.
    # [START transit_callback]
    def distance_callback(from_index: int, to_index: int) -> int:
        # pylint: disable=unused-argument
        """Returns the distance between the two nodes."""
        del from_index
        del to_index
        return 1

    transit_callback_index = routing_model.register_transit_callback(distance_callback)
    # [END transit_callback]

    # Define cost of each arc.
    # [START arc_cost]
    routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_callback_index)
    # [END arc_cost]

    # Add Distance constraint.
    # [START distance_constraint]
    dimension_name = "Distance"
    routing_model.add_dimension(
        transit_callback_index,
        0,  # no slack
        3000,  # vehicle maximum travel distance
        True,  # start cumul to zero
        dimension_name,
    )
    distance_dimension = routing_model.get_dimension_or_die(dimension_name)
    distance_dimension.set_global_span_cost_coefficient(100)
    # [END distance_constraint]

    # Setting first solution heuristic.
    # [START parameters]
    search_parameters: parameters_pb2.RoutingSearchParameters = (
        routing.default_routing_search_parameters()
    )
    search_parameters.first_solution_strategy = (
        enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC
    )
    search_parameters.local_search_metaheuristic = (
        enums_pb2.LocalSearchMetaheuristic.GUIDED_LOCAL_SEARCH
    )
    search_parameters.log_search = True
    search_parameters.time_limit.seconds = 5
    # [END parameters]

    # Solve the problem.
    # [START solve]
    solution = routing_model.solve_with_parameters(search_parameters)
    # [END solve]

    # Print solution on console.
    # [START print_solution]
    if solution:
        print_solution(manager, routing_model, solution)
    # [END print_solution]


if __name__ == "__main__":
    main()
# [END program]
