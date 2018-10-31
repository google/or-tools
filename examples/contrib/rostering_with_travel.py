from ortools.sat.python import cp_model


def SolveRosteringWithTravel():
  model = cp_model.CpModel()

  # [duration, start, end, location]
  jobs = [[3, 0, 6, 1], [5, 0, 6, 0], [1, 3, 7, 1], [1, 3, 5, 0], [3, 0, 3, 0],
          [3, 0, 8, 0]]

  max_length = 20

  num_machines = 3
  all_machines = range(num_machines)

  horizon = 20
  travel_time = 1
  num_jobs = len(jobs)
  all_jobs = range(num_jobs)

  intervals = []
  optional_intervals = []
  performed = []
  starts = []
  ends = []
  travels = []

  for m in all_machines:
    optional_intervals.append([])

  for i in all_jobs:
    # Create main interval.
    start = model.NewIntVar(jobs[i][1], horizon, 'start_%i' % i)
    duration = jobs[i][0]
    end = model.NewIntVar(0, jobs[i][2], 'end_%i' % i)
    interval = model.NewIntervalVar(start, duration, end, 'interval_%i' % i)
    starts.append(start)
    intervals.append(interval)
    ends.append(end)

    job_performed = []
    job_travels = []
    for m in all_machines:
      performed_on_m = model.NewBoolVar('perform_%i_on_m%i' % (i, m))
      job_performed.append(performed_on_m)

      # Create an optional copy of interval to be executed on a machine
      location0 = model.NewIntVar(jobs[i][3], jobs[i][3],
                                  'location_%i_on_m%i' % (i, m))
      start0 = model.NewIntVar(jobs[i][1], horizon, 'start_%i_on_m%i' % (i, m))
      end0 = model.NewIntVar(0, jobs[i][2], 'end_%i_on_m%i' % (i, m))
      interval0 = model.NewOptionalIntervalVar(
          start0, duration, end0, performed_on_m, 'interval_%i_on_m%i' % (i, m))
      optional_intervals[m].append(interval0)

      # We only propagate the constraint if the tasks is performed on the machine.
      model.Add(start0 == start).OnlyEnforceIf(performed_on_m)
      # Adding travel constraint
      travel = model.NewBoolVar('is_travel_%i_on_m%i' % (i, m))
      startT = model.NewIntVar(0, horizon, 'start_%i_on_m%i' % (i, m))
      endT = model.NewIntVar(0, horizon, 'end_%i_on_m%i' % (i, m))
      intervalT = model.NewOptionalIntervalVar(
          startT, travel_time, endT, travel,
          'travel_interval_%i_on_m%i' % (i, m))
      optional_intervals[m].append(intervalT)
      job_travels.append(travel)

      model.Add(end0 == startT).OnlyEnforceIf(travel)

    performed.append(job_performed)
    travels.append(job_travels)

    model.Add(sum(job_performed) == 1)

  for m in all_machines:
    if m == 1:
      for i in all_jobs:
        if i == 2:
          for c in all_jobs:
            if (i != c) and (jobs[i][3] != jobs[c][3]):
              is_job_earlier = model.NewBoolVar('is_j%i_earlier_j%i' % (i, c))
              model.Add(starts[i] < starts[c]).OnlyEnforceIf(is_job_earlier)
              model.Add(starts[i] >= starts[c]).OnlyEnforceIf(
                  is_job_earlier.Not())

  # Max Length constraint (modeled as a cumulative)
  # model.AddCumulative(intervals, demands, max_length)

  # Choose which machine to perform the jobs on.
  for m in all_machines:
    model.AddNoOverlap(optional_intervals[m])

  # Objective variable.
  total_cost = model.NewIntVar(0, 1000, 'cost')
  model.Add(total_cost == sum(
      performed[j][m] * (10 * (m + 1)) for j in all_jobs for m in all_machines))
  model.Minimize(total_cost)

  # Solve model.
  solver = cp_model.CpSolver()
  result = solver.Solve(model)

  print()
  print(result)
  print('Statistics')
  print('  - conflicts       : %i' % solver.NumConflicts())
  print('  - branches        : %i' % solver.NumBranches())
  print('  - wall time       : %f ms' % solver.WallTime())


def main():
  SolveRosteringWithTravel()


if __name__ == '__main__':
  main()
