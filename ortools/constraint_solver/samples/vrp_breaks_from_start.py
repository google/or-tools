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
"""Vehicles Routing Problem (VRP) with breaks relative to the vehicle start time.
   Each vehicles start at T:15min, T:30min, T:45min and T:60min respectively.

   Each vehicle must perform a break lasting 5 minutes,
   starting between 25 and 45 minutes after route start.
   e.g. vehicle 2 starting a T:45min must start a 5min breaks
   between [45+25,45+45] i.e. in the range [70, 90].

   Durations are in minutes.
"""

# [START import]
from ortools.constraint_solver import routing_enums_pb2
from ortools.constraint_solver import pywrapcp
# [END import]


# [START data_model]
def create_data_model():
    """Stores the data for the problem."""
    data = {}
    data['num_vehicles'] = 4
    data['depot'] = 0
    data['time_matrix'] = [
        [0, 27, 38, 34, 29, 13, 25, 9, 15, 9, 26, 25, 19, 17, 23, 38, 33],
        [27, 0, 34, 15, 9, 25, 36, 17, 34, 37, 54, 29, 24, 33, 50, 43, 60],
        [38, 34, 0, 49, 43, 25, 13, 40, 23, 37, 20, 63, 58, 56, 39, 77, 37],
        [34, 15, 49, 0, 5, 32, 43, 25, 42, 44, 61, 25, 31, 41, 58, 28, 67],
        [29, 9, 43, 5, 0, 26, 38, 19, 36, 38, 55, 20, 25, 35, 52, 33, 62],
        [13, 25, 25, 32, 26, 0, 11, 15, 9, 12, 29, 38, 33, 31, 25, 52, 35],
        [25, 36, 13, 43, 38, 11, 0, 26, 9, 23, 17, 50, 44, 42, 25, 63, 24],
        [9, 17, 40, 25, 19, 15, 26, 0, 17, 19, 36, 23, 17, 16, 33, 37, 42],
        [15, 34, 23, 42, 36, 9, 9, 17, 0, 13, 19, 40, 34, 33, 16, 54, 25],
        [9, 37, 37, 44, 38, 12, 23, 19, 13, 0, 17, 26, 21, 19, 13, 40, 23],
        [26, 54, 20, 61, 55, 29, 17, 36, 19, 17, 0, 43, 38, 36, 19, 57, 17],
        [25, 29, 63, 25, 20, 38, 50, 23, 40, 26, 43, 0, 5, 15, 32, 13, 42],
        [19, 24, 58, 31, 25, 33, 44, 17, 34, 21, 38, 5, 0, 9, 26, 19, 36],
        [17, 33, 56, 41, 35, 31, 42, 16, 33, 19, 36, 15, 9, 0, 17, 21, 26],
        [23, 50, 39, 58, 52, 25, 25, 33, 16, 13, 19, 32, 26, 17, 0, 38, 9],
        [38, 43, 77, 28, 33, 52, 63, 37, 54, 40, 57, 13, 19, 21, 38, 0, 39],
        [33, 60, 37, 67, 62, 35, 24, 42, 25, 23, 17, 42, 36, 26, 9, 39, 0],
    ]
    # 15 min of service time
    data['service_time'] = [15] * len(data['time_matrix'])
    data['service_time'][data['depot']] = 0
    assert len(data['time_matrix']) == len(data['service_time'])
    return data
    # [END data_model]


# [START solution_printer]
def print_solution(manager, routing, solution):
    """Prints solution on console."""
    print(f'Objective: {solution.ObjectiveValue()}')

    print('Breaks:')
    intervals = solution.IntervalVarContainer()
    for i in range(intervals.Size()):
        brk = intervals.Element(i)
        if brk.PerformedValue() == 1:
            print(f'{brk.Var().Name()}: ' +
                  f'Start({brk.StartValue()}) Duration({brk.DurationValue()})')
        else:
            print(f'{brk.Var().Name()}: Unperformed')

    time_dimension = routing.GetDimensionOrDie('Time')
    total_time = 0
    for vehicle_id in range(manager.GetNumberOfVehicles()):
        index = routing.Start(vehicle_id)
        plan_output = f'Route for vehicle {vehicle_id}:\n'
        while not routing.IsEnd(index):
            time_var = time_dimension.CumulVar(index)
            if routing.IsStart(index):
                start_time = solution.Value(time_var)
            plan_output += f'{manager.IndexToNode(index)} '
            plan_output += f'Time({solution.Value(time_var)}) -> '
            index = solution.Value(routing.NextVar(index))
        time_var = time_dimension.CumulVar(index)
        plan_output += f'{manager.IndexToNode(index)} '
        plan_output += f'Time({solution.Value(time_var)})'
        print(plan_output)
        route_time = solution.Value(time_var) - start_time
        print(f'Time of the route: {route_time}min\n')
        total_time += route_time
    print(f'Total time of all routes: {total_time}min')
    # [END solution_printer]


def main():
    """Solve the VRP with time windows."""
    # Instantiate the data problem.
    # [START data]
    data = create_data_model()
    # [END data]

    # Create the routing index manager.
    # [START index_manager]
    manager = pywrapcp.RoutingIndexManager(
            len(data['time_matrix']),
            data['num_vehicles'],
            data['depot'])
    # [END index_manager]

    # Create Routing Model.
    # [START routing_model]
    routing = pywrapcp.RoutingModel(manager)
    # [END routing_model]


    # Create and register a transit callback.
    # [START transit_callback]
    def time_callback(from_index, to_index):
        """Returns the travel time between the two nodes."""
        # Convert from routing variable Index to time matrix NodeIndex.
        from_node = manager.IndexToNode(from_index)
        to_node = manager.IndexToNode(to_index)
        return data['time_matrix'][from_node][to_node]

    transit_callback_index = routing.RegisterTransitCallback(time_callback)
    # [END transit_callback]

    # Define cost of each arc.
    # [START arc_cost]
    routing.SetArcCostEvaluatorOfAllVehicles(transit_callback_index)
    # [END arc_cost]

    # Add Time Windows constraint.
    time = 'Time'
    routing.AddDimension(
        transit_callback_index,
        10,  # need optional waiting time to place break
        180,  # maximum time per vehicle
        False,  # Don't force start cumul to zero.
        time)
    time_dimension = routing.GetDimensionOrDie(time)
    time_dimension.SetGlobalSpanCostCoefficient(10)

    # Each vehicle start with a 15min delay
    for vehicle_id in range(manager.GetNumberOfVehicles()):
        index = routing.Start(vehicle_id)
        time_dimension.CumulVar(index).SetValue((vehicle_id + 1) * 15)

    # Add breaks
    # [START break_constraint]
    # warning: Need a pre-travel array using the solver's index order.
    node_visit_transit = [0] * routing.Size()
    for index in range(routing.Size()):
        node = manager.IndexToNode(index)
        node_visit_transit[index] = data['service_time'][node]

    # Add a break lasting 5 minutes, start between 25 and 45 minutes after route start
    for v in range(manager.GetNumberOfVehicles()):
        start_var = time_dimension.CumulVar(routing.Start(v))
        break_start = routing.solver().Sum([routing.solver().IntVar(25, 45), start_var])

        break_intervals = [
            routing.solver().FixedDurationIntervalVar(
                break_start, 5, 'Break for vehicle {}'.format(v))
        ]
        time_dimension.SetBreakIntervalsOfVehicle(break_intervals, v, node_visit_transit)
    # [END break_constraint]

    # Setting first solution heuristic.
    # [START parameters]
    search_parameters = pywrapcp.DefaultRoutingSearchParameters()
    search_parameters.first_solution_strategy = (
        routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC)
    search_parameters.local_search_metaheuristic = (
        routing_enums_pb2.LocalSearchMetaheuristic.GUIDED_LOCAL_SEARCH)
    # search_parameters.log_search = True
    search_parameters.time_limit.FromSeconds(2)
    # [END parameters]

    # Solve the problem.
    # [START solve]
    solution = routing.SolveWithParameters(search_parameters)
    # [END solve]

    # Print solution on console.
    # [START print_solution]
    if solution:
        print_solution(manager, routing, solution)
    else:
        print('No solution found !')
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program]
