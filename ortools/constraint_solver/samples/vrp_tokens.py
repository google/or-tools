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
"""Simple VRP with special locations which need to be visited at end of the route."""

# [START import]
from ortools.constraint_solver import routing_enums_pb2
from ortools.constraint_solver import pywrapcp
# [END import]


def create_data_model():
    """Stores the data for the problem."""
    data = {}
    # Special location don't consume token, while regular one consume one
    data['tokens'] = [
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
    data['vehicle_tokens'] = [20, 20, 20, 20]
    data['num_vehicles'] = 4
    data['depot'] = 0
    return data


def print_solution(manager, routing, solution):
    """Prints solution on console."""
    print(f'Objective: {solution.ObjectiveValue()}')
    token_dimension = routing.GetDimensionOrDie('Token')
    total_distance = 0
    total_token = 0
    for vehicle_id in range(manager.GetNumberOfVehicles()):
        plan_output = f'Route for vehicle {vehicle_id}:\n'
        index = routing.Start(vehicle_id)
        total_token += solution.Value(token_dimension.CumulVar(index))
        route_distance = 0
        route_token = 0
        while not routing.IsEnd(index):
            node_index = manager.IndexToNode(index)
            token_var = token_dimension.CumulVar(index)
            route_token = solution.Value(token_var)
            plan_output += f' {node_index} Token({route_token}) -> '
            previous_index = index
            index = solution.Value(routing.NextVar(index))
            route_distance += routing.GetArcCostForVehicle(
                previous_index, index, vehicle_id)
        node_index = manager.IndexToNode(index)
        token_var = token_dimension.CumulVar(index)
        route_token = solution.Value(token_var)
        plan_output += f' {node_index} Token({route_token})\n'
        plan_output += f'Distance of the route: {route_distance}m\n'
        total_distance += route_distance
        print(plan_output)
    print('Total distance of all routes: {}m'.format(total_distance))
    print('Total token of all routes: {}'.format(total_token))


def main():
    """Solve the CVRP problem."""
    # Instantiate the data problem.
    data = create_data_model()

    # Create the routing index manager.
    manager = pywrapcp.RoutingIndexManager(len(data['tokens']),
                                           data['num_vehicles'], data['depot'])

    # Create Routing Model.
    routing = pywrapcp.RoutingModel(manager)

    # Create and register a transit callback.
    def distance_callback(from_index, to_index):
        """Returns the distance between the two nodes."""
        del from_index
        del to_index
        return 10

    transit_callback_index = routing.RegisterTransitCallback(distance_callback)

    routing.AddDimension(
        transit_callback_index,
        0,  # null slack
        3000,  # maximum distance per vehicle
        True,  # start cumul to zero
        'distance')
    distance_dimension = routing.GetDimensionOrDie('distance')
    distance_dimension.SetGlobalSpanCostCoefficient(100)

    # Define cost of each arc.
    routing.SetArcCostEvaluatorOfAllVehicles(transit_callback_index)

    # Add Token constraint.
    def token_callback(from_index):
        """Returns the number of token consumed by the node."""
        # Convert from routing variable Index to tokens NodeIndex.
        from_node = manager.IndexToNode(from_index)
        return data['tokens'][from_node]

    token_callback_index = routing.RegisterUnaryTransitCallback(token_callback)
    routing.AddDimensionWithVehicleCapacity(
        token_callback_index,
        0,  # null capacity slack
        data['vehicle_tokens'],  # vehicle maximum tokens
        False,  # start cumul to zero
        'Token')
    # Add constraint: special node can only be visited if token remaining is zero
    token_dimension = routing.GetDimensionOrDie('Token')
    for node in range(1, 6):
        index = manager.NodeToIndex(node)
        routing.solver().Add(token_dimension.CumulVar(index) == 0)

    # Instantiate route start and end times to produce feasible times.
    # [START depot_start_end_times]
    for i in range(manager.GetNumberOfVehicles()):
        routing.AddVariableMinimizedByFinalizer(
            token_dimension.CumulVar(routing.Start(i)))
        routing.AddVariableMinimizedByFinalizer(
            token_dimension.CumulVar(routing.End(i)))
    # [END depot_start_end_times]

    # Setting first solution heuristic.
    search_parameters = pywrapcp.DefaultRoutingSearchParameters()
    search_parameters.first_solution_strategy = (
        routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC)
    search_parameters.local_search_metaheuristic = (
        routing_enums_pb2.LocalSearchMetaheuristic.GUIDED_LOCAL_SEARCH)
    search_parameters.time_limit.FromSeconds(1)

    # Solve the problem.
    solution = routing.SolveWithParameters(search_parameters)

    # Print solution on console.
    # [START print_solution]
    if solution:
        print_solution(manager, routing, solution)
    else:
        print('No solution found !')
    # [END print_solution]


if __name__ == '__main__':
    main()
