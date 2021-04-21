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
"""Vehicles Routing Problem (VRP).

Each route as an associated objective cost equal to the max node value along the
road multiply by a constant factor (4200)
"""

# [START import]
from ortools.constraint_solver import routing_enums_pb2
from ortools.constraint_solver import pywrapcp
# [END import]


# [START data_model]
def create_data_model():
    """Stores the data for the problem."""
    data = {}
    data['distance_matrix'] = [
        [
            0, 548, 776, 696, 582, 274, 502, 194, 308, 194, 536, 502, 388, 354,
            468, 776, 662
        ],
        [
            548, 0, 684, 308, 194, 502, 730, 354, 696, 742, 1084, 594, 480,
            674, 1016, 868, 1210
        ],
        [
            776, 684, 0, 992, 878, 502, 274, 810, 468, 742, 400, 1278, 1164,
            1130, 788, 1552, 754
        ],
        [
            696, 308, 992, 0, 114, 650, 878, 502, 844, 890, 1232, 514, 628,
            822, 1164, 560, 1358
        ],
        [
            582, 194, 878, 114, 0, 536, 764, 388, 730, 776, 1118, 400, 514,
            708, 1050, 674, 1244
        ],
        [
            274, 502, 502, 650, 536, 0, 228, 308, 194, 240, 582, 776, 662, 628,
            514, 1050, 708
        ],
        [
            502, 730, 274, 878, 764, 228, 0, 536, 194, 468, 354, 1004, 890,
            856, 514, 1278, 480
        ],
        [
            194, 354, 810, 502, 388, 308, 536, 0, 342, 388, 730, 468, 354, 320,
            662, 742, 856
        ],
        [
            308, 696, 468, 844, 730, 194, 194, 342, 0, 274, 388, 810, 696, 662,
            320, 1084, 514
        ],
        [
            194, 742, 742, 890, 776, 240, 468, 388, 274, 0, 342, 536, 422, 388,
            274, 810, 468
        ],
        [
            536, 1084, 400, 1232, 1118, 582, 354, 730, 388, 342, 0, 878, 764,
            730, 388, 1152, 354
        ],
        [
            502, 594, 1278, 514, 400, 776, 1004, 468, 810, 536, 878, 0, 114,
            308, 650, 274, 844
        ],
        [
            388, 480, 1164, 628, 514, 662, 890, 354, 696, 422, 764, 114, 0,
            194, 536, 388, 730
        ],
        [
            354, 674, 1130, 822, 708, 628, 856, 320, 662, 388, 730, 308, 194,
            0, 342, 422, 536
        ],
        [
            468, 1016, 788, 1164, 1050, 514, 514, 662, 320, 274, 388, 650, 536,
            342, 0, 764, 194
        ],
        [
            776, 868, 1552, 560, 674, 1050, 1278, 742, 1084, 810, 1152, 274,
            388, 422, 764, 0, 798
        ],
        [
            662, 1210, 754, 1358, 1244, 708, 480, 856, 514, 468, 354, 844, 730,
            536, 194, 798, 0
        ],
    ]
    data['value'] = [
        0,  # depot
        42,  # 1
        42,  # 2
        8,  # 3
        8,  # 4
        8,  # 5
        8,  # 6
        8,  # 7
        8,  # 8
        8,  # 9
        8,  # 10
        8,  # 11
        8,  # 12
        8,  # 13
        8,  # 14
        42,  # 15
        42,  # 16
    ]
    assert len(data['distance_matrix']) == len(data['value'])
    data['num_vehicles'] = 4
    data['depot'] = 0
    return data
# [END data_model]


# [START solution_printer]
def print_solution(data, manager, routing, solution):
    """Prints solution on console."""
    print(f'Objective: {solution.ObjectiveValue()}')
    max_route_distance = 0
    dim_one = routing.GetDimensionOrDie('One')
    dim_two = routing.GetDimensionOrDie('Two')

    for vehicle_id in range(data['num_vehicles']):
        index = routing.Start(vehicle_id)
        plan_output = 'Route for vehicle {}:\n'.format(vehicle_id)
        route_distance = 0
        while not routing.IsEnd(index):
            one_var = dim_one.CumulVar(index)
            one_slack_var = dim_one.SlackVar(index)
            two_var = dim_two.CumulVar(index)
            two_slack_var = dim_two.SlackVar(index)
            plan_output += ' N:{0} one:({1},{2}) two:({3},{4}) -> '.format(
                manager.IndexToNode(index), solution.Value(one_var),
                solution.Value(one_slack_var), solution.Value(two_var),
                solution.Value(two_slack_var))
            previous_index = index
            index = solution.Value(routing.NextVar(index))
            route_distance += routing.GetArcCostForVehicle(
                previous_index, index, vehicle_id)
        one_var = dim_one.CumulVar(index)
        two_var = dim_two.CumulVar(index)
        plan_output += 'N:{0} one:{1} two:{2}\n'.format(
            manager.IndexToNode(index), solution.Value(one_var),
            solution.Value(two_var))
        plan_output += 'Distance of the route: {}m\n'.format(route_distance)
        print(plan_output)
        max_route_distance = max(route_distance, max_route_distance)
    print('Maximum of the route distances: {}m'.format(max_route_distance))
# [END solution_printer]


def main():
    """Solve the CVRP problem."""
    # Instantiate the data problem.
    # [START data]
    data = create_data_model()
    # [END data]

    # Create the routing index manager.
    # [START index_manager]
    manager = pywrapcp.RoutingIndexManager(
            len(data['distance_matrix']),
            data['num_vehicles'],
            data['depot'])
    # [END index_manager]

    # Create Routing Model.
    # [START routing_model]
    routing = pywrapcp.RoutingModel(manager)
    # [END routing_model]

    # Create and register a transit callback.
    # [START transit_callback]
    def distance_callback(from_index, to_index):
        """Returns the distance between the two nodes."""
        # Convert from routing variable Index to distance matrix NodeIndex.
        from_node = manager.IndexToNode(from_index)
        to_node = manager.IndexToNode(to_index)
        return data['distance_matrix'][from_node][to_node]

    transit_callback_index = routing.RegisterTransitCallback(distance_callback)
    # [END transit_callback]

    # Define cost of each arc.
    # [START arc_cost]
    routing.SetArcCostEvaluatorOfAllVehicles(transit_callback_index)
    # [END arc_cost]

    # Add Distance constraint.
    # [START distance_constraint]
    dimension_name = 'Distance'
    routing.AddDimension(
        transit_callback_index,
        0,  # no slack
        3_000,  # vehicle maximum travel distance
        True,  # start cumul to zero
        dimension_name)
    distance_dimension = routing.GetDimensionOrDie(dimension_name)
    distance_dimension.SetGlobalSpanCostCoefficient(10)
    # [END distance_constraint]

    # Max Node value Constraint.
    # Dimension One will be used to compute the max node value up to the node in
    # the route and store the result in the SlackVar of the node.
    routing.AddConstantDimensionWithSlack(
        0,  # transit 0
        42 * 16,  # capacity: be able to store PEAK*ROUTE_LENGTH in worst case
        42,  # slack_max: to be able to store peak in slack
        True,  #  Fix StartCumulToZero not really matter here
        'One')
    dim_one = routing.GetDimensionOrDie('One')

    # Dimension Two will be used to store the max node value in the route end node
    # CumulVar so we can use it as an objective cost.
    routing.AddConstantDimensionWithSlack(
        0,  # transit 0
        42 * 16,  # capacity: be able to have PEAK value in CumulVar(End)
        42,  # slack_max: to be able to store peak in slack
        True,  #  Fix StartCumulToZero YES here
        'Two')
    dim_two = routing.GetDimensionOrDie('Two')

    # force depot Slack to be value since we don't have any predecessor...
    # Slack(Depot) = value(Depot)
    for v in range(manager.GetNumberOfVehicles()):
        start = routing.Start(v)
        dim_one.SlackVar(start).SetValue(data['value'][0])
        routing.AddToAssignment(dim_one.SlackVar(start))

        dim_two.SlackVar(start).SetValue(data['value'][0])
        routing.AddToAssignment(dim_two.SlackVar(start))

    # Step by step relation
    # Slack(N) = max( Slack(N-1) , value(N) )
    solver = routing.solver()
    for node in range(1, 17):
        index = manager.NodeToIndex(node)
        routing.AddToAssignment(dim_one.SlackVar(index))
        routing.AddToAssignment(dim_two.SlackVar(index))
        test = []
        for v in range(manager.GetNumberOfVehicles()):
            previous_index = routing.Start(v)
            cond = routing.NextVar(previous_index) == index
            value = solver.Max(dim_one.SlackVar(previous_index),
                               data['value'][node])
            test.append((cond * value).Var())
        for previous in range(1, 17):
            previous_index = manager.NodeToIndex(previous)
            cond = routing.NextVar(previous_index) == index
            value = solver.Max(dim_one.SlackVar(previous_index),
                               data['value'][node])
            test.append((cond * value).Var())
        solver.Add(solver.Sum(test) == dim_one.SlackVar(index))

    # relation between dimensions, copy last node Slack from dim ONE to dim TWO
    for node in range(1, 17):
        index = manager.NodeToIndex(node)
        values = []
        for v in range(manager.GetNumberOfVehicles()):
            next_index = routing.End(v)
            cond = routing.NextVar(index) == next_index
            value = dim_one.SlackVar(index)
            values.append((cond * value).Var())
        solver.Add(solver.Sum(values) == dim_two.SlackVar(index))

    # Should force all others dim_two slack var to zero...
    for v in range(manager.GetNumberOfVehicles()):
        end = routing.End(v)
        dim_two.SetCumulVarSoftUpperBound(end, 0, 4200)

    # Setting first solution heuristic.
    # [START parameters]
    search_parameters = pywrapcp.DefaultRoutingSearchParameters()
    search_parameters.first_solution_strategy = (
        routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC)
    search_parameters.local_search_metaheuristic = (
        routing_enums_pb2.LocalSearchMetaheuristic.GUIDED_LOCAL_SEARCH)
    # search_parameters.log_search = True
    search_parameters.time_limit.FromSeconds(5)
    # [END parameters]

    # Solve the problem.
    # [START solve]
    solution = routing.SolveWithParameters(search_parameters)
    # [END solve]

    # Print solution on console.
    # [START print_solution]
    if solution:
        print_solution(data, manager, routing, solution)
    else:
        print('No solution found !')
    # [END print_solution]


if __name__ == '__main__':
    main()
    # [END program]
