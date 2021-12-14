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
https://yetanothermathprogrammingconsultant.blogspot.com/2021/10/2d-knapsack-problem.html
"""

import io

from absl import app
from absl import flags
import numpy as np
import pandas as pd

from google.protobuf import text_format

from ortools.sat.python import cp_model

FLAGS = flags.FLAGS

flags.DEFINE_string('output_proto', '',
                    'Output file to write the cp_model proto to.')
flags.DEFINE_string('params', 'num_search_workers:16,log_search_progress:true',
                    'Sat solver parameters.')
flags.DEFINE_string('model', 'rotation',
                    '\'duplicate\' or \'rotation\' or \'optional\'')


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

    data = pd.read_table(io.StringIO(data), sep=r'\s+')
    print('Input data')
    print(data)

    max_height = 20
    max_width = 30

    print(f'Container max_width:{max_width} max_height:{max_height}')
    print(f'#Items: {len(data.index)}')
    return (data, max_height, max_width)


def solve_with_duplicate_items(data, max_height, max_width):
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

    num_items = len(item_values)

    # OR-Tools model
    model = cp_model.CpModel()

    # Variables
    x_starts = []
    x_ends = []
    y_starts = []
    y_ends = []
    is_used = []
    x_intervals = []
    y_intervals = []

    for i in range(num_items):
        ## Is the item used?
        is_used.append(model.NewBoolVar(f'is_used{i}'))

        ## Item coordinates.
        x_starts.append(model.NewIntVar(0, max_width, f'x_start{i}'))
        x_ends.append(model.NewIntVar(0, max_width, f'x_end{i}'))
        y_starts.append(model.NewIntVar(0, max_height, f'y_start{i}'))
        y_ends.append(model.NewIntVar(0, max_height, f'y_end{i}'))

        ## Interval variables.
        x_intervals.append(
            model.NewIntervalVar(x_starts[i], item_widths[i] * is_used[i],
                                 x_ends[i], f'x_interval{i}'))
        y_intervals.append(
            model.NewIntervalVar(y_starts[i], item_heights[i] * is_used[i],
                                 y_ends[i], f'y_interval{i}'))

    # Constraints.

    ## Only one of non-rotated/rotated pair can be used.
    for i in range(num_data_items):
        model.Add(is_used[i] + is_used[i + num_data_items] <= 1)

    ## 2D no overlap.
    model.AddNoOverlap2D(x_intervals, y_intervals)

    ## Objective.
    model.Maximize(cp_model.LinearExpr.ScalProd(is_used, item_values))

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
            'x_start': [solver.Value(x_starts[i]) for i in used],
            'y_start': [solver.Value(y_starts[i]) for i in used],
            'item_width': [item_widths[i] for i in used],
            'item_height': [item_heights[i] for i in used],
            'x_end': [solver.Value(x_ends[i]) for i in used],
            'y_end': [solver.Value(y_ends[i]) for i in used],
            'item_value': [item_values[i] for i in used]
        })
        print(data)


def solve_with_duplicate_optional_items(data, max_height, max_width):
    """Solve the problem by building 2 optional items (rotated or not) for each item."""
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

    num_items = len(item_values)

    # OR-Tools model
    model = cp_model.CpModel()

    # Variables
    x_starts = []
    y_starts = []
    is_used = []
    x_intervals = []
    y_intervals = []

    for i in range(num_items):
        ## Is the item used?
        is_used.append(model.NewBoolVar(f'is_used{i}'))

        ## Item coordinates.
        x_starts.append(
            model.NewIntVar(0, max_width - int(item_widths[i]), f'x_start{i}'))
        y_starts.append(
            model.NewIntVar(0, max_height - int(item_heights[i]),
                            f'y_start{i}'))

        ## Interval variables.
        x_intervals.append(
            model.NewOptionalFixedSizeIntervalVar(x_starts[i], item_widths[i],
                                                  is_used[i], f'x_interval{i}'))
        y_intervals.append(
            model.NewOptionalFixedSizeIntervalVar(y_starts[i], item_heights[i],
                                                  is_used[i], f'y_interval{i}'))

    # Constraints.

    ## Only one of non-rotated/rotated pair can be used.
    for i in range(num_data_items):
        model.Add(is_used[i] + is_used[i + num_data_items] <= 1)

    ## 2D no overlap.
    model.AddNoOverlap2D(x_intervals, y_intervals)

    ## Objective.
    model.Maximize(cp_model.LinearExpr.ScalProd(is_used, item_values))

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
            'x_start': [solver.Value(x_starts[i]) for i in used],
            'y_start': [solver.Value(y_starts[i]) for i in used],
            'item_width': [item_widths[i] for i in used],
            'item_height': [item_heights[i] for i in used],
            'x_end': [solver.Value(x_starts[i]) + item_widths[i] for i in used],
            'y_end': [
                solver.Value(y_starts[i]) + item_heights[i] for i in used
            ],
            'item_value': [item_values[i] for i in used]
        })
        print(data)


def solve_with_rotations(data, max_height, max_width):
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

    # OR-Tools model.
    model = cp_model.CpModel()

    # Coordinate variables for each rectangle.
    x_starts = []
    x_sizes = []
    x_ends = []
    y_starts = []
    y_sizes = []
    y_ends = []
    x_intervals = []
    y_intervals = []

    for i in range(num_items):
        sizes = [0, int(item_widths[i]), int(item_heights[i])]
        # X coordinates.
        x_starts.append(model.NewIntVar(0, max_width, f'x_start{i}'))
        x_sizes.append(
            model.NewIntVarFromDomain(cp_model.Domain.FromValues(sizes),
                                      f'x_size{i}'))
        x_ends.append(model.NewIntVar(0, max_width, f'x_end{i}'))

        # Y coordinates.
        y_starts.append(model.NewIntVar(0, max_height, f'y_start{i}'))
        y_sizes.append(
            model.NewIntVarFromDomain(cp_model.Domain.FromValues(sizes),
                                      f'y_size{i}'))
        y_ends.append(model.NewIntVar(0, max_height, f'y_end{i}'))

        ## Interval variables
        x_intervals.append(
            model.NewIntervalVar(x_starts[i], x_sizes[i], x_ends[i],
                                 f'x_interval{i}'))
        y_intervals.append(
            model.NewIntervalVar(y_starts[i], y_sizes[i], y_ends[i],
                                 f'y_interval{i}'))

    # is_used[i] == True if and only if item i is selected.
    is_used = []

    # Constraints.

    ## for each item, decide is unselected, no_rotation, rotated.
    for i in range(num_items):
        not_selected = model.NewBoolVar(f'not_selected_{i}')
        no_rotation = model.NewBoolVar(f'no_rotation_{i}')
        rotated = model.NewBoolVar(f'rotated_{i}')

        ### Exactly one state must be chosen.
        model.Add(not_selected + no_rotation + rotated == 1)

        ### Define height and width according to the state.
        dim1 = item_widths[i]
        dim2 = item_heights[i]
        model.Add(x_sizes[i] == 0).OnlyEnforceIf(not_selected)
        model.Add(y_sizes[i] == 0).OnlyEnforceIf(not_selected)
        model.Add(x_sizes[i] == dim1).OnlyEnforceIf(no_rotation)
        model.Add(y_sizes[i] == dim2).OnlyEnforceIf(no_rotation)
        model.Add(x_sizes[i] == dim2).OnlyEnforceIf(rotated)
        model.Add(y_sizes[i] == dim1).OnlyEnforceIf(rotated)

        is_used.append(not_selected.Not())

    ## 2D no overlap.
    model.AddNoOverlap2D(x_intervals, y_intervals)

    # Objective.
    model.Maximize(cp_model.LinearExpr.ScalProd(is_used, item_values))

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
            'x_start': [solver.Value(x_starts[i]) for i in used],
            'y_start': [solver.Value(y_starts[i]) for i in used],
            'item_width': [solver.Value(x_sizes[i]) for i in used],
            'item_height': [solver.Value(y_sizes[i]) for i in used],
            'x_end': [solver.Value(x_ends[i]) for i in used],
            'y_end': [solver.Value(y_ends[i]) for i in used],
            'item_value': [item_values[i] for i in used]
        })
        print(data)


def main(_):
    """Solve the problem with all models."""
    data, max_height, max_width = build_data()
    if FLAGS.model == 'duplicate':
        solve_with_duplicate_items(data, max_height, max_width)
    elif FLAGS.model == 'optional':
        solve_with_duplicate_optional_items(data, max_height, max_width)
    else:
        solve_with_rotations(data, max_height, max_width)


if __name__ == '__main__':
    app.run(main)
