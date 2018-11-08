from __future__ import print_function

# Import Python wrapper for or-tools CP-SAT solver.
from ortools.sat.python import cp_model

def main():
  # Create the model.
  model = cp_model.CpModel()

  machines_count = 3
  jobs_count = 3
  all_machines = range(0, machines_count)
  all_jobs = range(0, jobs_count)
  # Define data.
  machines = [[0, 1, 2],
              [0, 2, 1],
              [1, 2]]

  processing_times = [[3, 2, 2],
                      [2, 1, 4],
                      [4, 3]]

  # Computes horizon.
  horizon = 0
  for i in all_jobs:
    horizon += sum(processing_times[i])

  # Creates jobs.
  all_tasks = {}
  all_starts = {}
  all_ends = {}
  for i in all_jobs:
    for j in range(0, len(machines[i])):
      suffix = '%i_%i' % (i, j)
      start = model.NewIntVar(0, horizon, 'start' + suffix)
      end = model.NewIntVar(0, horizon, 'end' + suffix)
      interval = model.NewIntervalVar(start, processing_times[i][j], end,
                                      'Job' + suffix)
      all_starts[i, j] = start
      all_ends[i, j] = end
      all_tasks[i, j] = interval

  # Creates sequence variables and add disjunctive constraints.
  machines_jobs = {}
  for i in all_machines:

    machines_jobs[i] = []
    for j in all_jobs:
      for k in range(0, len(machines[j])):
        if machines[j][k] == i:
          machines_jobs[i].append(all_tasks[j, k])
    model.AddNoOverlap(machines_jobs[i])

  # Add precedence contraints.
  for i in all_jobs:
    for j in range(0, len(machines[i]) - 1):
      model.Add(all_starts[i, j + 1] >= all_ends[i, j])

  # Set the objective.
  makespan = model.NewIntVar(0, horizon, 'makespan')
  model.AddMaxEquality(makespan,
                       [all_ends[i, len(machines[i]) - 1] for i in all_jobs])
  model.Minimize(makespan)

  # Solve model.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)

  # Output solution.
  disp_col_width = 10
  if status == cp_model.OPTIMAL:
    print("\nOptimal Schedule Length:", solver.ObjectiveValue(), "\n")
    sol_line = ""
    sol_line_tasks = ""
    print("Optimal Schedule", "\n")

    for i in all_machines:
      jobs =
      sol_line += "Machine " + str(i) + ": "
      sol_line_tasks += "Machine " + str(i) + ": "
      sequence = collector.ForwardSequence(0, seq)
      seq_size = len(sequence)

      for j in range(0, seq_size):
        t = seq.Interval(sequence[j]);
         # Add spaces to output to align columns.
        sol_line_tasks +=  t.Name() + " " * (disp_col_width - len(t.Name()))

      for j in range(0, seq_size):
        t = seq.Interval(sequence[j]);
        sol_tmp = "[" + str(collector.Value(0, t.StartExpr().Var())) + ","
        sol_tmp += str(collector.Value(0, t.EndExpr().Var())) + "] "
        # Add spaces to output to align columns.
        sol_line += sol_tmp + " " * (disp_col_width - len(sol_tmp))

      sol_line += "\n"
      sol_line_tasks += "\n"

    print(sol_line_tasks)
    print("Time Intervals for Tasks\n")
    print(sol_line)


if __name__ == '__main__':
  main()
