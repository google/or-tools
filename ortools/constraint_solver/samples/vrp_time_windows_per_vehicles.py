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
from ortools.constraint_solver import routing_enums_pb2
from ortools.constraint_solver import pywrapcp
# [END import]


# [START data_model]
def create_data_model():
    """Stores the data for the problem."""
    data = {}
    data['time_matrix'] = [
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
    data['num_vehicles'] = 4
    data['depot'] = 0
    return data
    # [END data_model]


# [START solution_printer]
def print_solution(manager, routing, assignment):
    """Prints solution on console."""
    print(f'Objective: {assignment.ObjectiveValue()}')
    # Display dropped nodes.
    dropped_nodes = 'Dropped nodes:'
    for index in range(routing.Size()):
        if routing.IsStart(index) or routing.IsEnd(index):
            continue
        if assignment.Value(routing.NextVar(index)) == index:
            node = manager.IndexToNode(index)
            if node > 16:
              original = node
              while original > 16:
                original = original - 16
              dropped_nodes += f' {node}({original})'
            else:
              dropped_nodes += f' {node}'
    print(dropped_nodes)
    # Display routes
    time_dimension = routing.GetDimensionOrDie('Time')
    total_time = 0
    for vehicle_id in range(manager.GetNumberOfVehicles()):
        plan_output = f'Route for vehicle {vehicle_id}:\n'
        index = routing.Start(vehicle_id)
        start_time = 0
        while not routing.IsEnd(index):
            time_var = time_dimension.CumulVar(index)
            node = manager.IndexToNode(index)
            if node > 16:
              original = node
              while original > 16:
                original = original - 16
              plan_output += f'{node}({original})'
            else:
              plan_output += f'{node}'
            plan_output += f' Time:{assignment.Value(time_var)} -> '
            if start_time == 0:
                start_time = assignment.Value(time_var)
            index = assignment.Value(routing.NextVar(index))
        time_var = time_dimension.CumulVar(index)
        node = manager.IndexToNode(index)
        plan_output += f'{node} Time:{assignment.Value(time_var)}\n'
        end_time = assignment.Value(time_var)
        duration = end_time - start_time
        plan_output += f'Duration of the route:{duration}min\n'
        print(plan_output)
        total_time += duration
    print(f'Total duration of all routes: {total_time}min')
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
            1 + 16*4, # number of locations
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
        # since our matrix is 17x17 map duplicated node to original one to
        # retrieve the travel time
        while from_node > 16:
            from_node = from_node - 16;
        while to_node > 16:
            to_node = to_node - 16;
        # add service of 25min for each location (except depot)
        service_time = 0
        if from_node != data['depot']:
            service_time = 25
        return data['time_matrix'][from_node][to_node] + service_time

    transit_callback_index = routing.RegisterTransitCallback(time_callback)
    # [END transit_callback]

    # Define cost of each arc.
    # [START arc_cost]
    routing.SetArcCostEvaluatorOfAllVehicles(transit_callback_index)
    # [END arc_cost]

    # Add Time Windows constraint.
    # [START time_windows_constraint]
    time = 'Time'
    routing.AddDimension(
        transit_callback_index,
        0,  # allow waiting time (0 min)
        1020,  # maximum time per vehicle (9 hours)
        False,  # Don't force start cumul to zero.
        time)
    time_dimension = routing.GetDimensionOrDie(time)
    # Add time window constraints for each location except depot.
    for location_idx in range(17):
        if location_idx == data['depot']:
            continue
        # Vehicle 0 location TW: [9am, 11am]
        index_0 = manager.NodeToIndex(location_idx)
        time_dimension.CumulVar(index_0).SetRange(540, 660)
        routing.VehicleVar(index_0).SetValues([-1, 0])

        # Vehicle 1 location TW: [11am, 1pm]
        index_1 = manager.NodeToIndex(location_idx+16*1)
        time_dimension.CumulVar(index_1).SetRange(660, 780)
        routing.VehicleVar(index_1).SetValues([-1, 1])

        # Vehicle 2 location TW: [1pm, 3pm]
        index_2 = manager.NodeToIndex(location_idx+16*2)
        time_dimension.CumulVar(index_2).SetRange(780, 900)
        routing.VehicleVar(index_2).SetValues([-1, 2])

        # Vehicle 3 location TW: [3pm, 5pm]
        index_3 = manager.NodeToIndex(location_idx+16*3)
        time_dimension.CumulVar(index_3).SetRange(900, 1020)
        routing.VehicleVar(index_3).SetValues([-1, 3])

        # Add Disjunction so only one node among duplicate is visited
        penalty = 100_000 # Give solver strong incentive to visit one node
        routing.AddDisjunction([index_0, index_1, index_2, index_3], penalty, 1)

    # Add time window constraints for each vehicle start node.
    depot_idx = data['depot']
    for vehicle_id in range(data['num_vehicles']):
        index = routing.Start(vehicle_id)
        time_dimension.CumulVar(index).SetRange(480, 1020) # (8am, 5pm)

    # Add time window constraints for each vehicle end node.
    depot_idx = data['depot']
    for vehicle_id in range(data['num_vehicles']):
        index = routing.End(vehicle_id)
        time_dimension.CumulVar(index).SetRange(480, 1020) # (8am, 5pm)
    # [END time_windows_constraint]

    # Instantiate route start and end times to produce feasible times.
    # [START depot_start_end_times]
    for i in range(data['num_vehicles']):
        routing.AddVariableMinimizedByFinalizer(
            time_dimension.CumulVar(routing.Start(i)))
        routing.AddVariableMinimizedByFinalizer(
            time_dimension.CumulVar(routing.End(i)))
    # [END depot_start_end_times]

    # Setting first solution heuristic.
    # [START parameters]
    search_parameters = pywrapcp.DefaultRoutingSearchParameters()
    search_parameters.first_solution_strategy = (
        routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC)
    search_parameters.local_search_metaheuristic = (
        routing_enums_pb2.LocalSearchMetaheuristic.GUIDED_LOCAL_SEARCH)
    search_parameters.time_limit.FromSeconds(1)
    # [END parameters]

    # Solve the problem.
    # [START solve]
    assignment = routing.SolveWithParameters(search_parameters)
    # [END solve]

    # Print solution on console.
    # [START print_solution]
    if assignment:
        print_solution(manager, routing, assignment)
    else:
        print("no solution found !")
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program]
