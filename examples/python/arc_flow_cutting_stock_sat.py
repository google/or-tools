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
"""Cutting stock problem with the objective to minimize wasted space."""

from __future__ import print_function

import argparse
import collections
import time

from ortools.linear_solver import pywraplp
from ortools.sat.python import cp_model

PARSER = argparse.ArgumentParser()

PARSER.add_argument(
    '--solver', default='sat', help='Method used to solve: sat, mip.')
PARSER.add_argument(
    '--output_proto',
    default='',
    help='Output file to write the cp_model proto to.')

DESIRED_LENGTHS = [
    2490, 3980, 2490, 3980, 2391, 2391, 2391, 596, 596, 596, 2456, 2456, 3018,
    938, 3018, 938, 943, 3018, 943, 3018, 2490, 3980, 2490, 3980, 2391, 2391,
    2391, 596, 596, 596, 2456, 2456, 3018, 938, 3018, 938, 943, 3018, 943,
    3018, 2890, 3980, 2890, 3980, 2391, 2391, 2391, 596, 596, 596, 2856, 2856,
    3018, 938, 3018, 938, 943, 3018, 943, 3018, 3290, 3980, 3290, 3980, 2391,
    2391, 2391, 596, 596, 596, 3256, 3256, 3018, 938, 3018, 938, 943, 3018,
    943, 3018, 3690, 3980, 3690, 3980, 2391, 2391, 2391, 596, 596, 596, 3656,
    3656, 3018, 938, 3018, 938, 943, 3018, 943, 3018, 2790, 3980, 2790, 3980,
    2391, 2391, 2391, 596, 596, 596, 2756, 2756, 3018, 938, 3018, 938, 943,
    3018, 943, 3018, 2790, 3980, 2790, 3980, 2391, 2391, 2391, 596, 596, 596,
    2756, 2756, 3018, 938, 3018, 938, 943
]
POSSIBLE_CAPACITIES = [4000, 5000, 6000, 7000, 8000]

# Toy problem
# DESIRED_LENGTHS = [12, 12, 8, 8, 8]
# POSSIBLE_CAPACITIES = [10, 20]


def regroup_and_count(raw_input):
    grouped = collections.defaultdict(int)
    for i in raw_input:
        grouped[i] += 1
    output = []
    for size, count in grouped.items():
        output.append([size, count])
    output.sort(reverse=True)
    return output


def price_capacity(capacity, capacities):
    price = max(capacities)
    for c in capacities:
        if c < capacity:
            continue
        price = min(c - capacity, price)
    return price


def create_state_graph(items, max_capacity):
    states = []
    state_to_index = {}
    states.append(0)
    state_to_index[0] = 0
    transitions = []

    for item_index, size_and_count in enumerate(items):
        size, count = size_and_count
        num_states = len(states)
        for state_index in range(num_states):
            previous_state = states[state_index]
            previous_state_index = state_index

            for _ in range(count):
                new_state = previous_state + size
                if new_state > max_capacity:
                    break
                new_state_index = -1
                if new_state in state_to_index:
                    new_state_index = state_to_index[new_state]
                else:
                    new_state_index = len(states)
                    states.append(new_state)
                    state_to_index[new_state] = new_state_index
                # Add the transition
                transitions.append(
                    [previous_state_index, new_state_index, item_index])
                # And update counters
                previous_state_index = new_state_index
                previous_state = new_state
    return states, transitions


def solve_cutting_stock_with_arc_flow_with_sat(output_proto):
    items = regroup_and_count(DESIRED_LENGTHS)
    print('Items:', items)
    num_items = len(DESIRED_LENGTHS)

    max_capacity = max(POSSIBLE_CAPACITIES)
    states, transitions = create_state_graph(items, max_capacity)

    print('Dynamic programming has generated', len(states), 'states and',
          len(transitions), 'transitions')

    incoming_vars = collections.defaultdict(list)
    outgoing_vars = collections.defaultdict(list)
    incoming_sink_vars = []
    item_vars = collections.defaultdict(list)

    model = cp_model.CpModel()

    objective_vars = []
    objective_coeffs = []

    for outgoing, incoming, item_index in transitions:
        count = items[item_index][1]
        count_var = model.NewIntVar(
            0, count, 'i%i_f%i_t%i' % (item_index, incoming, outgoing))
        incoming_vars[incoming].append(count_var)
        outgoing_vars[outgoing].append(count_var)
        item_vars[item_index].append(count_var)

    for state_index, state in enumerate(states):
        if state_index == 0:
            continue
        exit_var = model.NewIntVar(0, num_items, 'e%i' % state_index)
        outgoing_vars[state_index].append(exit_var)
        incoming_sink_vars.append(exit_var)
        price = price_capacity(state, POSSIBLE_CAPACITIES)
        objective_vars.append(exit_var)
        objective_coeffs.append(price)

    # Flow conservation
    for state_index in range(1, len(states)):
        model.Add(
            sum(incoming_vars[state_index]) == sum(outgoing_vars[state_index]))

    # Flow going out of the source must go in the sink
    model.Add(sum(outgoing_vars[0]) == sum(incoming_sink_vars))

    # Items must be placed
    for item_index, size_and_count in enumerate(items):
        size, count = size_and_count
        model.Add(sum(item_vars[item_index]) == count)

    # Objective is the sum of waste
    model.Minimize(
        sum(objective_vars[i] * objective_coeffs[i]
            for i in range(len(objective_vars))))

    # Output model proto to file.
    if output_proto:
        output_file = open(output_proto, 'w')
        output_file.write(str(model.Proto()))
        output_file.close()

    # Solve model.
    solver = cp_model.CpSolver()
    solver.parameters.log_search_progress = True
    solver.parameters.num_search_workers = 8
    status = solver.Solve(model)
    print(solver.ResponseStats())


def solve_cutting_stock_with_arc_flow_with_mip():
    items = regroup_and_count(DESIRED_LENGTHS)
    print('Items:', items)
    num_items = len(DESIRED_LENGTHS)
    max_capacity = max(POSSIBLE_CAPACITIES)
    states, transitions = create_state_graph(items, max_capacity)

    print('Dynamic programming has generated', len(states), 'states and',
          len(transitions), 'transitions')

    incoming_vars = collections.defaultdict(list)
    outgoing_vars = collections.defaultdict(list)
    incoming_sink_vars = []
    item_vars = collections.defaultdict(list)

    start_time = time.time()
    solver = pywraplp.Solver('Steel',
                             pywraplp.Solver.CBC_MIXED_INTEGER_PROGRAMMING)

    objective_vars = []
    objective_coeffs = []

    var_index = 0
    for outgoing, incoming, item_index in transitions:
        count = items[item_index][1]
        count_var = solver.IntVar(
            0, count,
            'a%i_i%i_f%i_t%i' % (var_index, item_index, incoming, outgoing))
        var_index += 1
        incoming_vars[incoming].append(count_var)
        outgoing_vars[outgoing].append(count_var)
        item_vars[item_index].append(count_var)

    for state_index, state in enumerate(states):
        if state_index == 0:
            continue
        exit_var = solver.IntVar(0, num_items, 'e%i' % state_index)
        outgoing_vars[state_index].append(exit_var)
        incoming_sink_vars.append(exit_var)
        price = price_capacity(state, POSSIBLE_CAPACITIES)
        objective_vars.append(exit_var)
        objective_coeffs.append(price)

    # Flow conservation
    for state_index in range(1, len(states)):
        solver.Add(
            sum(incoming_vars[state_index]) == sum(outgoing_vars[state_index]))

    # Flow going out of the source must go in the sink
    solver.Add(sum(outgoing_vars[0]) == sum(incoming_sink_vars))

    # Items must be placed
    for item_index, size_and_count in enumerate(items):
        size, count = size_and_count
        solver.Add(sum(item_vars[item_index]) == count)

    # Objective is the sum of waste
    solver.Minimize(
        sum(objective_vars[i] * objective_coeffs[i]
            for i in range(len(objective_vars))))
    solver.EnableOutput()

    status = solver.Solve()

    ### Output the solution.
    if status == pywraplp.Solver.OPTIMAL:
        print('Objective value = %f found in %.2f s' %
              (solver.Objective().Value(), time.time() - start_time))
    else:
        print('No solution')


def main(args):
    """Main function"""
    if args.solver == 'sat':
        solve_cutting_stock_with_arc_flow_with_sat(args.output_proto)
    else:  # 'mip'
        solve_cutting_stock_with_arc_flow_with_mip()


if __name__ == '__main__':
    main(PARSER.parse_args())
