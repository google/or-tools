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
"""Fill a 72x37 rectangle by a minimum number of non-overlapping squares."""


from ortools.sat.python import cp_model


def cover_rectangle(num_squares):
    """Try to fill the rectangle with a given number of squares."""
    size_x = 72
    size_y = 37

    model = cp_model.CpModel()

    areas = []
    sizes = []
    x_intervals = []
    y_intervals = []
    x_starts = []
    y_starts = []

    # Creates intervals for the NoOverlap2D and size variables.
    for i in range(num_squares):
        size = model.NewIntVar(1, size_y, 'size_%i' % i)
        start_x = model.NewIntVar(0, size_x, 'sx_%i' % i)
        end_x = model.NewIntVar(0, size_x, 'ex_%i' % i)
        start_y = model.NewIntVar(0, size_y, 'sy_%i' % i)
        end_y = model.NewIntVar(0, size_y, 'ey_%i' % i)

        interval_x = model.NewIntervalVar(start_x, size, end_x, 'ix_%i' % i)
        interval_y = model.NewIntervalVar(start_y, size, end_y, 'iy_%i' % i)

        area = model.NewIntVar(1, size_y * size_y, 'area_%i' % i)
        model.AddProdEquality(area, [size, size])

        areas.append(area)
        x_intervals.append(interval_x)
        y_intervals.append(interval_y)
        sizes.append(size)
        x_starts.append(start_x)
        y_starts.append(start_y)

    # Main constraint.
    model.AddNoOverlap2D(x_intervals, y_intervals)

    # Redundant constraints.
    model.AddCumulative(x_intervals, sizes, size_y)
    model.AddCumulative(y_intervals, sizes, size_x)

    # Forces the rectangle to be exactly covered.
    model.Add(sum(areas) == size_x * size_y)

    # Symmetry breaking 1: sizes are ordered.
    for i in range(num_squares - 1):
        model.Add(sizes[i] <= sizes[i + 1])

        # Define same to be true iff sizes[i] == sizes[i + 1]
        same = model.NewBoolVar('')
        model.Add(sizes[i] == sizes[i + 1]).OnlyEnforceIf(same)
        model.Add(sizes[i] < sizes[i + 1]).OnlyEnforceIf(same.Not())

        # Tie break with starts.
        model.Add(x_starts[i] <= x_starts[i + 1]).OnlyEnforceIf(same)

    # Symmetry breaking 2: first square in one quadrant.
    model.Add(x_starts[0] < 36)
    model.Add(y_starts[0] < 19)

    # Creates a solver and solves.
    solver = cp_model.CpSolver()
    status = solver.Solve(model)
    print('%s found in %0.2fs' % (solver.StatusName(status), solver.WallTime()))

    # Prints solution.
    if status == cp_model.OPTIMAL:
        display = [[' ' for _ in range(size_x)] for _ in range(size_y)]
        for i in range(num_squares):
            sol_x = solver.Value(x_starts[i])
            sol_y = solver.Value(y_starts[i])
            sol_s = solver.Value(sizes[i])
            char = format(i, '01x')
            for j in range(sol_s):
                for k in range(sol_s):
                    if display[sol_y + j][sol_x + k] != ' ':
                        print('ERROR between %s and %s' %
                              (display[sol_y + j][sol_x + k], char))
                    display[sol_y + j][sol_x + k] = char

        for line in range(size_y):
            print(' '.join(display[line]))
    return status == cp_model.FEASIBLE


for num in range(1, 15):
    print('Trying with size =', num)
    if cover_rectangle(num):
        break
