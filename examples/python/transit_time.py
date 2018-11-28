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
"""Display Transit Time
   Distances are in meters and time in minutes.

   Manhattan average block: 750ft x 264ft -> 228m x 80m
   src: https://nyti.ms/2GDoRIe "NY Times: Know Your distance"
   here we use: 114m x 80m city block
"""

from __future__ import print_function
from six.moves import xrange
from ortools.constraint_solver import pywrapcp
from ortools.constraint_solver import routing_enums_pb2


###########################
# Problem Data Definition #
###########################
class Vehicle():
    """Stores the property of a vehicle"""

    def __init__(self):
        """Initializes the vehicle properties"""
        self._capacity = 15
        # Travel speed: 5km/h to convert in m/min
        self._speed = 5 * 60 / 3.6

    @property
    def speed(self):
        """Gets the average travel speed of a vehicle"""
        return self._speed


class CityBlock():
    """City block definition"""

    @property
    def width(self):
        """Gets Block size West to East"""
        return 228 / 2

    @property
    def height(self):
        """Gets Block size North to South"""
        return 80


class DataProblem():
    """Stores the data for the problem"""

    def __init__(self):
        """Initializes the data for the problem"""
        self._vehicle = Vehicle()

        # Locations in block unit
        locations = \
                [(4, 4), # depot
                 (2, 0), (8, 0), # row 0
                 (0, 1), (1, 1),
                 (5, 2), (7, 2),
                 (3, 3), (6, 3),
                 (5, 5), (8, 5),
                 (1, 6), (2, 6),
                 (3, 7), (6, 7),
                 (0, 8), (7, 8)]
        # locations in meters using the city block dimension
        city_block = CityBlock()
        self._locations = [(loc[0] * city_block.width,
                            loc[1] * city_block.height) for loc in locations]

        self._depot = 0

        self._demands = \
            [0, # depot
             1, 1, # 1, 2
             2, 4, # 3, 4
             2, 4, # 5, 6
             8, 8, # 7, 8
             1, 2, # 9,10
             1, 2, # 11,12
             4, 4, # 13, 14
             8, 8] # 15, 16

        self._time_windows = \
            [(0, 0),
             (75, 85), (75, 85), # 1, 2
             (60, 70), (45, 55), # 3, 4
             (0, 8), (50, 60), # 5, 6
             (0, 10), (10, 20), # 7, 8
             (0, 10), (75, 85), # 9, 10
             (85, 95), (5, 15), # 11, 12
             (15, 25), (10, 20), # 13, 14
             (45, 55), (30, 40)] # 15, 16

    @property
    def vehicle(self):
        """Gets a vehicle"""
        return self._vehicle

    @property
    def locations(self):
        """Gets locations"""
        return self._locations

    @property
    def num_locations(self):
        """Gets number of locations"""
        return len(self.locations)

    @property
    def depot(self):
        """Gets depot location index"""
        return self._depot

    @property
    def demands(self):
        """Gets demands at each location"""
        return self._demands

    @property
    def time_per_demand_unit(self):
        """Gets the time (in min) to load a demand"""
        return 5  # 5 minutes/unit

    @property
    def time_windows(self):
        """Gets (start time, end time) for each locations"""
        return self._time_windows


#######################
# Problem Constraints #
#######################
def manhattan_distance(position_1, position_2):
    """Computes the Manhattan distance between two points"""
    return (
        abs(position_1[0] - position_2[0]) + abs(position_1[1] - position_2[1]))


class CreateTimeEvaluator(object):
    """Creates callback to get total times between locations."""

    @staticmethod
    def service_time(data, node):
        """Gets the service time for the specified location."""
        return data.demands[node] * data.time_per_demand_unit

    @staticmethod
    def travel_time(data, from_node, to_node):
        """Gets the travel times between two locations."""
        if from_node == to_node:
            travel_time = 0
        else:
            travel_time = manhattan_distance(data.locations[
                from_node], data.locations[to_node]) / data.vehicle.speed
        return travel_time

    def __init__(self, data):
        """Initializes the total time matrix."""
        self._total_time = {}
        # precompute total time to have time callback in O(1)
        for from_node in xrange(data.num_locations):
            self._total_time[from_node] = {}
            for to_node in xrange(data.num_locations):
                if from_node == to_node:
                    self._total_time[from_node][to_node] = 0
                else:
                    self._total_time[from_node][to_node] = int(
                        self.service_time(data, from_node) + self.travel_time(
                            data, from_node, to_node))

    def time_evaluator(self, from_node, to_node):
        """Returns the total time between the two nodes"""
        return self._total_time[from_node][to_node]


def print_transit_time(route, time_evaluator):
    """Print transit time between nodes of a route"""
    total_time = 0
    for i, j in route:
        total_time += time_evaluator(i, j)
        print('{0} -> {1}: {2}min'.format(i, j, time_evaluator(i, j)))
    print('Total time: {0}min\n'.format(total_time))


########
# Main #
########
def main():
    """Entry point of the program"""
    # Instantiate the data problem.
    data = DataProblem()

    # Print Transit Time
    time_evaluator = CreateTimeEvaluator(data).time_evaluator
    print('Route 0:')
    print_transit_time([[0, 5], [5, 8], [8, 6], [6, 2], [2, 0]], time_evaluator)

    print('Route 1:')
    print_transit_time([[0, 9], [9, 14], [14, 16], [16, 10], [10, 0]],
                       time_evaluator)

    print('Route 2:')
    print_transit_time([[0, 12], [12, 13], [13, 15], [15, 11], [11, 0]],
                       time_evaluator)

    print('Route 3:')
    print_transit_time([[0, 7], [7, 4], [4, 3], [3, 1], [1, 0]], time_evaluator)


if __name__ == '__main__':
    main()
