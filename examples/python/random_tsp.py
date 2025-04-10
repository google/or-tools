# Copyright 2010-2025 Google LLC
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
"""Traveling Salesman Sample.

   This is a sample using the routing library python wrapper to solve a
   Traveling Salesman Problem.
   The description of the problem can be found here:
   http://en.wikipedia.org/wiki/Travelling_salesman_problem.
   The optimization engine uses local search to improve solutions, first
   solutions being generated using a cheapest addition heuristic.
   Optionally one can randomly forbid a set of random connections between nodes
   (forbidden arcs).
"""

import argparse
from functools import partial
import random

from ortools.constraint_solver import routing_enums_pb2
from ortools.constraint_solver import pywrapcp

parser = argparse.ArgumentParser()

parser.add_argument(
    '--tsp_size',
    default=10,
    type=int,
    help='Size of Traveling Salesman Problem instance.')
parser.add_argument(
    '--tsp_use_random_matrix',
    default=True,
    type=bool,
    help='Use random cost matrix.')
parser.add_argument(
    '--tsp_random_forbidden_connections',
    default=0,
    type=int,
    help='Number of random forbidden connections.')
parser.add_argument(
    '--tsp_random_seed', default=0, type=int, help='Random seed.')

# Cost/distance functions.


def Distance(manager, i, j):
    """Sample function."""
    # Put your distance code here.
    node_i = manager.IndexToNode(i)
    node_j = manager.IndexToNode(j)
    return node_i + node_j


class RandomMatrix(object):
    """Random matrix."""

    def __init__(self, size, seed):
        """Initialize random matrix."""

        rand = random.Random()
        rand.seed(seed)
        distance_max = 100
        self.matrix = {}
        for from_node in range(size):
            self.matrix[from_node] = {}
            for to_node in range(size):
                if from_node == to_node:
                    self.matrix[from_node][to_node] = 0
                else:
                    self.matrix[from_node][to_node] = rand.randrange(
                        distance_max)

    def Distance(self, manager, from_index, to_index):
        return self.matrix[manager.IndexToNode(from_index)][manager.IndexToNode(
            to_index)]


def main(args):
    # Create routing model
    if args.tsp_size > 0:
        # TSP of size args.tsp_size
        # Second argument = 1 to build a single tour (it's a TSP).
        # Nodes are indexed from 0 to args_tsp_size - 1, by default the start of
        # the route is node 0.
        manager = pywrapcp.RoutingIndexManager(args.tsp_size, 1, 0)
        routing = pywrapcp.RoutingModel(manager)
        search_parameters = pywrapcp.DefaultRoutingSearchParameters()
        # Setting first solution heuristic (cheapest addition).
        search_parameters.first_solution_strategy = (
            routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC)

        # Setting the cost function.
        # Put a callback to the distance accessor here. The callback takes two
        # arguments (the from and to node indices) and returns the distance between
        # these indices.
        cost = 0
        if args.tsp_use_random_matrix:
            matrix = RandomMatrix(args.tsp_size, args.tsp_random_seed)
            cost = routing.RegisterTransitCallback(
                partial(matrix.Distance, manager))
        else:
            cost = routing.RegisterTransitCallback(partial(Distance, manager))
        routing.SetArcCostEvaluatorOfAllVehicles(cost)
        # Forbid node connections (randomly).
        rand = random.Random()
        rand.seed(args.tsp_random_seed)
        forbidden_connections = 0
        while forbidden_connections < args.tsp_random_forbidden_connections:
            from_node = rand.randrange(args.tsp_size - 1)
            to_node = rand.randrange(args.tsp_size - 1) + 1
            if routing.NextVar(from_node).Contains(to_node):
                print('Forbidding connection ' + str(from_node) + ' -> ' +
                      str(to_node))
                routing.NextVar(from_node).RemoveValue(to_node)
                forbidden_connections += 1

        # Solve, returns a solution if any.
        assignment = routing.Solve()
        if assignment:
            # Solution cost.
            print(assignment.ObjectiveValue())
            # Inspect solution.
            # Only one route here; otherwise iterate from 0 to routing.vehicles() - 1
            route_number = 0
            node = routing.Start(route_number)
            route = ''
            while not routing.IsEnd(node):
                route += str(node) + ' -> '
                node = assignment.Value(routing.NextVar(node))
            route += '0'
            print(route)
        else:
            print('No solution found.')
    else:
        print('Specify an instance greater than 0.')


if __name__ == '__main__':
    main(parser.parse_args())
