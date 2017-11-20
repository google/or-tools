from ortools.sat.python import cp_model


def main():
  # Creates the solver.
  model = cp_model.CpModel()

  machines_count = 6
  jobs_count = 6
  all_machines = range(0, machines_count)
  all_jobs = range(0, jobs_count)

  durations = [[1, 3, 6, 7, 3, 6],
               [8, 5, 10, 10, 10, 4],
               [5, 4, 8, 9, 1, 7],
               [5, 5, 5, 3, 8, 9],
               [9, 3, 5, 4, 3, 1],
               [3, 3, 9, 10, 4, 1]]

  machines = [[2, 0, 1, 3, 5, 4],
              [1, 2, 4, 5, 0, 3],
              [2, 3, 5, 0, 1, 4],
              [1, 0, 2, 3, 4, 5],
              [2, 1, 4, 5, 0, 3],
              [1, 3, 5, 0, 4, 2]]

  # Computes horizon dynamically.
  horizon = sum([sum(durations[i]) for i in all_jobs])

  # Creates jobs.
  all_tasks = {}
  for i in all_jobs:
    for j in all_machines:
      start = model.NewIntVar(0, horizon, 'start_%i_%i' % (i, j))
      duration = durations[i][j]
      end = model.NewIntVar(0, horizon, 'end_%i_%i' % (i, j))
      interval = model.NewIntervalVar(start, duration, end,
                                      'interval_%i_%i' % (i, j))
      all_tasks[(i, j)] = (start, end, interval)

  # Create disjuctive constraints.
  machine_to_jobs = {}
  for i in all_machines:
    machines_jobs = []
    for j in all_jobs:
      for k in all_machines:
        if machines[j][k] == i:
          machines_jobs.append(all_tasks[(j, k)][2])
    machine_to_jobs[i] = machines_jobs
    model.AddNoOverlap(machines_jobs)

  # Precedences inside a job.
  for i in all_jobs:
    for j in range(0, machines_count - 1):
      model.Add(all_tasks[(i, j + 1)][0] >= all_tasks[(i, j)][1])

  # Makespan objective.
  obj_var = model.NewIntVar(0, horizon, 'makespan')
  model.AddMaxEquality(
      obj_var, [all_tasks[(i, machines_count - 1)][1] for i in all_jobs])
  model.Minimize(obj_var)

  # Solve model.
  solver = cp_model.CpSolver()
  response = solver.Solve(model)
  print(solver.ObjectiveValue())


if __name__ == '__main__':
  main()
