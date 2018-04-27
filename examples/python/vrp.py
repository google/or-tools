# Copyright 2010-2014 Google
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

"""Vehicle Routing Problem Sample.
   This is a sample using the routing library python wrapper to solve a
   Vehicle Routing Problem.
   The description of the problem can be found here:
   https://en.wikipedia.org/wiki/Vehicle_routing_problem.
   The optimization engine uses local search to improve solutions, first
   solutions being generated using a cheapest addition heuristic.
   Optionally one can randomly forbid a set of random connections between nodes
   (forbidden arcs).
"""

import math
from ortools.constraint_solver import pywrapcp
from ortools.constraint_solver import routing_enums_pb2

def distance(x1, y1, x2, y2):
    # Manhattan distance
    dist = abs(x1 - x2) + abs(y1 - y2)

    return dist

# Distance callback

class CreateDistanceCallback(object):
  """Create callback to calculate distances between points."""

  def __init__(self, locations):
    """Initialize distance array."""
    size = len(locations)
    self.matrix = {}

    for from_node in xrange(size):
      self.matrix[from_node] = {}
      for to_node in xrange(size):
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

# Demand callback
class CreateDemandCallback(object):
  """Create callback to get demands at each location."""

  def __init__(self, demands):
    self.matrix = demands

  def Demand(self, from_node, to_node):
    return self.matrix[from_node]

def main():
  # Create the data.
  data = create_data_array()
  locations = data[0]
  demands = data[1]
  print (demands[12]+demands[1]+demands[16])

  num_locations = len(locations)
  depot = 0
  num_vehicles = 5
  time_limit = 11

  # Create routing model.
  if num_locations > 0:

    # The number of nodes of the VRP is num_locations.
    # Nodes are indexed from 0 to num_locations - 1. By default the start of
    # a route is node 0.
    routing = pywrapcp.RoutingModel(num_locations, num_vehicles, depot)
    search_parameters = pywrapcp.RoutingModel.DefaultSearchParameters()

    # Setting first solution heuristic: the
    # method for finding a first solution to the problem.
    # If no feasible solution exists, program will timeout
    search_parameters.first_solution_strategy = (
        routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC)

    # Improve opon the initial solution
    # search_parameters.local_search_metaheuristic = (
    #     routing_enums_pb2.LocalSearchMetaheuristic.GUIDED_LOCAL_SEARCH)

    search_parameters.time_limit_ms = time_limit*1000

    # The 'PATH_CHEAPEST_ARC' method does the following:
    # Starting from a route "start" node, connect it to the node which produces the
    # cheapest route segment, then extend the route by iterating on the last
    # node added to the route.

    # Put a callback to the distance function here. The callback takes two
    # arguments (the from and to node indices) and returns the distance between
    # these nodes.

    dist_between_locations = CreateDistanceCallback(locations)
    dist_callback = dist_between_locations.Distance
    routing.SetArcCostEvaluatorOfAllVehicles(dist_callback)

    # Put a callback to the demands.
    demands_at_locations = CreateDemandCallback(demands)
    demands_callback = demands_at_locations.Demand

    # Adding capacity dimension constraints.
    vehicle_capacity = 100
    null_capacity_slack = 0
    fix_start_cumul_to_zero = True
    capacity = "Capacity"
    routing.AddDimension(demands_callback, null_capacity_slack, vehicle_capacity,
                         fix_start_cumul_to_zero, capacity)

    # Solve, displays a solution if any.
    assignment = routing.SolveWithParameters(search_parameters)
    if assignment:
      # Display solution.

      for vehicle_nbr in range(num_vehicles):
        index = routing.Start(vehicle_nbr)
        index_next = assignment.Value(routing.NextVar(index))
        route = ''
        route_dist = 0
        route_demand = 0

        while not routing.IsEnd(index_next):
          node_index = routing.IndexToNode(index)
          node_index_next = routing.IndexToNode(index_next)
          route += str(node_index) + " -> "
          # Add the distance to the next node.
          route_dist += dist_callback(node_index, node_index_next)
          # Add demand.
          route_demand += demands[node_index_next]
          index = index_next
          index_next = assignment.Value(routing.NextVar(index))

        node_index = routing.IndexToNode(index)
        node_index_next = routing.IndexToNode(index_next)
        route += str(node_index) + " -> " + str(node_index_next)
        route_dist += dist_callback(node_index, node_index_next)
        print "Route for vehicle " + str(vehicle_nbr) + ":\n\n" + route + "\n"
        print "Distance of route " + str(vehicle_nbr) + ": " + str(route_dist)
        print "Demand met by vehicle " + str(vehicle_nbr) + ": " + str(route_demand) + "\n"
    else:
      print 'No feasible solution found after %d seconds.' % time_limit
  else:
    print 'Specify an instance greater than 0.'

def create_data_array():

  locations = [[82, 76], [96, 44], [50, 5], [49, 8], [13, 7], [29, 89], [58, 30], [84, 39],
               [14, 24], [12, 39], [3, 82], [5, 10], [98, 52], [84, 25], [61, 59], [1, 65],
               [88, 51], [91, 2], [19, 32], [93, 3], [50, 93], [98, 14], [5, 42], [42, 9],
               [61, 62], [9, 97], [80, 55], [57, 69], [23, 15], [20, 70], [85, 60], [98, 5]]

  demands = [0, 19, 21, 6, 19, 7, 12, 16, 6, 16, 8, 14, 21, 16, 3, 22, 18,
             19, 1, 24, 8, 12, 4, 8, 24, 24, 2, 20, 15, 2, 14, 9]
  data = [locations, demands]
  return data
if __name__ == '__main__':
  main()
