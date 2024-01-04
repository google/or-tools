# Copyright 2010-2024 Google LLC
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


import collections
import time
import numpy as np

from absl import app
from absl import flags
from google.protobuf import text_format
from ortools.linear_solver.python import model_builder as mb
from ortools.sat.python import cp_model

FLAGS = flags.FLAGS

_OUTPUT_PROTO = flags.DEFINE_string(
    'output_proto', '', 'Output file to write the cp_model proto to.')
_PARAMS = flags.DEFINE_string(
    'params',
    'num_search_workers:8,log_search_progress:true,max_time_in_seconds:10',
    'Sat solver parameters.')
_SOLVER = flags.DEFINE_string(
    'solver', 'sat', 'Method used to solve: sat, mip.')


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
    """Regroup all equal capacities in a multiset."""
    grouped = collections.defaultdict(int)
    for i in raw_input:
        grouped[i] += 1
    output = []
    for size, count in grouped.items():
        output.append([size, count])
    output.sort(reverse=False)
    return output


def price_usage(usage, capacities):
    """Compute the best price for a given usage and possible capacities."""
    price = max(capacities)
    for capacity in capacities:
        if capacity < usage:
            continue
        price = min(capacity - usage, price)
    return price


def create_state_graph(items, max_capacity):
    """Create a state graph from a multiset of items, and a maximum capacity."""
    states = []
    state_to_index = {}
    states.append(0)
    state_to_index[0] = 0
    transitions = []

    for item_index, size_and_count in enumerate(items):
        size, count = size_and_count
        num_states = len(states)
        for state_index in range(num_states):
            current_state = states[state_index]
            current_state_index = state_index

            for card in range(count):
                new_state = current_state + size * (card + 1)
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
                transitions.append([
                    current_state_index, new_state_index, item_index, card + 1
                ])

    return states, transitions


def solve_cutting_stock_with_arc_flow_and_sat(output_proto_file: str, params: str):
    """Solve the cutting stock with arc-flow and the CP-SAT solver."""
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
    item_coeffs = collections.defaultdict(list)
    transition_vars = []

    model = cp_model.CpModel()

    objective_vars = []
    objective_coeffs = []

    for outgoing, incoming, item_index, card in transitions:
        count = items[item_index][1]
        max_count = count // card
        count_var = model.NewIntVar(
            0, max_count,
            'i%i_f%i_t%i_C%s' % (item_index, incoming, outgoing, card))
        incoming_vars[incoming].append(count_var)
        outgoing_vars[outgoing].append(count_var)
        item_vars[item_index].append(count_var)
        item_coeffs[item_index].append(card)
        transition_vars.append(count_var)

    for state_index, state in enumerate(states):
        if state_index == 0:
            continue
        exit_var = model.NewIntVar(0, num_items, 'e%i' % state_index)
        outgoing_vars[state_index].append(exit_var)
        incoming_sink_vars.append(exit_var)
        price = price_usage(state, POSSIBLE_CAPACITIES)
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
        num_arcs = len(item_vars[item_index])
        model.Add(
            sum(item_vars[item_index][i] * item_coeffs[item_index][i]
                for i in range(num_arcs)) == size_and_count[1])

    # Objective is the sum of waste
    model.Minimize(
        sum(objective_vars[i] * objective_coeffs[i]
            for i in range(len(objective_vars))))

    # Output model proto to file.
    if output_proto_file:
        model.ExportToFile(output_proto_file)

    # Solve model.
    solver = cp_model.CpSolver()
    if params:
        text_format.Parse(params, solver.parameters)
    solver.parameters.log_search_progress = True
    solver.Solve(model)


def solve_cutting_stock_with_arc_flow_and_mip():
    """Solve the cutting stock with arc-flow and a MIP solver."""
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
    item_coeffs = collections.defaultdict(list)

    start_time = time.time()
    model = mb.ModelBuilder()

    objective_vars = []
    objective_coeffs = []

    var_index = 0
    for outgoing, incoming, item_index, card in transitions:
        count = items[item_index][1]
        count_var = model.new_int_var(
            0, count, 'a%i_i%i_f%i_t%i_c%i' % (var_index, item_index, incoming,
                                               outgoing, card))
        var_index += 1
        incoming_vars[incoming].append(count_var)
        outgoing_vars[outgoing].append(count_var)
        item_vars[item_index].append(count_var)
        item_coeffs[item_index].append(card)

    for state_index, state in enumerate(states):
        if state_index == 0:
            continue
        exit_var = model.new_int_var(0, num_items, 'e%i' % state_index)
        outgoing_vars[state_index].append(exit_var)
        incoming_sink_vars.append(exit_var)
        price = price_usage(state, POSSIBLE_CAPACITIES)
        objective_vars.append(exit_var)
        objective_coeffs.append(price)

    # Flow conservation
    for state_index in range(1, len(states)):
        model.add(
            mb.LinearExpr.sum(incoming_vars[state_index]) == mb.LinearExpr.sum(
                outgoing_vars[state_index]))

    # Flow going out of the source must go in the sink
    model.add(
        mb.LinearExpr.sum(outgoing_vars[0]) == mb.LinearExpr.sum(
            incoming_sink_vars))

    # Items must be placed
    for item_index, size_and_count in enumerate(items):
        num_arcs = len(item_vars[item_index])
        model.add(
            mb.LinearExpr.sum([item_vars[item_index][i] * item_coeffs[item_index][i]
                for i in range(num_arcs)]) == size_and_count[1])

    # Objective is the sum of waste
    model.minimize(np.dot(objective_vars, objective_coeffs))

    solver = mb.ModelSolver('scip')
    solver.enable_output(True)
    status = solver.solve(model)

    ### Output the solution.
    if status == mb.SolveStatus.OPTIMAL or status == mb.SolveStatus.FEASIBLE:
        print('Objective value = %f found in %.2f s' %
              (solver.objective_value, time.time() - start_time))
    else:
        print('No solution')


def main(_):
    """Main function"""
    if _SOLVER.value == 'sat':
        solve_cutting_stock_with_arc_flow_and_sat(_OUTPUT_PROTO.value,
                                                  _PARAMS.value)
    else:  # 'mip'
        solve_cutting_stock_with_arc_flow_and_mip()


if __name__ == '__main__':
    app.run(main)
