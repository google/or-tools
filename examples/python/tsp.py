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
"""Travelling Salesman Problem (TSP).

   This is a sample using the routing library python wrapper to solve a TSP
   problem.
   A description of the problem can be found here:
   http://en.wikipedia.org/wiki/Travelling_salesman_problem.

   Distances are in meters.
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
  # src: https://nyti.ms/2GDoRIe 'NY Times: Know Your distance'
  data['locations'] = [(l[0] * 114, l[1] * 80) for l in _locations]
  data['num_locations'] = len(data['locations'])
  data['num_vehicles'] = 1
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


###########
# Printer #
###########
def print_solution(routing, manager, assignment):
  """Prints assignment on console"""
  print('Objective: {}'.format(assignment.ObjectiveValue()))
  index = routing.Start(0)
  plan_output = 'Route:\n'
  distance = 0
  while not routing.IsEnd(index):
    plan_output += ' {} ->'.format(manager.IndexToNode(index))
    previous_index = index
    index = assignment.Value(routing.NextVar(index))
    distance += routing.GetArcCostForVehicle(previous_index, index, 0)
  plan_output += ' {}\n'.format(manager.IndexToNode(index))
  plan_output += 'Distance of the route: {}m\n'.format(distance)
  print(plan_output)


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

  # Setting first solution heuristic (cheapest addition).
  search_parameters = pywrapcp.DefaultRoutingSearchParameters()
  search_parameters.first_solution_strategy = (
      routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC)  # pylint: disable=no-member
  # Solve the problem.
  assignment = routing.SolveWithParameters(search_parameters)
  print_solution(routing, manager, assignment)


if __name__ == '__main__':
  main()
