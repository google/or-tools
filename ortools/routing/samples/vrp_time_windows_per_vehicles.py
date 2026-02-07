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
"""Vehicles Routing Problem (VRP) with Time Window (TW) per vehicle.

All time are in minutes using 0am as origin
e.g. 8am = 480, 11am = 660, 1pm = 780 ...

We have 1 depot (0) and 16 locations (1-16).
We have a fleet of 4 vehicles (0-3) whose working time is [480, 1020] (8am-5pm)
We have the distance matrix between these locations and depot.
We have a service time of 25min at each location.

Locations are duplicated so we can simulate a TW per vehicle.
location: [01-16] vehicle: 0 TW: [540, 660] (9am-11am)
location: [17-32] vehicle: 1 TW: [660, 780] (11am-1pm)
location: [33-48] vehicle: 2 TW: [780, 900] (1pm-3pm)
location: [49-64] vehicle: 3 TW: [900, 1020] (3pm-5pm)
"""

# [START import]
from typing import Any, Dict

from ortools.constraint_solver.python import constraint_solver
from ortools.routing import enums_pb2
from ortools.routing.python import routing

# [END import]


# [START data_model]
def create_data_model() -> Dict[str, Any]:
    """Stores the data for the problem."""
    data = {}
    data["time_matrix"] = [
        [0, 6, 9, 8, 7, 3, 6, 2, 3, 2, 6, 6, 4, 4, 5, 9, 7],
        [6, 0, 8, 3, 2, 6, 8, 4, 8, 8, 13, 7, 5, 8, 12, 10, 14],
        [9, 8, 0, 11, 10, 6, 3, 9, 5, 8, 4, 15, 14, 13, 9, 18, 9],
        [8, 3, 11, 0, 1, 7, 10, 6, 10, 10, 14, 6, 7, 9, 14, 6, 16],
        [7, 2, 10, 1, 0, 6, 9, 4, 8, 9, 13, 4, 6, 8, 12, 8, 14],
        [3, 6, 6, 7, 6, 0, 2, 3, 2, 2, 7, 9, 7, 7, 6, 12, 8],
        [6, 8, 3, 10, 9, 2, 0, 6, 2, 5, 4, 12, 10, 10, 6, 15, 5],
        [2, 4, 9, 6, 4, 3, 6, 0, 4, 4, 8, 5, 4, 3, 7, 8, 10],
        [3, 8, 5, 10, 8, 2, 2, 4, 0, 3, 4, 9, 8, 7, 3, 13, 6],
        [2, 8, 8, 10, 9, 2, 5, 4, 3, 0, 4, 6, 5, 4, 3, 9, 5],
        [6, 13, 4, 14, 13, 7, 4, 8, 4, 4, 0, 10, 9, 8, 4, 13, 4],
        [6, 7, 15, 6, 4, 9, 12, 5, 9, 6, 10, 0, 1, 3, 7, 3, 10],
        [4, 5, 14, 7, 6, 7, 10, 4, 8, 5, 9, 1, 0, 2, 6, 4, 8],
        [4, 8, 13, 9, 8, 7, 10, 3, 7, 4, 8, 3, 2, 0, 4, 5, 6],
        [5, 12, 9, 14, 12, 6, 6, 7, 3, 3, 4, 7, 6, 4, 0, 9, 2],
        [9, 10, 18, 6, 8, 12, 15, 8, 13, 9, 13, 3, 4, 5, 9, 0, 9],
        [7, 14, 9, 16, 14, 8, 5, 10, 6, 5, 4, 10, 8, 6, 2, 9, 0],
    ]
    data["num_vehicles"] = 4
    data["depot"] = 0
    return data
    # [END data_model]


# [START solution_printer]
def print_solution(
    manager: routing.IndexManager,
    routing_model: routing.Model,
    assignment: constraint_solver.Assignment,
) -> None:
    """Prints solution on console."""
    print(f"Objective: {assignment.objective_value()}")
    # Display dropped nodes.
    dropped_nodes = "Dropped nodes:"
    for index in range(routing_model.size()):
        if routing_model.is_start(index) or routing_model.is_end(index):
            continue
        if assignment.value(routing_model.next_var(index)) == index:
            node = manager.index_to_node(index)
            if node > 16:
                original = node
                while original > 16:
                    original = original - 16
                dropped_nodes += f" {node}({original})"
            else:
                dropped_nodes += f" {node}"
    print(dropped_nodes)
    # Display routes
    time_dimension = routing_model.get_dimension_or_die("Time")
    total_time = 0
    for vehicle_id in range(manager.num_vehicles()):
        if not routing_model.is_vehicle_used(assignment, vehicle_id):
            continue
        plan_output = f"Route for vehicle {vehicle_id}:\n"
        index = routing_model.start(vehicle_id)
        start_time = 0
        while not routing_model.is_end(index):
            time_var = time_dimension.cumul_var(index)
            node = manager.index_to_node(index)
            if node > 16:
                original = node
                while original > 16:
                    original = original - 16
                plan_output += f"{node}({original})"
            else:
                plan_output += f"{node}"
            plan_output += f" Time:{assignment.value(time_var)} -> "
            if start_time == 0:
                start_time = assignment.value(time_var)
            index = assignment.value(routing_model.next_var(index))
        time_var = time_dimension.cumul_var(index)
        node = manager.index_to_node(index)
        plan_output += f"{node} Time:{assignment.value(time_var)}\n"
        end_time = assignment.value(time_var)
        duration = end_time - start_time
        plan_output += f"Duration of the route:{duration}min\n"
        print(plan_output)
        total_time += duration
    print(f"Total duration of all routes: {total_time}min")
    # [END solution_printer]


def main() -> None:
    """Solve the VRP with time windows."""
    # Instantiate the data problem.
    # [START data]
    data = create_data_model()
    # [END data]

    # Create the routing index manager.
    # [START index_manager]
    manager = routing.IndexManager(
        1 + 16 * 4, data["num_vehicles"], data["depot"]  # number of locations
    )
    # [END index_manager]

    # Create Routing Model.
    # [START routing_model]
    routing_model = routing.Model(manager)

    # [END routing_model]

    # Create and register a transit callback.
    # [START transit_callback]
    def time_callback(from_index: int, to_index: int) -> int:
        """Returns the travel time between the two nodes."""
        # Convert from routing variable Index to time matrix NodeIndex.
        from_node = manager.index_to_node(from_index)
        to_node = manager.index_to_node(to_index)
        # since our matrix is 17x17 map duplicated node to original one to
        # retrieve the travel time
        while from_node > 16:
            from_node = from_node - 16
        while to_node > 16:
            to_node = to_node - 16
        # add service of 25min for each location (except depot)
        service_time = 0
        if from_node != data["depot"]:
            service_time = 25
        return data["time_matrix"][from_node][to_node] + service_time

    transit_callback_index = routing_model.register_transit_callback(time_callback)
    # [END transit_callback]

    # Define cost of each arc.
    # [START arc_cost]
    routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_callback_index)
    # [END arc_cost]

    # Add Time Windows constraint.
    # [START time_windows_constraint]
    time = "Time"
    routing_model.add_dimension(
        transit_callback_index,
        0,  # allow waiting time (0 min)
        1020,  # maximum time per vehicle (9 hours)
        False,  # Don't force start cumul to zero.
        time,
    )
    time_dimension = routing_model.get_dimension_or_die(time)
    # Add time window constraints for each location except depot.
    for location_idx in range(17):
        if location_idx == data["depot"]:
            continue
        # Vehicle 0 location TW: [9am, 11am]
        index_0 = manager.node_to_index(location_idx)
        time_dimension.cumul_var(index_0).set_range(540, 660)
        routing_model.vehicle_var(index_0).set_values([-1, 0])

        # Vehicle 1 location TW: [11am, 1pm]
        index_1 = manager.node_to_index(location_idx + 16 * 1)
        time_dimension.cumul_var(index_1).set_range(660, 780)
        routing_model.vehicle_var(index_1).set_values([-1, 1])

        # Vehicle 2 location TW: [1pm, 3pm]
        index_2 = manager.node_to_index(location_idx + 16 * 2)
        time_dimension.cumul_var(index_2).set_range(780, 900)
        routing_model.vehicle_var(index_2).set_values([-1, 2])

        # Vehicle 3 location TW: [3pm, 5pm]
        index_3 = manager.node_to_index(location_idx + 16 * 3)
        time_dimension.cumul_var(index_3).set_range(900, 1020)
        routing_model.vehicle_var(index_3).set_values([-1, 3])

        # Add Disjunction so only one node among duplicate is visited
        penalty = 100_000  # Give solver strong incentive to visit one node
        routing_model.add_disjunction([index_0, index_1, index_2, index_3], penalty, 1)

    # Add time window constraints for each vehicle start node.
    for vehicle_id in range(data["num_vehicles"]):
        index = routing_model.start(vehicle_id)
        time_dimension.cumul_var(index).set_range(480, 1020)  # (8am, 5pm)

    # Add time window constraints for each vehicle end node.
    for vehicle_id in range(data["num_vehicles"]):
        index = routing_model.end(vehicle_id)
        time_dimension.cumul_var(index).set_range(480, 1020)  # (8am, 5pm)
    # [END time_windows_constraint]

    # Instantiate route start and end times to produce feasible times.
    # [START depot_start_end_times]
    for i in range(data["num_vehicles"]):
        routing_model.add_variable_minimized_by_finalizer(
            time_dimension.cumul_var(routing_model.start(i))
        )
        routing_model.add_variable_minimized_by_finalizer(
            time_dimension.cumul_var(routing_model.end(i))
        )
    # [END depot_start_end_times]

    # Setting first solution heuristic.
    # [START parameters]
    search_parameters = routing.default_routing_search_parameters()
    search_parameters.first_solution_strategy = (
        enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC
    )
    search_parameters.local_search_metaheuristic = (
        enums_pb2.LocalSearchMetaheuristic.GUIDED_LOCAL_SEARCH
    )
    search_parameters.time_limit.seconds = 1
    # [END parameters]

    # Solve the problem.
    # [START solve]
    assignment = routing_model.solve_with_parameters(search_parameters)
    # [END solve]

    # Print solution on console.
    # [START print_solution]
    if assignment:
        print_solution(manager, routing_model, assignment)
    else:
        print("no solution found !")
    # [END print_solution]


if __name__ == "__main__":
    main()
# [END program]
