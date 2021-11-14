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
"""Solver a 2D rectangle knapsack problem.

This code is adapted from 
https://yetanothermathprogrammingconsultant.blogspot.com/2021/02/2d-bin-packing.html
"""

from io import StringIO
from absl import app
from absl import flags
from google.protobuf import text_format
import numpy as np
import pandas as pd
from ortools.sat.python import cp_model

FLAGS = flags.FLAGS

flags.DEFINE_string('output_proto', '',
                    'Output file to write the cp_model proto to.')
flags.DEFINE_string('params', 'num_search_workers:16,log_search_progress:true',
                    'Sat solver parameters.')
flags.DEFINE_string('model', 'rotation', '\'duplicate\' or \'rotation\'')


def build_data():
    """Build the data frame."""
    data = """
    item         width    height available    value    color
    k1             20       4       2        338.984   blue
    k2             12      17       6        849.246   orange
    k3             20      12       2        524.022   green
    k4             16       7       9        263.303   red
    k5              3       6       3        113.436   purple
    k6             13       5       3        551.072   brown
    k7              4       7       6         86.166   pink
    k8              6      18       8        755.094   grey
    k9             14       2       7        223.516   olive
    k10             9      11       5        369.560   cyan
    """

    data = pd.read_table(StringIO(data), sep='\s+')
    print('Input data')
    print(data)

    height = 20
    width = 30

    print(f'Container width:{width} height:{height}')
    print(f'#Items: {len(data.index)}')
    return (data, height, width)


def solve_with_duplicate_items(data, height, width):
    """Solve the problem by building 2 items (rotated or not) for each item."""
    # Derived data (expanded to individual items).
    data_widths = data['width'].to_numpy()
    data_heights = data['height'].to_numpy()
    data_availability = data['available'].to_numpy()
    data_values = data['value'].to_numpy()

    # Non duplicated items data.
    base_item_widths = np.repeat(data_widths, data_availability)
    base_item_heights = np.repeat(data_heights, data_availability)
    base_item_values = np.repeat(data_values, data_availability)
    num_data_items = len(base_item_values)

    # Create rotated items by duplicating.
    item_widths = np.concatenate((base_item_widths, base_item_heights))
    item_heights = np.concatenate((base_item_heights, base_item_widths))
    item_values = np.concatenate((base_item_values, base_item_values))

    # Scale values to become integers.
    item_values = (item_values * 1000.0).astype('int')
    num_items = len(item_values)

    # OR-Tools model
    model = cp_model.CpModel()

    # Variables

    ## u[i] : item i is used
    is_used = [model.NewBoolVar(f'u{i}') for i in range(num_items)]

    ## x_start[i],y_start[i] : location of item i
    x_start = [model.NewIntVar(0, width, f'x_start{i}') for i in range(num_items)]
    y_start = [model.NewIntVar(0, height, f'y_start{i}') for i in range(num_items)]

    ## x_end[i],y_end[i] : upper limit of interval variable
    x_end = [model.NewIntVar(0, width, f'x_end{i}') for i in range(num_items)]
    y_end = [model.NewIntVar(0, height, f'y_end{i}') for i in range(num_items)]

    ## interval variables
    x_intervals = [
        model.NewIntervalVar(x_start[i], item_widths[i] * is_used[i], x_end[i],
                             f'xival{i}') for i in range(num_items)
    ]
    y_intervals = [
        model.NewIntervalVar(y_start[i], item_heights[i] * is_used[i], y_end[i],
                             f'yival{i}') for i in range(num_items)
    ]
    # only one of non-rotated/rotated pair can be used
    for i in range(num_data_items):
        model.Add(is_used[i] + is_used[i + num_data_items] <= 1)

    # Constraints.

    ## 2D no overlap.
    model.AddNoOverlap2D(x_intervals, y_intervals)

    ## Objective.
    model.Maximize(sum([is_used[i] * item_values[i] for i in range(num_items)]))

    # Output proto to file.
    if FLAGS.output_proto:
        print('Writing proto to %s' % FLAGS.output_proto)
        with open(FLAGS.output_proto, 'w') as text_file:
            text_file.write(str(model))

    # Solve model.
    solver = cp_model.CpSolver()
    if FLAGS.params:
        text_format.Parse(FLAGS.params, solver.parameters)

    status = solver.Solve(model)

    # Report solution.
    if status == cp_model.OPTIMAL:
        used = {i for i in range(num_items) if solver.BooleanValue(is_used[i])}
        data = pd.DataFrame({
            'x_start': [solver.Value(x_start[i]) for i in used],
            'y_start': [solver.Value(y_start[i]) for i in used],
            'item_width': [item_widths[i] for i in used],
            'item_height': [item_heights[i] for i in used],
            'x_end': [solver.Value(x_end[i]) for i in used],
            'y_end': [solver.Value(y_end[i]) for i in used],
            'item_value': [item_values[i] for i in used]
        })
        print(data)


def solve_with_rotations(data, height, width):
    """Solve the problem by rotating items."""
    # Derived data (expanded to individual items).
    data_widths = data['width'].to_numpy()
    data_heights = data['height'].to_numpy()
    data_availability = data['available'].to_numpy()
    data_values = data['value'].to_numpy()

    item_widths = np.repeat(data_widths, data_availability)
    item_heights = np.repeat(data_heights, data_availability)
    item_values = np.repeat(data_values, data_availability)

    num_items = len(item_widths)

    # Scale values to become integers.
    item_values = (item_values * 1000.0).astype('int')

    # OR-Tools model.
    model = cp_model.CpModel()

    # Variables.

    ## x_start[i],y_start[i] : location of item i.
    x_start = [model.NewIntVar(0, width, f'x_start{i}') for i in range(num_items)]
    y_start = [model.NewIntVar(0, height, f'y_start{i}') for i in range(num_items)]

    ## x_size[i],y_size[i] : sizes of item i.
    x_size = [
        model.NewIntVarFromDomain(
            cp_model.Domain.FromValues([0, item_widths[i], item_heights[i]]),
            f'x_size{i}') for i in range(num_items)
    ]
    y_size = [
        model.NewIntVarFromDomain(
            cp_model.Domain.FromValues([0, item_widths[i], item_heights[i]]),
            f'y_size{i}') for i in range(num_items)
    ]

    ## x_end[i],y_end[i] : upper limit of interval variable.
    x_end = [model.NewIntVar(0, width, f'x_end{i}') for i in range(num_items)]
    y_end = [model.NewIntVar(0, height, f'y_end{i}') for i in range(num_items)]

    ## Interval variables
    x_intervals = [
        model.NewIntervalVar(x_start[i], x_size[i], x_end[i], f'xival{i}')
        for i in range(num_items)
    ]
    y_intervals = [
        model.NewIntervalVar(y_start[i], y_size[i], y_end[i], f'yival{i}')
        for i in range(num_items)
    ]

    is_used = []

    # Constraints.

    ## for each item, decide is unselected, no_rotation, rotated.
    for i in range(num_items):
        not_selected = model.NewBoolVar(f'not_selected_{i}')
        no_rotation = model.NewBoolVar(f'no_rotation{i}')
        rotation = model.NewBoolVar(f'rotation{i}')

        ### Only one state can be chosen.
        model.Add(not_selected + no_rotation + rotation == 1)

        ### Define height and width.
        model.Add(x_size[i] == 0).OnlyEnforceIf(not_selected)
        model.Add(y_size[i] == 0).OnlyEnforceIf(not_selected)
        model.Add(x_size[i] == item_widths[i]).OnlyEnforceIf(no_rotation)
        model.Add(y_size[i] == item_heights[i]).OnlyEnforceIf(no_rotation)
        model.Add(x_size[i] == item_heights[i]).OnlyEnforceIf(rotation)
        model.Add(y_size[i] == item_widths[i]).OnlyEnforceIf(rotation)

        is_used.append(not_selected.Not())

    ## 2D no overlap.
    model.AddNoOverlap2D(x_intervals, y_intervals)

    # Objective.
    model.Maximize(sum(is_used[i] * item_values[i] for i in range(num_items)))

    # Output proto to file.
    if FLAGS.output_proto:
        print('Writing proto to %s' % FLAGS.output_proto)
        with open(FLAGS.output_proto, 'w') as text_file:
            text_file.write(str(model))

    # Solve model.
    solver = cp_model.CpSolver()
    if FLAGS.params:
        text_format.Parse(FLAGS.params, solver.parameters)

    status = solver.Solve(model)

    # Report solution.
    if status == cp_model.OPTIMAL:
        used = {i for i in range(num_items) if solver.BooleanValue(is_used[i])}
        data = pd.DataFrame({
            'x_start': [solver.Value(x_start[i]) for i in used],
            'y_start': [solver.Value(y_start[i]) for i in used],
            'item_width': [solver.Value(x_size[i]) for i in used],
            'item_height': [solver.Value(y_size[i]) for i in used],
            'x_end': [solver.Value(x_end[i]) for i in used],
            'y_end': [solver.Value(y_end[i]) for i in used],
            'item_value': [item_values[i] for i in used]
        })
        print(data)


def main(_):
    """Solve the problem with all models."""
    data, height, width = build_data()
    if FLAGS.model == 'duplicate':
        solve_with_duplicate_items(data, height, width)
    else:
        solve_with_rotations(data, height, width)


if __name__ == '__main__':
    app.run(main)