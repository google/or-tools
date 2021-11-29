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
"""Capacitated Vehicle Routing Problem with Time Windows (CVRPTW).

   This is a sample using the routing library python wrapper to solve a CVRPTW
   problem.
   A description of the problem can be found here:
   http://en.wikipedia.org/wiki/Vehicle_routing_problem.

   Distances are in meters and time in minutes.
"""

# [START import]
import functools
from ortools.constraint_solver import routing_enums_pb2
from ortools.constraint_solver import pywrapcp
# [END import]


# [START data_model]
def create_data_model():
    """Stores the data for the problem."""
    data = {}
    # Locations in block unit
    locations_ = \
            [(4, 4),  # depot
             (2, 0), (8, 0),  # locations to visit
             (0, 1), (1, 1),
             (5, 2), (7, 2),
             (3, 3), (6, 3),
             (5, 5), (8, 5),
             (1, 6), (2, 6),
             (3, 7), (6, 7),
             (0, 8), (7, 8)]
    # Compute locations in meters using the block dimension defined as follow
    # Manhattan average block: 750ft x 264ft -> 228m x 80m
    # here we use: 114m x 80m city block
    # src: https://nyti.ms/2GDoRIe "NY Times: Know Your distance"
    data['locations'] = [(l[0] * 114, l[1] * 80) for l in locations_]
    data['numlocations_'] = len(data['locations'])
    data['time_windows'] = \
          [(0, 0),
           (75, 85), (75, 85),  # 1, 2
           (60, 70), (45, 55),  # 3, 4
           (0, 8), (50, 60),  # 5, 6
           (0, 10), (10, 20),  # 7, 8
           (0, 10), (75, 85),  # 9, 10
           (85, 95), (5, 15),  # 11, 12
           (15, 25), (10, 20),  # 13, 14
           (45, 55), (30, 40)]  # 15, 16
    data['demands'] = \
          [0,  # depot
           1, 1,  # 1, 2
           2, 4,  # 3, 4
           2, 4,  # 5, 6
           8, 8,  # 7, 8
           1, 2,  # 9,10
           1, 2,  # 11,12
           4, 4,  # 13, 14
           8, 8]  # 15, 16
    data['time_per_demand_unit'] = 5  # 5 minutes/unit
    data['num_vehicles'] = 4
    data['breaks'] = [(2, False), (2, False), (2, False), (2, False)]
    data['vehicle_capacity'] = 15
    data['vehicle_speed'] = 83  # Travel speed: 5km/h converted in m/min
    data['depot'] = 0
    return data
    # [END data_model]


def manhattan_distance(position_1, position_2):
    """Computes the Manhattan distance between two points."""
    return (abs(position_1[0] - position_2[0]) +
            abs(position_1[1] - position_2[1]))


def create_distance_evaluator(data):
    """Creates callback to return distance between points."""
    distances_ = {}
    # precompute distance between location to have distance callback in O(1)
    for from_node in range(data['numlocations_']):
        distances_[from_node] = {}
        for to_node in range(data['numlocations_']):
            if from_node == to_node:
                distances_[from_node][to_node] = 0
            else:
                distances_[from_node][to_node] = (manhattan_distance(
                    data['locations'][from_node], data['locations'][to_node]))

    def distance_evaluator(manager, from_node, to_node):
        """Returns the manhattan distance between the two nodes."""
        return distances_[manager.IndexToNode(from_node)][manager.IndexToNode(
            to_node)]

    return distance_evaluator


def create_demand_evaluator(data):
    """Creates callback to get demands at each location."""
    demands_ = data['demands']

    def demand_evaluator(manager, node):
        """Returns the demand of the current node."""
        return demands_[manager.IndexToNode(node)]

    return demand_evaluator


def add_capacity_constraints(routing, data, demand_evaluator_index):
    """Adds capacity constraint."""
    capacity = 'Capacity'
    routing.AddDimension(
        demand_evaluator_index,
        0,  # null capacity slack
        data['vehicle_capacity'],
        True,  # start cumul to zero
        capacity)


def create_time_evaluator(data):
    """Creates callback to get total times between locations."""

    def service_time(data, node):
        """Gets the service time for the specified location."""
        return data['demands'][node] * data['time_per_demand_unit']

    def travel_time(data, from_node, to_node):
        """Gets the travel times between two locations."""
        if from_node == to_node:
            travel_time = 0
        else:
            travel_time = manhattan_distance(
                data['locations'][from_node],
                data['locations'][to_node]) / data['vehicle_speed']
        return travel_time

    total_time_ = {}
    # precompute total time to have time callback in O(1)
    for from_node in range(data['numlocations_']):
        total_time_[from_node] = {}
        for to_node in range(data['numlocations_']):
            if from_node == to_node:
                total_time_[from_node][to_node] = 0
            else:
                total_time_[from_node][to_node] = int(
                    service_time(data, from_node) +
                    travel_time(data, from_node, to_node))

    def time_evaluator(manager, from_node, to_node):
        """Returns the total time between the two nodes."""
        return total_time_[manager.IndexToNode(from_node)][manager.IndexToNode(
            to_node)]

    return time_evaluator


def add_time_window_constraints(routing, manager, data, time_evaluator_index):
    """Add Global Span constraint."""
    time = 'Time'
    horizon = 120
    routing.AddDimension(
        time_evaluator_index,
        horizon,  # allow waiting time
        horizon,  # maximum time per vehicle
        False,  # don't force start cumul to zero
        time)
    time_dimension = routing.GetDimensionOrDie(time)
    # Add time window constraints for each location except depot
    # and 'copy' the slack var in the solution object (aka Assignment) to print it
    for location_idx, time_window in enumerate(data['time_windows']):
        if location_idx == data['depot']:
            continue
        index = manager.NodeToIndex(location_idx)
        time_dimension.CumulVar(index).SetRange(time_window[0], time_window[1])
        routing.AddToAssignment(time_dimension.SlackVar(index))
    # Add time window constraints for each vehicle start node
    # and 'copy' the slack var in the solution object (aka Assignment) to print it
    for vehicle_id in range(data['num_vehicles']):
        index = routing.Start(vehicle_id)
        time_dimension.CumulVar(index).SetRange(data['time_windows'][0][0],
                                                data['time_windows'][0][1])
        routing.AddToAssignment(time_dimension.SlackVar(index))
        # The time window at the end node was impliclty set in the time dimension
        # definition to be [0, horizon].
        # Warning: Slack var is not defined for vehicle end nodes and should not
        # be added to the assignment.


# [START solution_printer]
def print_solution(data, manager, routing, assignment):  # pylint:disable=too-many-locals
    """Prints assignment on console."""
    print('Objective: {}'.format(assignment.ObjectiveValue()))

    print('Breaks:')
    intervals = assignment.IntervalVarContainer()
    for i in range(intervals.Size()):
        brk = intervals.Element(i)
        if brk.PerformedValue() == 1:
            print('{}: Start({}) Duration({})'.format(brk.Var().Name(),
                                                      brk.StartValue(),
                                                      brk.DurationValue()))
        else:
            print('{}: Unperformed'.format(brk.Var().Name()))

    total_distance = 0
    total_load = 0
    total_time = 0
    capacity_dimension = routing.GetDimensionOrDie('Capacity')
    time_dimension = routing.GetDimensionOrDie('Time')
    for vehicle_id in range(data['num_vehicles']):
        index = routing.Start(vehicle_id)
        plan_output = 'Route for vehicle {}:\n'.format(vehicle_id)
        distance = 0
        while not routing.IsEnd(index):
            load_var = capacity_dimension.CumulVar(index)
            time_var = time_dimension.CumulVar(index)
            slack_var = time_dimension.SlackVar(index)
            plan_output += ' {0} Load({1}) Time({2},{3}) Slack({4},{5}) ->'.format(
                manager.IndexToNode(index), assignment.Value(load_var),
                assignment.Min(time_var), assignment.Max(time_var),
                assignment.Min(slack_var), assignment.Max(slack_var))
            previous_index = index
            index = assignment.Value(routing.NextVar(index))
            distance += routing.GetArcCostForVehicle(previous_index, index,
                                                     vehicle_id)
        load_var = capacity_dimension.CumulVar(index)
        time_var = time_dimension.CumulVar(index)
        plan_output += ' {0} Load({1}) Time({2},{3})\n'.format(
            manager.IndexToNode(index), assignment.Value(load_var),
            assignment.Min(time_var), assignment.Max(time_var))
        plan_output += 'Distance of the route: {0}m\n'.format(distance)
        plan_output += 'Load of the route: {}\n'.format(
            assignment.Value(load_var))
        plan_output += 'Time of the route: {}\n'.format(
            assignment.Value(time_var))
        print(plan_output)
        total_distance += distance
        total_load += assignment.Value(load_var)
        total_time += assignment.Value(time_var)
    print('Total Distance of all routes: {0}m'.format(total_distance))
    print('Total Load of all routes: {}'.format(total_load))
    print('Total Time of all routes: {0}min'.format(total_time))
    # [END solution_printer]


def main():
    """Entry point of the program."""
    # Instantiate the data problem.
    # [START data]
    data = create_data_model()
    # [END data]

    # Create the routing index manager
    manager = pywrapcp.RoutingIndexManager(data['numlocations_'],
                                           data['num_vehicles'], data['depot'])

    # Create Routing Model
    routing = pywrapcp.RoutingModel(manager)

    # Define weight of each edge
    distance_evaluator_index = routing.RegisterTransitCallback(
        functools.partial(create_distance_evaluator(data), manager))
    routing.SetArcCostEvaluatorOfAllVehicles(distance_evaluator_index)

    # Add Capacity constraint
    demand_evaluator_index = routing.RegisterUnaryTransitCallback(
        functools.partial(create_demand_evaluator(data), manager))
    add_capacity_constraints(routing, data, demand_evaluator_index)

    # Add Time Window constraint
    time_evaluator_index = routing.RegisterTransitCallback(
        functools.partial(create_time_evaluator(data), manager))
    add_time_window_constraints(routing, manager, data, time_evaluator_index)

    # Add breaks
    time_dimension = routing.GetDimensionOrDie('Time')
    node_visit_transit = {}
    for index in range(routing.Size()):
        node = manager.IndexToNode(index)
        node_visit_transit[index] = int(data['demands'][node] *
                                        data['time_per_demand_unit'])

    break_intervals = {}
    for v in range(data['num_vehicles']):
        vehicle_break = data['breaks'][v]
        break_intervals[v] = [
            routing.solver().FixedDurationIntervalVar(15, 100, vehicle_break[0],
                                                      vehicle_break[1],
                                                      f'Break for vehicle {v}')
        ]
        time_dimension.SetBreakIntervalsOfVehicle(break_intervals[v], v,
                                                  node_visit_transit.values())

    # Setting first solution heuristic (cheapest addition).
    # [START parameters]
    search_parameters = pywrapcp.DefaultRoutingSearchParameters()
    search_parameters.first_solution_strategy = (
        routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC)  # pylint: disable=no-member
    # [END parameters]

    # Solve the problem.
    # [START solve]
    assignment = routing.SolveWithParameters(search_parameters)
    # [END solve]

    # Print solution on console.
    # [START print_solution]
    if assignment:
        print_solution(data, manager, routing, assignment)
    else:
        print('No solution found!')
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program]
