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
"""Simple Vehicles Routing Problem (VRP).

This is a sample using the routing library python wrapper to solve a VRP
problem.

The solver stop after improving its solution 15 times or after 5 seconds.

Distances are in meters.
"""

# [START import]
from typing import Any, Dict
import weakref

from ortools.routing import enums_pb2
from ortools.routing import parameters_pb2
from ortools.routing.python import routing

# [END import]


# [START data_model]
def create_data_model() -> Dict[str, Any]:
    """Stores the data for the problem."""
    data = {}
    data["distance_matrix"] = [
        # fmt: off
      [0, 548, 776, 696, 582, 274, 502, 194, 308, 194, 536, 502, 388, 354, 468, 776, 662],
      [548, 0, 684, 308, 194, 502, 730, 354, 696, 742, 1084, 594, 480, 674, 1016, 868, 1210],
      [776, 684, 0, 992, 878, 502, 274, 810, 468, 742, 400, 1278, 1164, 1130, 788, 1552, 754],
      [696, 308, 992, 0, 114, 650, 878, 502, 844, 890, 1232, 514, 628, 822, 1164, 560, 1358],
      [582, 194, 878, 114, 0, 536, 764, 388, 730, 776, 1118, 400, 514, 708, 1050, 674, 1244],
      [274, 502, 502, 650, 536, 0, 228, 308, 194, 240, 582, 776, 662, 628, 514, 1050, 708],
      [502, 730, 274, 878, 764, 228, 0, 536, 194, 468, 354, 1004, 890, 856, 514, 1278, 480],
      [194, 354, 810, 502, 388, 308, 536, 0, 342, 388, 730, 468, 354, 320, 662, 742, 856],
      [308, 696, 468, 844, 730, 194, 194, 342, 0, 274, 388, 810, 696, 662, 320, 1084, 514],
      [194, 742, 742, 890, 776, 240, 468, 388, 274, 0, 342, 536, 422, 388, 274, 810, 468],
      [536, 1084, 400, 1232, 1118, 582, 354, 730, 388, 342, 0, 878, 764, 730, 388, 1152, 354],
      [502, 594, 1278, 514, 400, 776, 1004, 468, 810, 536, 878, 0, 114, 308, 650, 274, 844],
      [388, 480, 1164, 628, 514, 662, 890, 354, 696, 422, 764, 114, 0, 194, 536, 388, 730],
      [354, 674, 1130, 822, 708, 628, 856, 320, 662, 388, 730, 308, 194, 0, 342, 422, 536],
      [468, 1016, 788, 1164, 1050, 514, 514, 662, 320, 274, 388, 650, 536, 342, 0, 764, 194],
      [776, 868, 1552, 560, 674, 1050, 1278, 742, 1084, 810, 1152, 274, 388, 422, 764, 0, 798],
      [662, 1210, 754, 1358, 1244, 708, 480, 856, 514, 468, 354, 844, 730, 536, 194, 798, 0],
        # fmt: on
    ]
    data["num_vehicles"] = 4
    data["depot"] = 0
    return data
    # [END data_model]


# [START solution_callback_printer]
def print_solution(
    routing_manager: routing.IndexManager,
    routing_model: routing.Model,
) -> None:
    """Prints solution on console."""
    print("################")
    print(f"Solution objective: {routing_model.cost_var().value()}")
    total_distance = 0
    for vehicle_id in range(routing_manager.num_vehicles()):
        index = routing_model.start(vehicle_id)
        if routing_model.is_end(routing_model.next_var(index).value()):
            continue
        plan_output = f"Route for vehicle {vehicle_id}:\n"
        route_distance = 0
        while not routing_model.is_end(index):
            plan_output += f" {routing_manager.index_to_node(index)} ->"
            previous_index = index
            index = routing_model.next_var(index).value()
            route_distance += routing_model.get_arc_cost_for_vehicle(
                previous_index, index, vehicle_id
            )
        plan_output += f" {routing_manager.index_to_node(index)}\n"
        plan_output += f"Distance of the route: {route_distance}m\n"
        print(plan_output)
        total_distance += route_distance
    print(f"Total Distance of all routes: {total_distance}m")


# [END solution_callback_printer]


# [START solution_callback]
class SolutionCallback:
    """Create a solution callback."""

    def __init__(
        self,
        manager: routing.IndexManager,
        model: routing.Model,
        limit: int,
    ):
        # We need a weak ref on the routing model to avoid a cycle.
        self._routing_manager_ref = weakref.ref(manager)
        self._routing_model_ref = weakref.ref(model)
        self._counter = 0
        self._counter_limit = limit
        self.objectives = []

    def __call__(self):
        objective = int(
            self._routing_model_ref().cost_var().value()
        )  # pytype: disable=attribute-error
        if not self.objectives or objective < self.objectives[-1]:
            self.objectives.append(objective)
            print_solution(
                self._routing_manager_ref(), self._routing_model_ref()
            )  # pytype: disable=attribute-error
            self._counter += 1
        if self._counter > self._counter_limit:
            self._routing_model_ref().solver.finish_current_search()  # pytype: disable=attribute-error


# [END solution_callback]


def main() -> None:
    """Entry point of the program."""
    # Instantiate the data problem.
    # [START data]
    data = create_data_model()
    # [END data]

    # Create the routing index manager.
    # [START index_manager]
    routing_manager = routing.IndexManager(
        len(data["distance_matrix"]), data["num_vehicles"], data["depot"]
    )
    # [END index_manager]

    # Create Routing Model.
    # [START routing_model]
    routing_model = routing.Model(routing_manager)

    # [END routing_model]

    # Create and register a transit callback.
    # [START transit_callback]
    def distance_callback(from_index: int, to_index: int) -> int:
        """Returns the distance between the two nodes."""
        # Convert from routing variable Index to distance matrix NodeIndex.
        from_node = routing_manager.index_to_node(from_index)
        to_node = routing_manager.index_to_node(to_index)
        return data["distance_matrix"][from_node][to_node]

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

    # Attach a solution callback.
    # [START attach_callback]
    solution_callback = SolutionCallback(routing_manager, routing_model, 15)
    routing_model.add_at_solution_callback(solution_callback)
    # [END attach_callback]

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
    search_parameters.time_limit.FromSeconds(5)
    # [END parameters]

    # Solve the problem.
    # [START solve]
    solution = routing_model.solve_with_parameters(search_parameters)
    # [END solve]

    # Print solution on console.
    # [START print_solution]
    if solution:
        print(f"Best objective: {solution_callback.objectives[-1]}")
    else:
        print("No solution found !")
    # [END print_solution]


if __name__ == "__main__":
    main()
# [END program]
