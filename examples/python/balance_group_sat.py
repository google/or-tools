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
"""We are trying to group items in equal sized groups.

Each item has a color and a value. We want the sum of values of each group to
be as close to the average as possible.
Furthermore, if one color is an a group, at least k items with this color must
be in that group.
"""


from ortools.sat.python import cp_model


# Create a solution printer.
class SolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, values, colors, all_groups, all_items, item_in_group):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__solution_count = 0
        self.__values = values
        self.__colors = colors
        self.__all_groups = all_groups
        self.__all_items = all_items
        self.__item_in_group = item_in_group

    def on_solution_callback(self):
        print('Solution %i' % self.__solution_count)
        self.__solution_count += 1

        print('  objective value = %i' % self.ObjectiveValue())
        groups = {}
        sums = {}
        for g in self.__all_groups:
            groups[g] = []
            sums[g] = 0
            for item in self.__all_items:
                if self.BooleanValue(self.__item_in_group[(item, g)]):
                    groups[g].append(item)
                    sums[g] += self.__values[item]

        for g in self.__all_groups:
            group = groups[g]
            print('group %i: sum = %0.2f [' % (g, sums[g]), end='')
            for item in group:
                value = self.__values[item]
                color = self.__colors[item]
                print(' (%i, %i, %i)' % (item, value, color), end='')
            print(']')


def main():
    # Data.
    num_groups = 10
    num_items = 100
    num_colors = 3
    min_items_of_same_color_per_group = 4

    all_groups = range(num_groups)
    all_items = range(num_items)
    all_colors = range(num_colors)

    # Values for each items.
    values = [1 + i + (i * i // 200) for i in all_items]
    # Color for each item (simple modulo).
    colors = [i % num_colors for i in all_items]

    sum_of_values = sum(values)
    average_sum_per_group = sum_of_values // num_groups

    num_items_per_group = num_items // num_groups

    # Collect all items in a given color.
    items_per_color = {}
    for c in all_colors:
        items_per_color[c] = []
        for i in all_items:
            if colors[i] == c:
                items_per_color[c].append(i)

    print('Model has %i items, %i groups, and %i colors' %
          (num_items, num_groups, num_colors))
    print('  average sum per group = %i' % average_sum_per_group)

    # Model.

    model = cp_model.CpModel()

    item_in_group = {}
    for i in all_items:
        for g in all_groups:
            item_in_group[(i, g)] = model.NewBoolVar('item %d in group %d' %
                                                     (i, g))

    # Each group must have the same size.
    for g in all_groups:
        model.Add(
            sum(item_in_group[(i, g)]
                for i in all_items) == num_items_per_group)

    # One item must belong to exactly one group.
    for i in all_items:
        model.Add(sum(item_in_group[(i, g)] for g in all_groups) == 1)

    # The deviation of the sum of each items in a group against the average.
    e = model.NewIntVar(0, 550, 'epsilon')

    # Constrain the sum of values in one group around the average sum per group.
    for g in all_groups:
        model.Add(
            sum(item_in_group[(i, g)] * values[i]
                for i in all_items) <= average_sum_per_group + e)
        model.Add(
            sum(item_in_group[(i, g)] * values[i]
                for i in all_items) >= average_sum_per_group - e)

    # color_in_group variables.
    color_in_group = {}
    for g in all_groups:
        for c in all_colors:
            color_in_group[(c, g)] = model.NewBoolVar(
                'color %d is in group %d' % (c, g))

    # Item is in a group implies its color is in that group.
    for i in all_items:
        for g in all_groups:
            model.AddImplication(item_in_group[(i, g)],
                                 color_in_group[(colors[i], g)])

    # If a color is in a group, it must contains at least
    # min_items_of_same_color_per_group items from that color.
    for c in all_colors:
        for g in all_groups:
            literal = color_in_group[(c, g)]
            model.Add(
                sum(item_in_group[(i, g)] for i in items_per_color[c]) >=
                min_items_of_same_color_per_group).OnlyEnforceIf(literal)

    # Compute the maximum number of colors in a group.
    max_color = num_items_per_group // min_items_of_same_color_per_group
    # Redundant contraint: The problem does not solve in reasonable time without it.
    if max_color < num_colors:
        for g in all_groups:
            model.Add(
                sum(color_in_group[(c, g)] for c in all_colors) <= max_color)

    # Minimize epsilon
    model.Minimize(e)

    model.ExportToFile('balance_group_sat.pbtxt')

    solver = cp_model.CpSolver()
    solution_printer = SolutionPrinter(values, colors, all_groups, all_items,
                                       item_in_group)
    status = solver.Solve(model, solution_printer)

    if status == cp_model.OPTIMAL:
        print('Optimal epsilon: %i' % solver.ObjectiveValue())
        print('Statistics')
        print('  - conflicts : %i' % solver.NumConflicts())
        print('  - branches  : %i' % solver.NumBranches())
        print('  - wall time : %f s' % solver.WallTime())
    else:
        print('No solution found')


if __name__ == '__main__':
    main()
