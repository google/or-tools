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
"""Appointment selection.

This module maximizes the number of appointments that can
be fulfilled by a crew of installers while staying close to ideal
ratio of appointment types.
"""

# overloaded sum() clashes with pytype.
# pytype: disable=wrong-arg-types

from absl import app
from absl import flags
from ortools.linear_solver import pywraplp
from ortools.sat.python import cp_model

FLAGS = flags.FLAGS
flags.DEFINE_integer('load_min', 480, 'Minimum load in minutes.')
flags.DEFINE_integer('load_max', 540, 'Maximum load in minutes.')
flags.DEFINE_integer('commute_time', 30, 'Commute time in minutes.')
flags.DEFINE_integer('num_workers', 98, 'Maximum number of workers.')


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


def EnumerateAllKnapsacksWithRepetition(item_sizes, total_size_min,
                                        total_size_max):
    """Enumerate all possible knapsacks with total size in the given range.

  Args:
    item_sizes: a list of integers. item_sizes[i] is the size of item #i.
    total_size_min: an integer, the minimum total size.
    total_size_max: an integer, the maximum total size.

  Returns:
    The list of all the knapsacks whose total size is in the given inclusive
    range. Each knapsack is a list [#item0, #item1, ... ], where #itemK is an
    nonnegative integer: the number of times we put item #K in the knapsack.
  """
    model = cp_model.CpModel()
    variables = [
        model.NewIntVar(0, total_size_max // size, '') for size in item_sizes
    ]
    load = sum(variables[i] * size for i, size in enumerate(item_sizes))
    model.AddLinearConstraint(load, total_size_min, total_size_max)

    solver = cp_model.CpSolver()
    solution_collector = AllSolutionCollector(variables)
    # Enumerate all solutions.
    solver.parameters.enumerate_all_solutions = True
    # Solve
    solver.Solve(model, solution_collector)
    return solution_collector.combinations()


def AggregateItemCollectionsOptimally(item_collections, max_num_collections,
                                      ideal_item_ratios):
    """Selects a set (with repetition) of combination of items optimally.

  Given a set of collections of N possible items (in each collection, an item
  may appear multiple times), a  given "ideal breakdown of items", and a
  maximum number of collections, this method finds the optimal way to
  aggregate the collections in order to:
  - maximize the overall number of items
  - while keeping the ratio of each item, among the overall selection, as close
    as possible to a given input ratio (which depends on the item).
  Each collection may be selected more than one time.

  Args:
    item_collections: a list of item collections. Each item collection is a
        list of integers [#item0, ..., #itemN-1], where #itemK is the number
        of times item #K appears in the collection, and N is the number of
        distinct items.
    max_num_collections: an integer, the maximum number of item collections
        that may be selected (counting repetitions of the same collection).
    ideal_item_ratios: A list of N float which sums to 1.0: the K-th element is
        the ideal ratio of item #K in the whole aggregated selection.

  Returns:
    A pair (objective value, list of pairs (item collection, num_selections)),
    where:
      - "objective value" is the value of the internal objective function used
        by the MIP Solver
      - Each "item collection" is an element of the input item_collections
      - and its associated "num_selections" is the number of times it was
        selected.
  """
    solver = pywraplp.Solver('Select',
                             pywraplp.Solver.SCIP_MIXED_INTEGER_PROGRAMMING)
    n = len(ideal_item_ratios)
    num_distinct_collections = len(item_collections)
    max_num_items_per_collection = 0
    for template in item_collections:
        max_num_items_per_collection = max(max_num_items_per_collection,
                                           sum(template))
    upper_bound = max_num_items_per_collection * max_num_collections

    # num_selections_of_collection[i] is an IntVar that represents the number
    # of times that we will use collection #i in our global selection.
    num_selections_of_collection = [
        solver.IntVar(0, max_num_collections, 's[%d]' % i)
        for i in range(num_distinct_collections)
    ]

    # num_overall_item[i] is an IntVar that represents the total count of item #i,
    # aggregated over all selected collections. This is enforced with dedicated
    # constraints that bind them with the num_selections_of_collection vars.
    num_overall_item = [
        solver.IntVar(0, upper_bound, 'num_overall_item[%d]' % i)
        for i in range(n)
    ]
    for i in range(n):
        ct = solver.Constraint(0.0, 0.0)
        ct.SetCoefficient(num_overall_item[i], -1)
        for j in range(num_distinct_collections):
            ct.SetCoefficient(num_selections_of_collection[j],
                              item_collections[j][i])

    # Maintain the num_all_item variable as the sum of all num_overall_item
    # variables.
    num_all_items = solver.IntVar(0, upper_bound, 'num_all_items')
    solver.Add(solver.Sum(num_overall_item) == num_all_items)

    # Sets the total number of workers.
    solver.Add(solver.Sum(num_selections_of_collection) == max_num_collections)

    # Objective variables.
    deviation_vars = [
        solver.NumVar(0, upper_bound, 'deviation_vars[%d]' % i)
        for i in range(n)
    ]
    for i in range(n):
        deviation = deviation_vars[i]
        solver.Add(deviation >= num_overall_item[i] -
                   ideal_item_ratios[i] * num_all_items)
        solver.Add(deviation >= ideal_item_ratios[i] * num_all_items -
                   num_overall_item[i])

    solver.Maximize(num_all_items - solver.Sum(deviation_vars))

    result_status = solver.Solve()

    if result_status == pywraplp.Solver.OPTIMAL:
        # The problem has an optimal solution.
        return [int(v.solution_value()) for v in num_selections_of_collection]
    return []


def GetOptimalSchedule(demand):
    """Computes the optimal schedule for the installation input.

  Args:
    demand: a list of "appointment types". Each "appointment type" is
      a triple (ideal_ratio_pct, name, duration_minutes), where
      ideal_ratio_pct is the ideal percentage (in [0..100.0]) of that
      type of appointment among all appointments scheduled.

  Returns:
    The same output type as EnumerateAllKnapsacksWithRepetition.
  """
    combinations = EnumerateAllKnapsacksWithRepetition(
        [a[2] + FLAGS.commute_time for a in demand], FLAGS.load_min,
        FLAGS.load_max)
    print(('Found %d possible day schedules ' % len(combinations) +
           '(i.e. combination of appointments filling up one worker\'s day)'))

    selection = AggregateItemCollectionsOptimally(
        combinations, FLAGS.num_workers, [a[0] / 100.0 for a in demand])
    output = []
    for i in range(len(selection)):
        if selection[i] != 0:
            output.append((selection[i], [(combinations[i][t], demand[t][1])
                                          for t in range(len(demand))
                                          if combinations[i][t] != 0]))

    return output


def main(_):
    demand = [(45.0, 'Type1', 90), (30.0, 'Type2', 120), (25.0, 'Type3', 180)]
    print('*** input problem ***')
    print('Appointments: ')
    for a in demand:
        print('   %.2f%% of %s : %d min' % (a[0], a[1], a[2]))
    print('Commute time = %d' % FLAGS.commute_time)
    print('Acceptable duration of a work day = [%d..%d]' %
          (FLAGS.load_min, FLAGS.load_max))
    print('%d workers' % FLAGS.num_workers)
    selection = GetOptimalSchedule(demand)
    print()
    installed = 0
    installed_per_type = {}
    for a in demand:
        installed_per_type[a[1]] = 0

    print('*** output solution ***')
    for template in selection:
        num_instances = template[0]
        print('%d schedules with ' % num_instances)
        for t in template[1]:
            mult = t[0]
            print('   %d installation of type %s' % (mult, t[1]))
            installed += num_instances * mult
            installed_per_type[t[1]] += num_instances * mult

    print()
    print('%d installations planned' % installed)
    for a in demand:
        name = a[1]
        per_type = installed_per_type[name]
        print(('   %d (%.2f%%) installations of type %s planned' %
               (per_type, per_type * 100.0 / installed, name)))


if __name__ == '__main__':
    app.run(main)
