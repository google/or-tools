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
"""This model implements a bus driver scheduling problem.

Constraints:
- max driving time per driver <= 9h
- max working time per driver <= 12h
- min working time per driver >= 6.5h (soft)
- 30 min break after each 4h of driving time per driver
- 10 min preparation time before the first shift
- 15 min cleaning time after the last shift
- 2 min waiting time after each shift for passenger boarding and alighting
"""
from __future__ import print_function

import math

from ortools.sat.python import cp_model

SAMPLE_SHIFTS = [
    #
    # column description:
    # - shift id
    # - shift start time as hh:mm string (for logging and readability purposes)
    # - shift end time as hh:mm string (for logging and readability purposes)
    # - shift start minute
    # - shift end minute
    # - shift duration in minutes
    #
    [0, '05:18', '06:00', 318, 360, 42],
    [1, '05:26', '06:08', 326, 368, 42],
    [2, '05:40', '05:56', 340, 356, 16],
    [3, '06:06', '06:51', 366, 411, 45],
    [4, '06:40', '07:52', 400, 472, 72],
    [5, '06:42', '07:13', 402, 433, 31],
    [6, '06:48', '08:15', 408, 495, 87],
    [7, '06:59', '08:07', 419, 487, 68],
    [8, '07:20', '07:36', 440, 456, 16],
    [9, '07:35', '08:22', 455, 502, 47],
    [10, '07:50', '08:55', 470, 535, 65],
    [11, '08:00', '09:05', 480, 545, 65],
    [12, '08:00', '08:35', 480, 515, 35],
    [13, '08:11', '09:41', 491, 581, 90],
    [14, '08:28', '08:50', 508, 530, 22],
    [15, '08:35', '08:45', 515, 525, 10],
    [16, '08:40', '08:50', 520, 530, 10],
    [17, '09:03', '10:28', 543, 628, 85],
    [18, '09:23', '09:49', 563, 589, 26],
    [19, '09:30', '09:40', 570, 580, 10],
    [20, '09:57', '10:20', 597, 620, 23],
    [21, '10:09', '11:03', 609, 663, 54],
    [22, '10:20', '10:30', 620, 630, 10],
    [23, '11:00', '11:10', 660, 670, 10],
    [24, '11:45', '12:24', 705, 744, 39],
    [25, '12:18', '13:00', 738, 780, 42],
    [26, '13:18', '14:44', 798, 884, 86],
    [27, '13:53', '14:49', 833, 889, 56],
    [28, '14:03', '14:50', 843, 890, 47],
    [29, '14:28', '15:15', 868, 915, 47],
    [30, '14:30', '15:41', 870, 941, 71],
    [31, '14:48', '15:35', 888, 935, 47],
    [32, '15:03', '15:50', 903, 950, 47],
    [33, '15:28', '16:54', 928, 1014, 86],
    [34, '15:38', '16:25', 938, 985, 47],
    [35, '15:40', '15:56', 940, 956, 16],
    [36, '15:58', '16:45', 958, 1005, 47],
    [37, '16:04', '17:30', 964, 1050, 86],
    [38, '16:28', '17:15', 988, 1035, 47],
    [39, '16:36', '17:21', 996, 1041, 45],
    [40, '16:50', '17:00', 1010, 1020, 10],
    [41, '16:54', '18:20', 1014, 1100, 86],
    [42, '17:01', '17:13', 1021, 1033, 12],
    [43, '17:19', '18:31', 1039, 1111, 72],
    [44, '17:23', '18:10', 1043, 1090, 47],
    [45, '17:34', '18:15', 1054, 1095, 41],
    [46, '18:04', '19:29', 1084, 1169, 85],
    [47, '18:34', '19:58', 1114, 1198, 84],
    [48, '19:56', '20:34', 1196, 1234, 38],
    [49, '20:05', '20:48', 1205, 1248, 43]
]


def bus_driver_scheduling(minimize_drivers, max_num_drivers):
    """Optimize the bus driver scheduling problem.

    This model has to modes.
    If minimize_drivers == True, the objective will be to find the minimal
    number of drivers, independently of the working times of each drivers.

    Otherwise, will will create max_num_drivers non optional drivers, and
    minimize the sum of working times of these drivers.
    """
    num_shifts = len(SAMPLE_SHIFTS)

    # All durations are in minutes.
    max_driving_time = 540  # 8 hours.
    max_driving_time_without_pauses = 240  # 4 hours
    min_pause_after_4h = 30
    min_delay_between_shifts = 2
    max_working_time = 720
    min_working_time = 390  # 6.5 hours
    setup_time = 10
    cleanup_time = 15

    # Computed data.
    total_driving_time = sum(shift[5] for shift in SAMPLE_SHIFTS)
    min_num_drivers = int(
        math.ceil(total_driving_time * 1.0 / max_driving_time))
    num_drivers = 2 * min_num_drivers if minimize_drivers else max_num_drivers
    min_start_time = min(shift[3] for shift in SAMPLE_SHIFTS)
    max_end_time = max(shift[4] for shift in SAMPLE_SHIFTS)

    print('Bus driver scheduling')
    print('  num shifts =', num_shifts)
    print('  total driving time =', total_driving_time, 'minutes')
    print('  min num drivers =', min_num_drivers)
    print('  num drivers =', num_drivers)
    print('  min start time =', min_start_time)
    print('  max end time =', max_end_time)

    model = cp_model.CpModel()

    # For each driver and each shift, we store:
    #   - the total driving time including this shift
    #   - the acrued driving time since the last 30 minute break
    # Special arcs have the following effect:
    #   - 'from source to shift' sets the starting time and accumulate the first
    #      shift
    #   - 'from shift to end' sets the ending time, and fill the driving_times
    #      variable
    # Arcs between two shifts have the following impact
    #   - add the duration of the shift to the total driving time
    #   - reset the accumulated driving time if the distance between the two
    #     shifts is more than 30 minutes, add the duration of the shift to the
    #     accumulated driving time since the last break otherwise

    # Per (driver, node) info (driving time, performed,
    # driving time since break)
    total_driving = {}
    no_break_driving = {}
    performed = {}
    starting_shifts = {}

    # Per driver info (start, end, driving times, is working)
    start_times = []
    end_times = []
    driving_times = []
    working_drivers = []

    # Weighted Objective 
    delay_literals = []
    delay_weights = []

    for d in range(num_drivers):
        start_times.append(
            model.NewIntVar(min_start_time, max_end_time, 'start_%i' % d))
        end_times.append(
            model.NewIntVar(min_start_time, max_end_time, 'end_%i' % d))
        driving_times.append(
            model.NewIntVar(0, max_driving_time, 'driving_%i' % d))
        arcs = []

        # Create all the shift variables before iterating on the transitions
        # between these shifts.
        for s in range(num_shifts):
            total_driving[d, s] = model.NewIntVar(0, max_driving_time,
                                                  'dr_%i_%i' % (d, s))
            no_break_driving[d, s] = model.NewIntVar(
                0, max_driving_time_without_pauses, 'mdr_%i_%i' % (d, s))
            visit_node = model.NewBoolVar('performed_%i_%i' % (d, s))
            performed[d, s] = visit_node

        for s in range(num_shifts):
            shift = SAMPLE_SHIFTS[s]
            duration = shift[5]
            visit_node = performed[d, s]

            # Arc from source to shift.
            #    - set the start time of the driver
            #    - increase driving time and driving time since break
            source_lit = model.NewBoolVar('%i from source to %i' % (d, s))
            arcs.append([0, s + 1, source_lit])
            model.Add(start_times[d] == shift[3]).OnlyEnforceIf(source_lit)
            model.Add(
                total_driving[d, s] == duration).OnlyEnforceIf(source_lit)
            model.Add(
                no_break_driving[d, s] == duration).OnlyEnforceIf(source_lit)
            starting_shifts[d, s] = source_lit

            # Arc from shift to sink
            #    - set the end time of the driver
            #    - set the driving times of the driver
            sink_lit = model.NewBoolVar('%i from %i to sink' % (d, s))
            arcs.append([s + 1, 0, sink_lit])
            model.Add(end_times[d] == shift[4]).OnlyEnforceIf(sink_lit)
            model.Add(driving_times[d] == total_driving[d, s]).OnlyEnforceIf(
                sink_lit)

            # Node not performed
            #    - set both driving times to 0
            #    - add a looping arc on the node
            model.Add(total_driving[d, s] == 0).OnlyEnforceIf(visit_node.Not())
            model.Add(no_break_driving[d, s] == 0).OnlyEnforceIf(
                visit_node.Not())
            arcs.append([s + 1, s + 1, visit_node.Not()])

            for o in range(num_shifts):
                other = SAMPLE_SHIFTS[o]
                delay = other[3] - shift[4]
                if delay < +min_delay_between_shifts:
                    continue
                lit = model.NewBoolVar('%i from %i to %i' % (d, s, o))

                # Increase driving time
                model.Add(total_driving[d, o] ==
                          total_driving[d, s] + other[5]).OnlyEnforceIf(lit)

                # Increase no_break_driving or reset it to 0 depending on the delay
                if delay >= min_pause_after_4h:
                    model.Add(
                        no_break_driving[d, o] == other[5]).OnlyEnforceIf(lit)
                else:
                    model.Add(no_break_driving[d, o] == no_break_driving[d, s]
                              + other[5]).OnlyEnforceIf(lit)

                # Add arc
                arcs.append([s + 1, o + 1, lit])

                # Cost part
                delay_literals.append(lit)
                delay_weights.append(delay)

        if minimize_drivers:
            # Driver is not working.
            working = model.NewBoolVar('working_%i' % d)
            model.Add(start_times[d] == min_start_time).OnlyEnforceIf(
                working.Not())
            model.Add(end_times[d] == min_start_time).OnlyEnforceIf(
                working.Not())
            model.Add(driving_times[d] == 0).OnlyEnforceIf(working.Not())
            working_drivers.append(working)
            arcs.append([0, 0, working.Not()])
            # Conditional working time constraints
            model.Add(end_times[d] - start_times[d] + setup_time + cleanup_time
                      <= max_working_time)  # no need for enforcement
            model.Add(end_times[d] - start_times[d] + setup_time + cleanup_time
                      >= min_working_time).OnlyEnforceIf(working)
        else:
            # Working time constraints
            model.Add(end_times[d] - start_times[d] <=
                      max_working_time - setup_time - cleanup_time)
            model.Add(end_times[d] - start_times[d] >=
                      min_working_time - setup_time - cleanup_time)

        # Create circuit constraint.
        model.AddCircuit(arcs)

    # Each shift is covered.
    for s in range(num_shifts):
        model.Add(sum(performed[d, s] for d in range(num_drivers)) == 1)

    # Symmetry breaking

    # The first 3 shifts must be performed by 3 different drivers.
    # Let's assign them to the first 3 drivers in sequence
    model.Add(starting_shifts[0, 0] == 1)
    model.Add(starting_shifts[1, 1] == 1)
    model.Add(starting_shifts[2, 2] == 1)

    if minimize_drivers:
        # Push non working drivers to the end
        for d in range(num_drivers - 1):
            model.AddImplication(working_drivers[d].Not(),
                                 working_drivers[d + 1].Not())

    # Redundant constraints: sum of driving times = sum of shift driving times
    model.Add(sum(driving_times) == total_driving_time)

    if minimize_drivers:
        # Minimize the number of working drivers
        model.Minimize(sum(working_drivers))
    else:
        # Minimize the sum of delays between tasks, which in turns minimize the
        # sum of working times as the total driving time is fixed
        model.Minimize(
            cp_model.LinearExpr.ScalProd(delay_literals, delay_weights))

    # Solve model.
    solver = cp_model.CpSolver()
    solver.parameters.log_search_progress = not minimize_drivers
    solver.parameters.num_search_workers = 24
    status = solver.Solve(model)

    if status != cp_model.OPTIMAL and status != cp_model.FEASIBLE:
        return -1

    # Display solution
    if minimize_drivers:
        max_num_drivers = int(solver.ObjectiveValue())
        print('minimal number of drivers =', max_num_drivers)
        return max_num_drivers

    for d in range(num_drivers):
        print('Driver %i: ' % (d + 1))
        print('  total driving time =', solver.Value(driving_times[d]))
        first = True
        start_time = max_end_time
        end_time = 0
        for s in range(num_shifts):
            shift = SAMPLE_SHIFTS[s]

            if not solver.BooleanValue(performed[d, s]):
                continue

            start_time = min(start_time, shift[3])
            end_time = max(end_time, shift[4])

            # Hack to detect if the waiting time between the last shift and
            # this one exceeds 30 minutes. For this, we look at the
            # no_break_driving which was reinitialized in that case.
            if solver.Value(no_break_driving[d, s]) == shift[5] and not first:
                print('    **break**')
            print('    shift ', shift[0], ':', shift[1], "-", shift[2])
            first = False

        print('  working time =',
              end_time - start_time + setup_time + cleanup_time)

    return int(solver.ObjectiveValue())


def optimize_bus_driver_allocation():
    """Optimize the bus driver allocation in two passes."""
    print('----------- first pass: minimize the number of drivers')
    num_drivers = bus_driver_scheduling(True, -1)
    print('----------- second pass: minimize the sum of working times')
    bus_driver_scheduling(False, num_drivers)


optimize_bus_driver_allocation()
