#!/usr/bin/env python
# This Python file uses the following encoding: utf-8
# Copyright 2015 Tin Arm Engineering AB
# Copyright 2018 Google LLC
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
"""Capacitated Vehicle Routing Problem with Time Windows (CVRPTW).

   This is a sample using the routing library python wrapper to solve a CVRPTW
   problem.
   A description of the problem can be found here:
   http://en.wikipedia.org/wiki/Vehicle_routing_problem.

   Distances are in meters and time in minutes.
"""

from __future__ import print_function

from functools import partial
from six.moves import xrange

from ortools.constraint_solver import pywrapcp
from ortools.constraint_solver import routing_enums_pb2


###########################
# Problem Data Definition #
###########################
def create_data_model():
  """Stores the data for the problem"""
  data = {}
  # Locations in block unit
  _locations = \
          [(4, 4), # depot
           (2, 0), (8, 0), # locations to visit
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
  data['locations'] = [(l[0] * 114, l[1] * 80) for l in _locations]
  data['num_locations'] = len(data['locations'])
  data['time_windows'] = \
        [(0, 0),
         (75, 85), (75, 85), # 1, 2
         (60, 70), (45, 55), # 3, 4
         (0, 8), (50, 60), # 5, 6
         (0, 10), (10, 20), # 7, 8
         (0, 10), (75, 85), # 9, 10
         (85, 95), (5, 15), # 11, 12
         (15, 25), (10, 20), # 13, 14
         (45, 55), (30, 40)] # 15, 16
  data['demands'] = \
        [0, # depot
         1, 1, # 1, 2
         2, 4, # 3, 4
         2, 4, # 5, 6
         8, 8, # 7, 8
         1, 2, # 9,10
         1, 2, # 11,12
         4, 4, # 13, 14
         8, 8] # 15, 16
  data['time_per_demand_unit'] = 5  # 5 minutes/unit
  data['num_vehicles'] = 4
  data['vehicle_capacity'] = 15
  data['vehicle_speed'] = 83  # Travel speed: 5km/h converted in m/min
  data['depot'] = 0
  return data


#######################
# Problem Constraints #
#######################
def manhattan_distance(position_1, position_2):
  """Computes the Manhattan distance between two points"""
  return (
      abs(position_1[0] - position_2[0]) + abs(position_1[1] - position_2[1]))


def create_distance_evaluator(data):
  """Creates callback to return distance between points."""
  _distances = {}
  # precompute distance between location to have distance callback in O(1)
  for from_node in xrange(data['num_locations']):
    _distances[from_node] = {}
    for to_node in xrange(data['num_locations']):
      if from_node == to_node:
        _distances[from_node][to_node] = 0
      else:
        _distances[from_node][to_node] = (
            manhattan_distance(data['locations'][from_node],
                               data['locations'][to_node]))

  def distance_evaluator(manager, from_node, to_node):
    """Returns the manhattan distance between the two nodes"""
    return _distances[manager.IndexToNode(from_node)][manager.IndexToNode(
        to_node)]

  return distance_evaluator


def create_demand_evaluator(data):
  """Creates callback to get demands at each location."""
  _demands = data['demands']

  def demand_evaluator(manager, node):
    """Returns the demand of the current node"""
    return _demands[manager.IndexToNode(node)]

  return demand_evaluator


def add_capacity_constraints(routing, data, demand_evaluator_index):
  """Adds capacity constraint"""
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

  _total_time = {}
  # precompute total time to have time callback in O(1)
  for from_node in xrange(data['num_locations']):
    _total_time[from_node] = {}
    for to_node in xrange(data['num_locations']):
      if from_node == to_node:
        _total_time[from_node][to_node] = 0
      else:
        _total_time[from_node][to_node] = int(
            service_time(data, from_node) +
            travel_time(data, from_node, to_node))

  def time_evaluator(manager, from_node, to_node):
    """Returns the total time between the two nodes"""
    return _total_time[manager.IndexToNode(from_node)][manager.IndexToNode(
        to_node)]

  return time_evaluator


def add_time_window_constraints(routing, manager, data, time_evaluator_index):
  """Add Global Span constraint"""
  time = 'Time'
  horizon = 120
  routing.AddDimension(
      time_evaluator_index,
      horizon,  # allow waiting time
      horizon,  # maximum time per vehicle
      False,  # don't force start cumul to zero since we are giving TW to start nodes
      time)
  time_dimension = routing.GetDimensionOrDie(time)
  # Add time window constraints for each location except depot
  # and 'copy' the slack var in the solution object (aka Assignment) to print it
  for location_idx, time_window in enumerate(data['time_windows']):
    if location_idx == 0:
      continue
    index = manager.NodeToIndex(location_idx)
    time_dimension.CumulVar(index).SetRange(time_window[0], time_window[1])
    routing.AddToAssignment(time_dimension.SlackVar(index))
  # Add time window constraints for each vehicle start node
  # and 'copy' the slack var in the solution object (aka Assignment) to print it
  for vehicle_id in xrange(data['num_vehicles']):
    index = routing.Start(vehicle_id)
    time_dimension.CumulVar(index).SetRange(data['time_windows'][0][0],
                                            data['time_windows'][0][1])
    routing.AddToAssignment(time_dimension.SlackVar(index))
    # Warning: Slack var is not defined for vehicle's end node
    #routing.AddToAssignment(time_dimension.SlackVar(self.routing.End(vehicle_id)))


###########
# Printer #
###########
def print_solution(data, manager, routing, assignment):  # pylint:disable=too-many-locals
  """Prints assignment on console"""
  print('Objective: {}'.format(assignment.ObjectiveValue()))
  total_distance = 0
  total_load = 0
  total_time = 0
  capacity_dimension = routing.GetDimensionOrDie('Capacity')
  time_dimension = routing.GetDimensionOrDie('Time')
  for vehicle_id in xrange(data['num_vehicles']):
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
    slack_var = time_dimension.SlackVar(index)
    plan_output += ' {0} Load({1}) Time({2},{3})\n'.format(
        manager.IndexToNode(index), assignment.Value(load_var),
        assignment.Min(time_var), assignment.Max(time_var))
    plan_output += 'Distance of the route: {0}m\n'.format(distance)
    plan_output += 'Load of the route: {}\n'.format(assignment.Value(load_var))
    plan_output += 'Time of the route: {}\n'.format(assignment.Value(time_var))
    print(plan_output)
    total_distance += distance
    total_load += assignment.Value(load_var)
    total_time += assignment.Value(time_var)
  print('Total Distance of all routes: {0}m'.format(total_distance))
  print('Total Load of all routes: {}'.format(total_load))
  print('Total Time of all routes: {0}min'.format(total_time))


########
# Main #
########
def main():
  """Entry point of the program"""
  # Instantiate the data problem.
  data = create_data_model()

  # Create the routing index manager
  manager = pywrapcp.RoutingIndexManager(data['num_locations'],
                                         data['num_vehicles'], data['depot'])

  # Create Routing Model
  routing = pywrapcp.RoutingModel(manager)

  # Define weight of each edge
  distance_evaluator_index = routing.RegisterTransitCallback(
      partial(create_distance_evaluator(data), manager))
  routing.SetArcCostEvaluatorOfAllVehicles(distance_evaluator_index)

  # Add Capacity constraint
  demand_evaluator_index = routing.RegisterUnaryTransitCallback(
      partial(create_demand_evaluator(data), manager))
  add_capacity_constraints(routing, data, demand_evaluator_index)

  # Add Time Window constraint
  time_evaluator_index = routing.RegisterTransitCallback(
      partial(create_time_evaluator(data), manager))
  add_time_window_constraints(routing, manager, data, time_evaluator_index)

  # Setting first solution heuristic (cheapest addition).
  search_parameters = pywrapcp.DefaultRoutingSearchParameters()
  search_parameters.first_solution_strategy = (
      routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC)  # pylint: disable=no-member
  # Solve the problem.
  assignment = routing.SolveWithParameters(search_parameters)
  print_solution(data, manager, routing, assignment)


if __name__ == '__main__':
  main()
