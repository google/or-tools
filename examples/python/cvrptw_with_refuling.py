# This Python file uses the following encoding: utf-8
# Copyright 2015 Tin Arm Engineering AB
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

"""Capacitated Vehicle Routing Problem with Time Windows (and optional orders).

   This is a sample using the routing library python wrapper to solve a
   CVRPTW problem.
   A description of the problem can be found here:
   http://en.wikipedia.org/wiki/Vehicle_routing_problem.
   The variant which is tackled by this model includes a capacity dimension,
   time windows and optional orders, with a penalty cost if orders are not
   performed.
   
   Capacitated Vehicle Routing Problem with Time Windows and refueling
   constraints.
   This is an extension to the model in cvrptw.cc so refer to that file for
   more information on the common part of the model. The model implemented here
   takes into account refueling constraints using a specific dimension: vehicles
   must visit certain nodes (refueling nodes) before the quantity of fuel
   reaches zero. Fuel consumption is proportional to the distance traveled.

   The optimization engine uses local search to improve solutions, first
   solutions being generated using a cheapest addition heuristic.


"""
from __future__ import print_function
import math
from ortools.constraint_solver import pywrapcp
from ortools.constraint_solver import routing_enums_pb2


def distance(x1, y1, x2, y2):
    # Manhattan distance
    dist = abs(x1 - x2) + abs(y1 - y2)

    return dist


# Distance callback
class CreateDistanceCallback(object):
    """Create callback to calculate distances and travel times between points.
    """

    def __init__(self, locations):
        """Initialize distance array."""
        num_locations = len(locations)
        self.matrix = {}

        for from_node in range(num_locations):
            self.matrix[from_node] = {}
            for to_node in range(num_locations):
                if from_node == to_node:
                    self.matrix[from_node][to_node] = 0
                else:
                    x1 = locations[from_node][0]
                    y1 = locations[from_node][1]
                    x2 = locations[to_node][0]
                    y2 = locations[to_node][1]
                    self.matrix[from_node][to_node] = distance(x1, y1, x2, y2)

    def Distance(self, from_node, to_node):
        return self.matrix[from_node][to_node]

    def NegDistance(self, from_node, to_node):
        return -self.matrix[from_node][to_node]

    def MaxDistance(self):
        maxdist = 0
        for from_node in self.matrix:
            for to_node in self.matrix[from_node]:
                dist = self.Distance(from_node, to_node)
                if dist > maxdist:
                    maxdist = dist
        return(maxdist)


# Demand callback
class CreateDemandCallback(object):
    """Create callback to get demands at location node."""

    def __init__(self, demands):
        self.matrix = demands

    def Demand(self, from_node, to_node):
        return self.matrix[from_node]


# Service time (proportional to demand) callback.
class CreateServiceTimeCallback(object):
    """Create callback to get time windows at each location."""

    def __init__(self, demands, time_per_demand_unit):
        self.matrix = demands
        self.time_per_demand_unit = time_per_demand_unit

    def ServiceTime(self, from_node, to_node):
        return self.matrix[from_node] * self.time_per_demand_unit


# Create total_time callback (equals service time plus travel time).
class CreateTotalTimeCallback(object):
    def __init__(self, service_time_callback, dist_callback, speed):
        self.service_time_callback = service_time_callback
        self.dist_callback = dist_callback
        self.speed = speed

    def TotalTime(self, from_node, to_node):
        service_time = self.service_time_callback(from_node, to_node)
        travel_time = self.dist_callback(from_node, to_node) / self.speed
        return service_time + travel_time


def IsRefuelNode(node):
    """
    Returns true if node is a refueling node,
    based on node / refuel node ratio.
    """
    refuel_node_ratio = 5
    return(node % refuel_node_ratio == 0)


def DisplayPlan(routing, assignment):

    # Display dropped orders.
    dropped = ''
    plan_output = 'Plan: '
    for order in range(1, routing.nodes()):
        if assignment.Value(routing.NextVar(order)) == order:
            if len(dropped) == 0:
                dropped += "{0}[{1}]".format(order, IsRefuelNode(order))
            else:
                dropped += ", {0}[{1}]".format(order, IsRefuelNode(order))

    if len(dropped):
        plan_output += "Dropped orders:" + dropped + "\n"

    return plan_output


def main():

    # Create the data.
    locations, demands, start_times = create_data_array()
    # In the case of refuling nodes, we may not want them to also have demands,
    # in which case zero them out.
    demands = [dem * (1 - IsRefuelNode(node)) for node, dem in
               enumerate(demands)]

    num_locations = len(locations)
    # Set the depot node
    depot = 0
    demands[depot] = 0
    # Set the number of vehicles.
    num_vehicles = 4
    search_time_limit = 40000

    # Create routing model.
    if num_locations > 0:
        # The number of nodes of the VRP is num_locations.
        # Nodes are indexed from 0 to num_locations - 1. By default the start
        # of a route is node 0.
        routing = pywrapcp.RoutingModel(num_locations, num_vehicles)
        search_parameters = pywrapcp.RoutingModel.DefaultSearchParameters()

        # Setting first solution heuristic: the
        # method for finding a first solution to the problem.
        search_parameters.first_solution_strategy = (
            routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC)

        # The 'PATH_CHEAPEST_ARC' method does the following:
        # Starting from a route "start" node, connect it to the node which
        # produces the cheapest route segment, then extend the route by
        # iterating on the last node added to the route.

        # Set the depot.
        routing.SetDepot(depot)

        # Put callbacks to the distance function and travel time functions
        # here.
        dist_between_locations = CreateDistanceCallback(locations)
        dist_callback = dist_between_locations.Distance

        routing.SetArcCostEvaluatorOfAllVehicles(dist_callback)

        demands_at_locations = CreateDemandCallback(demands)
        demands_callback = demands_at_locations.Demand

        # Adding capacity dimension constraints.
        VehicleCapacity = 150
        NullCapacitySlack = 0
        fix_start_cumul_to_zero = True
        capacity = "Capacity"

        routing.AddDimension(demands_callback,
                             NullCapacitySlack,
                             VehicleCapacity,
                             fix_start_cumul_to_zero,
                             capacity)

        # Adding time dimension constraints.
        time_per_demand_unit = 300
        horizon = 24 * 3600
        time = "Time"
        tw_duration = 4 * 3600
        speed = 10

        service_times = CreateServiceTimeCallback(demands,
                                                  time_per_demand_unit)
        service_time_callback = service_times.ServiceTime

        total_times = CreateTotalTimeCallback(service_time_callback,
                                              dist_callback,
                                              speed)

        total_time_callback = total_times.TotalTime

        # Add a dimension for time-window constraints and limits on the start
        # times and end times.
        routing.AddDimension(total_time_callback,  # total time function callbk
                             horizon,
                             horizon,
                             fix_start_cumul_to_zero,
                             time)

        # Add limit on size of the time windows.
        time_dimension = routing.GetDimensionOrDie(time)

        for order in range(0, routing.Size()):
            if not routing.IsStart(order):
                start = start_times[order]
                time_dimension.CumulVar(order).SetRange(start,
                                                        start + tw_duration)
            elif IsRefuelNode(order):
                # The refuling nodes can be visited any time.
                time_dimension.CumulVar(order).SetRange(0, 24 * 3600)

        # Adding fuel dimension. This dimension consumes a quantity equal to
        # the distance traveled. Only refuel nodes can make the quantity of
        # dimension increase by letting slack variable replenish the fuel.

        fuel_capacity = dist_between_locations.MaxDistance()
        fuel_capacity = 200 
        negdist_callback = dist_between_locations.NegDistance

        routing.AddDimension(negdist_callback,  # negative dist callback
                             fuel_capacity,
                             fuel_capacity,
                             False, # fix_start_cumul_to_zero
                             "Fuel")
        # Only let slack free for refueling nodes.
        fuel_dimension = routing.GetDimensionOrDie("Fuel")

        for order in range(0, routing.Size()):
            if (not IsRefuelNode(order) or routing.IsStart(order)):
                fuel_dimension.SlackVar(order).SetValue(0)
            # Don't add disjunctions to start nodes.
            if not routing.IsStart(order):
                # Allow the refuling nodes to be skipped with no penalty
                if IsRefuelNode(order):
                    routing.AddDisjunction([order], 0)
                else:
                    routing.AddDisjunction([order], 5000)

            # Needed to instantiate fuel quantity at each node.
            routing.AddVariableMinimizedByFinalizer(fuel_dimension.CumulVar(order))
      
        # To allow the skipping of refuling nodes, we add disjunctions to all
        # nodes. Each disjunction is a list of 1 index, which allows that customer to
        # be active or not, with a penalty if not. The penalty should be larger
        # than the cost of servicing that customer, or it will always be dropped!

        # Solve displays a solution if any.
        assignment = routing.SolveWithParameters(search_parameters)
        if assignment:
            # data = create_data_array()
            # locations = data[0]
            # demands = [dem * (1 - IsRefuelNode(node))  for node, dem in enumerate(data[1])]
            # print(demands)

            # start_times = data[2]
            # size = len(locations)
            # Solution cost.
            print("Total distance of all routes: " + str(assignment.ObjectiveValue()) + "\n")
            plan_output = DisplayPlan(routing, assignment)
            print(plan_output)
            # Inspect solution.
            capacity_dimension = routing.GetDimensionOrDie(capacity)
            time_dimension = routing.GetDimensionOrDie(time)
            fuel_dimension = routing.GetDimensionOrDie("Fuel")

            for vehicle_nbr in range(num_vehicles):
                index = routing.Start(vehicle_nbr)
                plan_output = 'Route {0}:'.format(vehicle_nbr)

                while not routing.IsEnd(index):
                    node_index = routing.IndexToNode(index)
                    load_var = capacity_dimension.CumulVar(index)
                    time_var = time_dimension.CumulVar(index)
                    fuel_var = fuel_dimension.CumulVar(index)

                    #print(assignment.Value(fuel_transit_var))
                    plan_output += \
                            " {node_index} Load({load}) Time({tmin}, {tmax}) Fuel({fuel})  {fn}-> ".format(
                                node_index=node_index,
                                load=assignment.Value(load_var),
                                tmin=str(assignment.Min(time_var)),
                                tmax=str(assignment.Max(time_var)),
                                fuel=assignment.Value(fuel_var),
                                fn=IsRefuelNode(node_index))
                    index = assignment.Value(routing.NextVar(index))

                node_index = routing.IndexToNode(index)
                load_var = capacity_dimension.CumulVar(index)
                time_var = time_dimension.CumulVar(index)
                fuel_var = fuel_dimension.CumulVar(index)
                plan_output += \
                          " {node_index} Load({load}) Time({tmin}, {tmax}) Fuel({fuel})".format(
                              node_index=node_index,
                              load=assignment.Value(load_var),
                              tmin=str(assignment.Min(time_var)),
                              tmax=str(assignment.Max(time_var)),
                              fuel=assignment.Value(fuel_var))
                print(plan_output)
                print("\n")
        else:
            print('No solution found.')
    else:
        print('Specify an instance greater than 0.')


def create_data_array():

    locations = [[82, 76], [96, 44], [50, 5], [49, 8], [13, 7], [29, 89],
                 [58, 30], [84, 39], [14, 24], [12, 39], [3, 82], [5, 10],
                 [98, 52], [84, 25], [61, 59], [1, 65], [88, 51], [91, 2],
                 [19, 32], [93, 3], [50, 93], [98, 14], [5, 42], [42, 9],
                 [61, 62], [9, 97], [80, 55], [57, 69], [23, 15], [20, 70],
                 [85, 60], [98, 5]]

    demands = [4, 19, 21, 6, 19, 7, 12, 16, 6, 16, 8, 14, 21, 16, 3, 22, 18,
               19, 1, 24, 8, 12, 4, 8, 24, 24, 2, 20, 15, 2, 14, 9]

    start_times = [28842, 50891, 10351, 49370, 22553, 53131, 8908,
                   56509, 54032, 10883, 60235, 46644, 35674, 30304,
                   39950, 38297, 36273, 52108, 2333, 48986, 44552,
                   31869, 38027, 5532, 57458, 51521, 11039, 31063,
                   38781, 49169, 32833, 7392]

    data = [locations, demands, start_times]
    return data

if __name__ == '__main__':
    main()
