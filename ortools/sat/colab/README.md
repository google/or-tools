# CpModel Notebook

Below you'll find three examples of Google's CP-SAT solver.

-   The first example solves a [logic puzzle](#hidato)
-   The second solves a simple [job shop problem](#jobshop)
-   The third example solves a [two machine scheduling problem with shared
    resources](#twomachine)

## Local instance

Build and run locally:

```
bazel run -c opt ortools/python:ortools_notebook
```

This will open a jupyter notebook in your browser.

To use it as a server only, use the command

```
bazel run -c opt ortools/python:ortools_notebook -- --no-browser
```

And paste the resulting url in your favorite environment, like visual studio code.

## Hidato Problem {#hidato}

We model the Hidato problem (https://en.wikipedia.org/wiki/Hidato).

```
from google3.util.operations_research.sat.python import cp_model
from google3.util.operations_research.sat.colab import visualization


def BuildPairs(rows, cols):
  """Build closeness pairs for consecutive numbers.

  Build set of allowed pairs such that two consecutive numbers touch
  each other in the grid.

  Returns:
    A list of pairs for allowed consecutive position of numbers.

  Args:
    rows: the number of rows in the grid
    cols: the number of columns in the grid
  """
  return [(x * cols + y, (x + dx) * cols + (y + dy))
          for x in range(rows) for y in range(cols)
          for dx in (-1, 0, 1) for dy in (-1, 0, 1)
          if (x + dx >= 0 and x + dx < rows and
              y + dy >= 0 and y + dy < cols and (dx != 0 or dy != 0))]


def BuildPuzzle(problem):
  #
  # models, a 0 indicates an open cell which number is not yet known.
  #
  #
  puzzle = None
  if problem == 1:
    # Simple problem
    puzzle = [[6, 0, 9],
              [0, 2, 8],
              [1, 0, 0]]

  elif problem == 2:
    puzzle = [[0, 44, 41, 0, 0, 0, 0],
              [0, 43, 0, 28, 29, 0, 0],
              [0, 1, 0, 0, 0, 33, 0],
              [0, 2, 25, 4, 34, 0, 36],
              [49, 16, 0, 23, 0, 0, 0],
              [0, 19, 0, 0, 12, 7, 0],
              [0, 0, 0, 14, 0, 0, 0]]

  elif problem == 3:
    # Problems from the book:
    # Gyora Bededek: "Hidato: 2000 Pure Logic Puzzles"
    # Problem 1 (Practice)
    puzzle = [[0, 0, 20, 0, 0],
              [0, 0, 0, 16, 18],
              [22, 0, 15, 0, 0],
              [23, 0, 1, 14, 11],
              [0, 25, 0, 0, 12]]

  elif problem == 4:
    # problem 2 (Practice)
    puzzle = [[0, 0, 0, 0, 14],
              [0, 18, 12, 0, 0],
              [0, 0, 17, 4, 5],
              [0, 0, 7, 0, 0],
              [9, 8, 25, 1, 0]]

  elif problem == 5:
    # problem 3 (Beginner)
    puzzle = [[0, 26, 0, 0, 0, 18],
              [0, 0, 27, 0, 0, 19],
              [31, 23, 0, 0, 14, 0],
              [0, 33, 8, 0, 15, 1],
              [0, 0, 0, 5, 0, 0],
              [35, 36, 0, 10, 0, 0]]
  elif problem == 6:
    # Problem 15 (Intermediate)
    puzzle = [[64, 0, 0, 0, 0, 0, 0, 0],
              [1, 63, 0, 59, 15, 57, 53, 0],
              [0, 4, 0, 14, 0, 0, 0, 0],
              [3, 0, 11, 0, 20, 19, 0, 50],
              [0, 0, 0, 0, 22, 0, 48, 40],
              [9, 0, 0, 32, 23, 0, 0, 41],
              [27, 0, 0, 0, 36, 0, 46, 0],
              [28, 30, 0, 35, 0, 0, 0, 0]]
  return puzzle


def SolveHidato(puzzle, index):
  """Solve the given hidato table."""
  # Create the model.
  model = cp_model.CpModel()

  r = len(puzzle)
  c = len(puzzle[0])

  #
  # declare variables
  #
  positions = [model.NewIntVar(0, r * c - 1, 'p[%i]' % i)
               for i in range(r * c)]

  #
  # constraints
  #
  model.AddAllDifferent(positions)

  #
  # Fill in the clues
  #
  for i in range(r):
    for j in range(c):
      if puzzle[i][j] > 0:
        model.Add(positions[puzzle[i][j] - 1] == i * c + j)

  # Consecutive numbers must touch each other in the grid.
  # We use an allowed assignment constraint to model it.
  close_tuples = BuildPairs(r, c)
  for k in range(0, r * c - 1):
    model.AddAllowedAssignments([positions[k], positions[k + 1]], close_tuples)

  #
  # solution and search
  #

  solver = cp_model.CpSolver()
  status = solver.Solve(model)

  if status == cp_model.FEASIBLE:
    output = visualization.SvgWrapper(10, r, 40.0)
    for i in range(len(positions)):
      val = solver.Value(positions[i])
      x = val % c
      y = val // c
      color = 'white' if puzzle[y][x] == 0 else 'lightgreen'
      value = solver.Value(positions[i])
      output.AddRectangle(x, r - y - 1, 1, 1, color, 'black', str(i + 1))

    output.AddTitle('Puzzle %i solved in %f s' % (index, solver.WallTime()))
    output.Display()


for i in range(1, 7):
  SolveHidato(BuildPuzzle(i), i)
```

## Jobshop example {#jobshop}

This example demonstrates jobshop scheduling
(https://en.wikipedia.org/wiki/Job_shop_scheduling)

```python
from google3.util.operations_research.sat.python import cp_model
from google3.util.operations_research.sat.colab import visualization

def JobshopFT06():
  """Solves the ft06 jobshop from the jssp library.

  (http://people.brunel.ac.uk/~mastjjb/jeb/orlib/jobshopinfo.html)."""

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

  # Create disjunctive constraints.
  for i in all_machines:
    machines_jobs = []
    for j in all_jobs:
      for k in all_machines:
        if machines[j][k] == i:
          machines_jobs.append(all_tasks[(j, k)][2])
    model.AddNoOverlap(machines_jobs)

  # Makespan objective: minimize the total length of the schedule.
  obj_var = model.NewIntVar(0, horizon, 'makespan')
  model.AddMaxEquality(
      obj_var, [all_tasks[(i, machines_count - 1)][1] for i in all_jobs])
  model.Minimize(obj_var)

  # Precedences inside a job.
  for i in all_jobs:
    for j in range(0, machines_count - 1):
      model.Add(all_tasks[(i, j + 1)][0] >= all_tasks[(i, j)][1])

  solver = cp_model.CpSolver()
  response = solver.Solve(model)
  starts = [[solver.Value(all_tasks[(i, j)][0]) for j in all_machines]
            for i in all_jobs]
  visualization.DisplayJobshop(starts, durations, machines, 'FT06')


JobshopFT06()
```

## Two Machine Scheduling {#twomachine}

We have a set of jobs to perform (duration, width). We have two parallel
machines that can perform this job. One machine can only perform one job at a
time. At any point in time, the sum of the width of the two active jobs cannot
exceed a max_width.

The objective is to minimize the max end time of all jobs.

```
from google3.util.operations_research.sat.python import cp_model
from google3.util.operations_research.sat.colab import visualization


def TwoMachineScheduling():
  model = cp_model.CpModel()

  jobs = [[3, 3],  # (duration, width)
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

  max_width = 10

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
    # Create main interval (to be used in the cumulative constraint).
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
    start0 = model.NewIntVar(
        0, horizon, 'start_%i_on_m0' % i)
    end0 = model.NewIntVar(
        0, horizon, 'end_%i_on_m0' % i)
    interval0 = model.NewOptionalIntervalVar(
        start0, duration, end0, performed_on_m0, 'interval_%i_on_m0' % i)
    intervals0.append(interval0)

    # Create an optional copy of interval to be executed on machine 1.
    start1 = model.NewIntVar(
        0, horizon, 'start_%i_on_m1' % i)
    end1 = model.NewIntVar(
        0, horizon, 'end_%i_on_m1' % i)
    interval1 = model.NewOptionalIntervalVar(
        start1, duration, end1, performed_on_m0.Not(), 'interval_%i_on_m1' % i)
    intervals1.append(interval1)

    # We only propagate the constraint if the task is performed on the machine.
    model.Add(start0 == start).OnlyEnforceIf(performed_on_m0)
    model.Add(start1 == start).OnlyEnforceIf(performed_on_m0.Not())

  # Max width constraint (modeled as a cumulative).
  model.AddCumulative(intervals, demands, max_width)

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

  # Output solution.
  output = visualization.SvgWrapper(solver.ObjectiveValue(), max_width, 40.0)
  output.AddTitle('Makespan = %i' % solver.ObjectiveValue())
  color_manager = visualization.ColorManager()
  color_manager.SeedRandomColor(0)
  for i in all_jobs:
    performed_machine = 1 - solver.Value(performed[i])
    start = solver.Value(starts[i])
    dx = jobs[i][0]
    dy = jobs[i][1]
    sy = performed_machine * (max_width - dy)
    output.AddRectangle(start, sy, dx, dy, color_manager.RandomColor(), 'black',
                        'j%i' % i)

  output.AddXScale()
  output.AddYScale()
  output.Display()


TwoMachineScheduling()
```
