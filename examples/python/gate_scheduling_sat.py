# Copyright 2010-2017 Google
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

"""Gate Scheduling problem.

We have a set of jobs to perform (duration, width).
We have two parallel machines that can perform this job.
One machine can only perform one job at a time.
At any point in time, the sum of the width of the two active jobs does not
exceed a max_width.

The objective is to minimize the max end time of all jobs.
"""

from ortools.sat.python import cp_model


def main():
  model = cp_model.CpModel()

  jobs = [[3, 3],
          [2, 5],
          [1, 3],
          [3, 7],
          [7, 3],
          [2, 2],
          [2, 2],
          [5, 5],
          [10, 2],
          [4, 3],
          [2, 6],
          [1, 2],
          [6, 8],
          [4, 5],
          [3, 7]]

  max_length = 10

  horizon = sum(t[0] for t in jobs)
  num_jobs = len(jobs)
  all_jobs = range(num_jobs)

  intervals = []
  intervals0 = []
  intervals1 = []
  performed = []
  starts = []
  ends = []
  demands = []

  for i in all_jobs:
    # Create main interval.
    start = model.NewIntVar(0, horizon, 'start_%i' % i)
    duration = jobs[i][0]
    end = model.NewIntVar(0, horizon, 'end_%i' % i)
    interval = model.NewIntervalVar(start, duration, end, 'interval_%i' % i)
    starts.append(start)
    intervals.append(interval)
    ends.append(end)
    demands.append(jobs[i][1])

    performed_on_m0 = model.NewBoolVar('perform_%i_on_m0' % i)
    performed.append(performed_on_m0)

    # Create an optional copy of interval to be executed on machine 0.
    start0 = model.NewOptionalIntVar(
        0, horizon, performed_on_m0, 'start_%i_on_m0' % i)
    end0 = model.NewOptionalIntVar(
        0, horizon, performed_on_m0, 'end_%i_on_m0' % i)
    interval0 = model.NewOptionalIntervalVar(
        start0, duration, end0, performed_on_m0, 'interval_%i_on_m0' % i)
    intervals0.append(interval0)

    # Create an optional copy of interval to be executed on machine 1.
    start1 = model.NewOptionalIntVar(
        0, horizon, performed_on_m0.Not(), 'start_%i_on_m1' % i)
    end1 = model.NewOptionalIntVar(
        0, horizon, performed_on_m0.Not(), 'end_%i_on_m1' % i)
    interval1 = model.NewOptionalIntervalVar(
        start1, duration, end1, performed_on_m0.Not(), 'interval_%i_on_m1' % i)
    intervals1.append(interval1)

    # We only propagate the constraint if the tasks is performed on the machine.
    model.Add(start0 == start).OnlyEnforceIf(performed_on_m0)
    model.Add(start1 == start).OnlyEnforceIf(performed_on_m0.Not())

  # Max Length constraint (modeled as a cumulative)
  model.AddCumulative(intervals, demands, max_length)

  # Choose which machine to perform the jobs on.
  model.AddNoOverlap(intervals0)
  model.AddNoOverlap(intervals1)

  # Objective variable.
  makespan = model.NewIntVar(0, horizon, 'makespan')
  model.AddMaxEquality(makespan, ends)
  model.Minimize(makespan)

  # Symmetry breaking.
  model.Add(performed[0] == 0)

  # Solve model.
  solver = cp_model.CpSolver()
  solver.Solve(model)
  print('Solution')
  print('  - makespan = %i' % solver.ObjectiveValue())
  for i in all_jobs:
    performed_machine = 1 - solver.Value(performed[i])
    start = solver.Value(starts[i])
    print('  - Job %i starts at %i on machine %i' %
          (i, start, performed_machine))
  print('Statistics')
  print('  - conflicts : %i' % solver.NumConflicts())
  print('  - branches  : %i' % solver.NumBranches())
  print('  - wall time : %f ms' % solver.WallTime())


if __name__ == '__main__':
  main()
