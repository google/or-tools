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

"""Capacitated Vehicle Routing Problem (CVRP).

This is a sample using the routing library python wrapper to solve a CVRP
problem while allowing multiple trips, i.e., vehicles can return to a depot
to reset their load ("reload").

A description of the CVRP problem can be found here:
http://en.wikipedia.org/wiki/Vehicle_routing_problem.

Distances are in meters.

In order to implement multiple trips, new nodes are introduced at the same
locations of the original depots. These additional nodes can be dropped
from the schedule at 0 cost.

The max_slack parameter associated to the capacity constraints of all nodes
can be set to be the maximum of the vehicles' capacities, rather than 0 like
in a traditional CVRP. Slack is required since before a solution is found,
it is not known how much capacity will be transferred at the new nodes. For
all the other (original) nodes, the slack is then re-set to 0.

The above two considerations are implemented in `add_capacity_constraints()`.

Last, it is useful to set a large distance between the initial depot and the
new nodes introduced, to avoid schedules having spurious transits through
those new nodes unless it's necessary to reload. This consideration is taken
into account in `create_distance_evaluator()`.
"""

import functools

from ortools.routing.python import routing


###########################
# Problem Data Definition #
###########################
def create_data_model():
    """Stores the data for the problem."""
    data = {}
    _capacity = 15
    # Locations in block unit
    _locations = [
        (4, 4),  # depot
        (4, 4),  # unload depot_first
        (4, 4),  # unload depot_second
        (4, 4),  # unload depot_third
        (4, 4),  # unload depot_fourth
        (4, 4),  # unload depot_fifth
        (2, 0),
        (8, 0),  # locations to visit
        (0, 1),
        (1, 1),
        (5, 2),
        (7, 2),
        (3, 3),
        (6, 3),
        (5, 5),
        (8, 5),
        (1, 6),
        (2, 6),
        (3, 7),
        (6, 7),
        (0, 8),
        (7, 8),
    ]
    # Compute locations in meters using the block dimension defined as follow
    # Manhattan average block: 750ft x 264ft -> 228m x 80m
    # here we use: 114m x 80m city block
    # src: https://nyti.ms/2GDoRIe 'NY Times: Know Your distance'
    data["locations"] = [(l[0] * 114, l[1] * 80) for l in _locations]
    data["num_locations"] = len(data["locations"])
    data["demands"] = [
        0,  # depot
        -_capacity,  # unload depot_first
        -_capacity,  # unload depot_second
        -_capacity,  # unload depot_third
        -_capacity,  # unload depot_fourth
        -_capacity,  # unload depot_fifth
        3,
        3,  # 1, 2
        3,
        4,  # 3, 4
        3,
        4,  # 5, 6
        8,
        8,  # 7, 8
        3,
        3,  # 9,10
        3,
        3,  # 11,12
        4,
        4,  # 13, 14
        8,
        8,
    ]  # 15, 16
    data["time_per_demand_unit"] = 5  # 5 minutes/unit
    data["time_windows"] = [
        (0, 0),  # depot
        (0, 1000),  # unload depot_first
        (0, 1000),  # unload depot_second
        (0, 1000),  # unload depot_third
        (0, 1000),  # unload depot_fourth
        (0, 1000),  # unload depot_fifth
        (75, 850),
        (75, 850),  # 1, 2
        (60, 700),
        (45, 550),  # 3, 4
        (0, 800),
        (50, 600),  # 5, 6
        (0, 1000),
        (10, 200),  # 7, 8
        (0, 1000),
        (75, 850),  # 9, 10
        (85, 950),
        (5, 150),  # 11, 12
        (15, 250),
        (10, 200),  # 13, 14
        (45, 550),
        (30, 400),
    ]  # 15, 16
    data["num_vehicles"] = 3
    data["vehicle_capacity"] = _capacity
    data["vehicle_max_distance"] = 10_000
    data["vehicle_max_time"] = 1_500
    data["vehicle_speed"] = 5 * 60 / 3.6  # Travel speed: 5km/h to convert in m/min
    data["depot"] = 0
    return data


#######################
# Problem Constraints #
#######################
def manhattan_distance(position_1, position_2):
    """Computes the Manhattan distance between two points."""
    return abs(position_1[0] - position_2[0]) + abs(position_1[1] - position_2[1])


def create_distance_evaluator(data):
    """Creates callback to return distance between points."""
    _distances = {}
    # precompute distance between location to have distance callback in O(1)
    for from_node in range(data["num_locations"]):
        _distances[from_node] = {}
        for to_node in range(data["num_locations"]):
            if from_node == to_node:
                _distances[from_node][to_node] = 0
            # Forbid start/end/reload node to be consecutive.
            elif from_node in range(6) and to_node in range(6):
                _distances[from_node][to_node] = data["vehicle_max_distance"]
            else:
                _distances[from_node][to_node] = manhattan_distance(
                    data["locations"][from_node], data["locations"][to_node]
                )

    def distance_evaluator(manager, from_node, to_node):
        """Returns the manhattan distance between the two nodes."""
        return _distances[manager.index_to_node(from_node)][
            manager.index_to_node(to_node)
        ]

    return distance_evaluator


def add_distance_dimension(routing_model, manager, data, distance_evaluator_index):
    """Add Global Span constraint."""
    del manager
    distance = "Distance"
    routing_model.add_dimension(
        distance_evaluator_index,
        0,  # null slack
        data["vehicle_max_distance"],  # maximum distance per vehicle
        True,  # start cumul to zero
        distance,
    )
    distance_dimension = routing_model.get_dimension_or_die(distance)
    # Try to minimize the max distance among vehicles.
    # /!\ It doesn't mean the standard deviation is minimized
    distance_dimension.set_global_span_cost_coefficient(100)


def create_demand_evaluator(data):
    """Creates callback to get demands at each location."""
    _demands = data["demands"]

    def demand_evaluator(manager, from_node):
        """Returns the demand of the current node."""
        return _demands[manager.index_to_node(from_node)]

    return demand_evaluator


def add_capacity_constraints(routing_model, manager, data, demand_evaluator_index):
    """Adds capacity constraint."""
    vehicle_capacity = data["vehicle_capacity"]
    capacity = "Capacity"
    routing_model.add_dimension(
        demand_evaluator_index,
        vehicle_capacity,
        vehicle_capacity,
        True,  # start cumul to zero
        capacity,
    )

    # Add Slack for resetting to zero unload depot nodes.
    # e.g. vehicle with load 10/15 arrives at node 1 (depot unload)
    # so we have CumulVar = 10(current load) + -15(unload) + 5(slack) = 0.
    capacity_dimension = routing_model.get_dimension_or_die(capacity)
    # Allow to drop reloading nodes with zero cost.
    for node in [1, 2, 3, 4, 5]:
        node_index = manager.node_to_index(node)
        routing_model.add_disjunction([node_index], 0)

    # Allow to drop regular node with a cost.
    for node in range(6, len(data["demands"])):
        node_index = manager.node_to_index(node)
        capacity_dimension.slack_var(node_index).set_value(0)
        routing_model.add_disjunction([node_index], 100_000)


def create_time_evaluator(data):
    """Creates callback to get total times between locations."""

    def service_time(data, node):
        """Gets the service time for the specified location."""
        return abs(data["demands"][node]) * data["time_per_demand_unit"]

    def travel_time(data, from_node, to_node):
        """Gets the travel times between two locations."""
        if from_node == to_node:
            travel_time = 0
        else:
            travel_time = (
                manhattan_distance(
                    data["locations"][from_node], data["locations"][to_node]
                )
                / data["vehicle_speed"]
            )
        return travel_time

    _total_time = {}
    # precompute total time to have time callback in O(1)
    for from_node in range(data["num_locations"]):
        _total_time[from_node] = {}
        for to_node in range(data["num_locations"]):
            if from_node == to_node:
                _total_time[from_node][to_node] = 0
            else:
                _total_time[from_node][to_node] = int(
                    service_time(data, from_node)
                    + travel_time(data, from_node, to_node)
                )

    def time_evaluator(manager, from_node, to_node):
        """Returns the total time between the two nodes."""
        return _total_time[manager.index_to_node(from_node)][
            manager.index_to_node(to_node)
        ]

    return time_evaluator


def add_time_window_constraints(routing_model, manager, data, time_evaluator):
    """Add Time windows constraint."""
    time = "Time"
    max_time = data["vehicle_max_time"]
    routing_model.add_dimension(
        time_evaluator,
        max_time,  # allow waiting time
        max_time,  # maximum time per vehicle
        # don't fix start cumul to zero since we are giving TW to start nodes.
        False,
        time,
    )
    time_dimension = routing_model.get_dimension_or_die(time)
    # Add time window constraints for each location except depot
    # and 'copy' the slack var in the solution object (aka Assignment) to print it
    for location_idx, time_window in enumerate(data["time_windows"]):
        if location_idx == 0:
            continue
        index = manager.node_to_index(location_idx)
        time_dimension.cumul_var(index).set_range(time_window[0], time_window[1])
        routing_model.add_to_assignment(time_dimension.slack_var(index))
    # Add time window constraints for each vehicle start node
    # and 'copy' the slack var in the solution object (aka Assignment) to print it
    for vehicle_id in range(data["num_vehicles"]):
        index = routing_model.start(vehicle_id)
        time_dimension.cumul_var(index).set_range(
            data["time_windows"][0][0], data["time_windows"][0][1]
        )
        routing_model.add_to_assignment(time_dimension.slack_var(index))
        # Warning: slack_var is not defined for vehicle's end node
        # routing.add_to_assignment(
        #     time_dimension.slack_var(self.routing.End(vehicle_id)))


###########
# Printer #
###########
def print_solution(
    data, manager, routing_model, assignment
):  # pylint:disable=too-many-locals
    """Prints assignment on console."""
    print(f"Objective: {assignment.objective_value()}")
    total_distance = 0
    total_load = 0
    total_time = 0
    capacity_dimension = routing_model.get_dimension_or_die("Capacity")
    time_dimension = routing_model.get_dimension_or_die("Time")
    distance_dimension = routing_model.get_dimension_or_die("Distance")
    dropped = []
    for order in range(6, routing_model.nodes()):
        index = manager.node_to_index(order)
        if assignment.value(routing_model.next_var(index)) == index:
            dropped.append(order)
    print(f"dropped orders: {dropped}")
    dropped = []
    for reload in range(1, 6):
        index = manager.node_to_index(reload)
        if assignment.value(routing_model.next_var(index)) == index:
            dropped.append(reload)
    print(f"dropped reload stations: {dropped}")

    for vehicle_id in range(data["num_vehicles"]):
        if not routing_model.is_vehicle_used(assignment, vehicle_id):
            continue
        index = routing_model.start(vehicle_id)
        plan_output = f"Route for vehicle {vehicle_id}:\n"
        load_value = 0
        distance = 0
        while not routing_model.is_end(index):
            time_var = time_dimension.cumul_var(index)
            plan_output += (
                f" {manager.index_to_node(index)} "
                f"Load({assignment.min(capacity_dimension.cumul_var(index))}) "
                f"Time({assignment.min(time_var)},{assignment.max(time_var)}) ->"
            )
            previous_index = index
            index = assignment.value(routing_model.next_var(index))
            distance += distance_dimension.get_transit_value(
                previous_index, index, vehicle_id
            )
            # capacity dimension transit_var is negative at reload stations during
            # replenishment. We don't want to consider those values when calculating
            # the total load of the route, hence only considering the positive values.
            load_value += max(
                0,
                capacity_dimension.get_transit_value(previous_index, index, vehicle_id),
            )
        time_var = time_dimension.cumul_var(index)
        plan_output += (
            f" {manager.index_to_node(index)} "
            f"Load({assignment.min(capacity_dimension.cumul_var(index))}) "
            f"Time({assignment.min(time_var)},{assignment.max(time_var)})\n"
        )
        plan_output += f"Distance of the route: {distance}m\n"
        plan_output += f"Load of the route: {load_value}\n"
        plan_output += f"Time of the route: {assignment.min(time_var)}min\n"
        print(plan_output)
        total_distance += distance
        total_load += load_value
        total_time += assignment.min(time_var)

    print(f"Total Distance of all routes: {total_distance}m")
    print(f"Total Load of all routes: {total_load}")
    print(f"Total Time of all routes: {total_time}min")


########
# Main #
########
def main():
    """Entry point of the program."""
    # Instantiate the data problem.
    data = create_data_model()

    # Create the routing index manager
    manager = routing.IndexManager(
        data["num_locations"], data["num_vehicles"], data["depot"]
    )

    # Create Routing Model
    routing_model = routing.Model(manager)

    # Define weight of each edge
    distance_evaluator_index = routing_model.register_transit_callback(
        functools.partial(create_distance_evaluator(data), manager)
    )
    routing_model.set_arc_cost_evaluator_of_all_vehicles(distance_evaluator_index)

    # Add Distance constraint to minimize the longuest route
    add_distance_dimension(routing_model, manager, data, distance_evaluator_index)

    # Add Capacity constraint
    demand_evaluator_index = routing_model.register_unary_transit_callback(
        functools.partial(create_demand_evaluator(data), manager)
    )
    add_capacity_constraints(routing_model, manager, data, demand_evaluator_index)

    # Add Time Window constraint
    time_evaluator_index = routing_model.register_transit_callback(
        functools.partial(create_time_evaluator(data), manager)
    )
    add_time_window_constraints(routing_model, manager, data, time_evaluator_index)

    # Setting first solution heuristic (cheapest addition).
    search_parameters = routing.default_routing_search_parameters()
    search_parameters.first_solution_strategy = (
        routing.FirstSolutionStrategy.PATH_CHEAPEST_ARC
    )  # pylint: disable=no-member
    search_parameters.local_search_metaheuristic = (
        routing.LocalSearchMetaheuristic.GUIDED_LOCAL_SEARCH
    )
    search_parameters.time_limit.seconds = 3

    # Solve the problem.
    solution = routing_model.solve_with_parameters(search_parameters)
    if solution:
        print_solution(data, manager, routing_model, solution)
    else:
        print("No solution found !")


if __name__ == "__main__":
    main()
