# Copyright 2010-2018 Google LLC
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
"""Generates possible daily schedules for workers."""

from __future__ import print_function
from __future__ import division

import argparse
from ortools.sat.python import cp_model
from ortools.linear_solver import pywraplp

PARSER = argparse.ArgumentParser()
PARSER.add_argument(
    '--load_min', default=480, type=int, help='Minimum load in minutes')
PARSER.add_argument(
    '--load_max', default=540, type=int, help='Maximum load in minutes')
PARSER.add_argument(
    '--commute_time', default=30, type=int, help='Commute time in minutes')
PARSER.add_argument(
    '--num_workers', default=98, type=int, help='Maximum number of workers.')


class AllSolutionCollector(cp_model.CpSolverSolutionCallback):
    """Stores all solutions."""

    def __init__(self, variables):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__variables = variables
        self.__collect = []

    def on_solution_callback(self):
        """Collect a new combination."""
        combination = [self.Value(v) for v in self.__variables]
        self.__collect.append(combination)

    def combinations(self):
        """Returns all collected combinations."""
        return self.__collect


def find_combinations(durations, load_min, load_max, commute_time):
    """This methods find all valid combinations of appointments.

    This methods find all combinations of appointments such that the sum of
    durations + commute times is between load_min and load_max.

    Args:
        durations: The durations of all appointments.
        load_min: The min number of worked minutes for a valid selection.
        load_max: The max number of worked minutes for a valid selection.
        commute_time: The commute time between two appointments in minutes.

    Returns:
        A matrix where each line is a valid combinations of appointments.
    """
    model = cp_model.CpModel()
    variables = [
        model.NewIntVar(0, load_max // (duration + commute_time), '')
        for duration in durations
    ]
    terms = sum(variables[i] * (duration + commute_time)
                for i, duration in enumerate(durations))
    model.AddLinearConstraint(terms, load_min, load_max)

    solver = cp_model.CpSolver()
    solution_collector = AllSolutionCollector(variables)
    solver.SearchForAllSolutions(model, solution_collector)
    return solution_collector.combinations()


def select(combinations, loads, max_number_of_workers):
    """This method selects the optimal combination of appointments.

  This method uses Mixed Integer Programming to select the optimal mix of
  appointments.
  """
    solver = pywraplp.Solver('Select',
                             pywraplp.Solver.CBC_MIXED_INTEGER_PROGRAMMING)
    num_vars = len(loads)
    num_combinations = len(combinations)
    variables = [
        solver.IntVar(0, max_number_of_workers, 's[%d]' % i)
        for i in range(num_combinations)
    ]
    achieved = [
        solver.IntVar(0, 1000, 'achieved[%d]' % i) for i in range(num_vars)
    ]
    transposed = [[
        combinations[type][index] for type in range(num_combinations)
    ] for index in range(num_vars)]

    # Maintain the achieved variables.
    for i, coefs in enumerate(transposed):
        ct = solver.Constraint(0.0, 0.0)
        ct.SetCoefficient(achieved[i], -1)
        for j, coef in enumerate(coefs):
            ct.SetCoefficient(variables[j], coef)

    # Simple bound.
    solver.Add(solver.Sum(variables) <= max_number_of_workers)

    obj_vars = [
        solver.IntVar(0, 1000, 'obj_vars[%d]' % i) for i in range(num_vars)
    ]
    for i in range(num_vars):
        solver.Add(obj_vars[i] >= achieved[i] - loads[i])
        solver.Add(obj_vars[i] >= loads[i] - achieved[i])

    solver.Minimize(solver.Sum(obj_vars))

    result_status = solver.Solve()

    # The problem has an optimal solution.
    if result_status == pywraplp.Solver.OPTIMAL:
        print('Problem solved in %f milliseconds' % solver.WallTime())
        return solver.Objective().Value(), [
            int(v.SolutionValue()) for v in variables
        ]
    return -1, []


def get_optimal_schedule(demand, args):
    """Computes the optimal schedule for the appointment selection problem."""
    combinations = find_combinations([a[2] for a in demand], args.load_min,
                                     args.load_max, args.commute_time)
    print('found %d possible combinations of appointements' % len(combinations))

    cost, selection = select(combinations, [a[0]
                                            for a in demand], args.num_workers)
    output = [(selection[i], [(combinations[i][t], demand[t][1])
                              for t in range(len(demand))
                              if combinations[i][t] != 0])
              for i in range(len(selection)) if selection[i] != 0]
    return cost, output


def main(args):
    """Solve the assignment problem."""
    demand = [(40, 'A1', 90), (30, 'A2', 120), (25, 'A3', 180)]
    print('appointments: ')
    for a in demand:
        print('   %d * %s : %d min' % (a[0], a[1], a[2]))
    print('commute time = %d' % args.commute_time)
    print('accepted total duration = [%d..%d]' % (args.load_min, args.load_max))
    print('%d workers' % args.num_workers)
    cost, selection = get_optimal_schedule(demand, args)
    print('Optimal solution as a cost of %d' % cost)
    for template in selection:
        print('%d schedules with ' % template[0])
        for t in template[1]:
            print('   %d installation of type %s' % (t[0], t[1]))


if __name__ == '__main__':
    main(PARSER.parse_args())
