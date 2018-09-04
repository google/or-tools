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
"""SAT code samples used in documentation."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import collections

from ortools.sat.python import cp_model


def CodeSample():
  model = cp_model.CpModel()
  x = model.NewBoolVar('x')
  print(x)


def LiteralSample():
  model = cp_model.CpModel()
  x = model.NewBoolVar('x')
  not_x = x.Not()
  print(x)
  print(not_x)


def BoolOrSample():
  model = cp_model.CpModel()

  x = model.NewBoolVar('x')
  y = model.NewBoolVar('y')

  model.AddBoolOr([x, y.Not()])


def ReifiedSample():
  """Showcase creating a reified constraint."""
  model = cp_model.CpModel()

  x = model.NewBoolVar('x')
  y = model.NewBoolVar('y')
  b = model.NewBoolVar('b')

  # First version using a half-reified bool and.
  model.AddBoolAnd([x, y.Not()]).OnlyEnforceIf(b)

  # Second version using implications.
  model.AddImplication(b, x)
  model.AddImplication(b, y.Not())

  # Third version using bool or.
  model.AddBoolOr([b.Not(), x])
  model.AddBoolOr([b.Not(), y.Not()])


def RabbitsAndPheasants():
  """Solves the rabbits + pheasants problem."""
  model = cp_model.CpModel()

  r = model.NewIntVar(0, 100, 'r')
  p = model.NewIntVar(0, 100, 'p')

  # 20 heads.
  model.Add(r + p == 20)
  # 56 legs.
  model.Add(4 * r + 2 * p == 56)

  # Solves and prints out the solution.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)

  if status == cp_model.FEASIBLE:
    print('%i rabbits and %i pheasants' % (solver.Value(r), solver.Value(p)))


def BinpackingProblem():
  """Solves a bin-packing problem."""
  # Data.
  bin_capacity = 100
  slack_capacity = 20
  num_bins = 10
  all_bins = range(num_bins)

  items = [(20, 12), (15, 12), (30, 8), (45, 5)]
  num_items = len(items)
  all_items = range(num_items)

  # Model.
  model = cp_model.CpModel()

  # Main variables.
  x = {}
  for i in all_items:
    num_copies = items[i][1]
    for b in all_bins:
      x[(i, b)] = model.NewIntVar(0, num_copies, 'x_%i_%i' % (i, b))

  # Load variables.
  load = [model.NewIntVar(0, bin_capacity, 'load_%i' % b) for b in all_bins]

  # Slack variables.
  slacks = [model.NewBoolVar('slack_%i' % b) for b in all_bins]

  # Links load and x.
  for b in all_bins:
    model.Add(load[b] == sum(x[(i, b)] * items[i][0] for i in all_items))

  # Place all items.
  for i in all_items:
    model.Add(sum(x[(i, b)] for b in all_bins) == items[i][1])

  # Links load and slack through an equivalence relation.
  safe_capacity = bin_capacity - slack_capacity
  for b in all_bins:
    # slack[b] => load[b] <= safe_capacity.
    model.Add(load[b] <= safe_capacity).OnlyEnforceIf(slacks[b])
    # not(slack[b]) => load[b] > safe_capacity.
    model.Add(load[b] > safe_capacity).OnlyEnforceIf(slacks[b].Not())

  # Maximize sum of slacks.
  model.Maximize(sum(slacks))

  # Solves and prints out the solution.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)
  print('Solve status: %s' % solver.StatusName(status))
  if status == cp_model.OPTIMAL:
    print('Optimal objective value: %i' % solver.ObjectiveValue())
  print('Statistics')
  print('  - conflicts : %i' % solver.NumConflicts())
  print('  - branches  : %i' % solver.NumBranches())
  print('  - wall time : %f s' % solver.WallTime())


def IntervalSample():
  model = cp_model.CpModel()
  horizon = 100
  start_var = model.NewIntVar(0, horizon, 'start')
  duration = 10  # Python cp/sat code accept integer variables or constants.
  end_var = model.NewIntVar(0, horizon, 'end')
  interval_var = model.NewIntervalVar(start_var, duration, end_var, 'interval')
  print('start = %s, duration = %i, end = %s, interval = %s' %
        (start_var, duration, end_var, interval_var))


def OptionalIntervalSample():
  model = cp_model.CpModel()
  horizon = 100
  start_var = model.NewIntVar(0, horizon, 'start')
  duration = 10  # Python cp/sat code accept integer variables or constants.
  end_var = model.NewIntVar(0, horizon, 'end')
  presence_var = model.NewBoolVar('presence')
  interval_var = model.NewOptionalIntervalVar(start_var, duration, end_var,
                                              presence_var, 'interval')
  print('start = %s, duration = %i, end = %s, presence = %s, interval = %s' %
        (start_var, duration, end_var, presence_var, interval_var))


def MinimalCpSat():
  """Minimal CP-SAT example to showcase calling the solver."""
  # Creates the model.
  model = cp_model.CpModel()
  # Creates the variables.
  num_vals = 3
  x = model.NewIntVar(0, num_vals - 1, 'x')
  y = model.NewIntVar(0, num_vals - 1, 'y')
  z = model.NewIntVar(0, num_vals - 1, 'z')
  # Creates the constraints.
  model.Add(x != y)

  # Creates a solver and solves the model.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)

  if status == cp_model.FEASIBLE:
    print('x = %i' % solver.Value(x))
    print('y = %i' % solver.Value(y))
    print('z = %i' % solver.Value(z))


def MinimalCpSatWithTimeLimit():
  """Minimal CP-SAT example to showcase calling the solver."""
  # Creates the model.
  model = cp_model.CpModel()
  # Creates the variables.
  num_vals = 3
  x = model.NewIntVar(0, num_vals - 1, 'x')
  y = model.NewIntVar(0, num_vals - 1, 'y')
  z = model.NewIntVar(0, num_vals - 1, 'z')
  # Adds an all-different constraint.
  model.Add(x != y)

  # Creates a solver and solves the model.
  solver = cp_model.CpSolver()

  # Sets a time limit of 10 seconds.
  solver.parameters.max_time_in_seconds = 10.0

  status = solver.Solve(model)

  if status == cp_model.FEASIBLE:
    print('x = %i' % solver.Value(x))
    print('y = %i' % solver.Value(y))
    print('z = %i' % solver.Value(z))


# You need to subclass the cp_model.CpSolverSolutionCallback class.
class VarArrayAndObjectiveSolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, variables):
    cp_model.CpSolverSolutionCallback.__init__(self)
    self.__variables = variables
    self.__solution_count = 0

  def OnSolutionCallback(self):
    print('Solution %i' % self.__solution_count)
    print('  objective value = %i' % self.ObjectiveValue())
    for v in self.__variables:
      print('  %s = %i' % (v, self.Value(v)), end=' ')
    print()
    self.__solution_count += 1

  def SolutionCount(self):
    return self.__solution_count


def MinimalCpSatPrintIntermediateSolutions():
  """Showcases printing intermediate solutions found during search."""
  # Creates the model.
  model = cp_model.CpModel()
  # Creates the variables.
  num_vals = 3
  x = model.NewIntVar(0, num_vals - 1, 'x')
  y = model.NewIntVar(0, num_vals - 1, 'y')
  z = model.NewIntVar(0, num_vals - 1, 'z')
  # Creates the constraints.
  model.Add(x != y)
  model.Maximize(x + 2 * y + 3 * z)

  # Creates a solver and solves.
  solver = cp_model.CpSolver()
  solution_printer = VarArrayAndObjectiveSolutionPrinter([x, y, z])
  status = solver.SolveWithSolutionCallback(model, solution_printer)

  print('Status = %s' % solver.StatusName(status))
  print('Number of solutions found: %i' % solution_printer.SolutionCount())


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, variables):
    cp_model.CpSolverSolutionCallback.__init__(self)
    self.__variables = variables
    self.__solution_count = 0

  def OnSolutionCallback(self):
    self.__solution_count += 1
    for v in self.__variables:
      print('%s=%i' % (v, self.Value(v)), end=' ')
    print()

  def SolutionCount(self):
    return self.__solution_count


def MinimalCpSatAllSolutions():
  """Showcases calling the solver to search for all solutions."""
  # Creates the model.
  model = cp_model.CpModel()
  # Creates the variables.
  num_vals = 3
  x = model.NewIntVar(0, num_vals - 1, 'x')
  y = model.NewIntVar(0, num_vals - 1, 'y')
  z = model.NewIntVar(0, num_vals - 1, 'z')
  # Create the constraints.
  model.Add(x != y)

  # Create a solver and solve.
  solver = cp_model.CpSolver()
  solution_printer = VarArraySolutionPrinter([x, y, z])
  status = solver.SearchForAllSolutions(model, solution_printer)
  print('Status = %s' % solver.StatusName(status))
  print('Number of solutions found: %i' % solution_printer.SolutionCount())


def SolvingLinearProblem():
  """CP-SAT linear solver problem."""
  # Create a model.
  model = cp_model.CpModel()

  # x and y are integer non-negative variables.
  x = model.NewIntVar(0, 17, 'x')
  y = model.NewIntVar(0, 17, 'y')
  model.Add(2 * x + 14 * y <= 35)
  model.Add(2 * x <= 7)
  obj_var = model.NewIntVar(0, 1000, 'obj_var')
  model.Add(obj_var == x + 10 * y)
  model.Maximize(obj_var)

  # Create a solver and solve.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)
  if status == cp_model.OPTIMAL:
    print('Objective value: %i' % solver.ObjectiveValue())
    print()
    print('x= %i' % solver.Value(x))
    print('y= %i' % solver.Value(y))


def MinimalJobShop():
  """Minimal jobshop problem."""
  # Create the model.
  model = cp_model.CpModel()

  machines_count = 3
  jobs_count = 3
  all_machines = range(0, machines_count)
  all_jobs = range(0, jobs_count)
  # Define data.
  machines = [[0, 1, 2], [0, 2, 1], [1, 2]]

  processing_times = [[3, 2, 2], [2, 1, 4], [4, 3]]
  # Computes horizon.
  horizon = 0
  for job in all_jobs:
    horizon += sum(processing_times[job])

  task_type = collections.namedtuple('task_type', 'start end interval')
  assigned_task_type = collections.namedtuple('assigned_task_type',
                                              'start job index')

  # Creates jobs.
  all_tasks = {}
  for job in all_jobs:
    for index in range(0, len(machines[job])):
      start_var = model.NewIntVar(0, horizon, 'start_%i_%i' % (job, index))
      duration = processing_times[job][index]
      end_var = model.NewIntVar(0, horizon, 'end_%i_%i' % (job, index))
      interval_var = model.NewIntervalVar(start_var, duration, end_var,
                                          'interval_%i_%i' % (job, index))
      all_tasks[(job, index)] = task_type(
          start=start_var, end=end_var, interval=interval_var)

  # Creates sequence variables and add disjunctive constraints.
  for machine in all_machines:
    intervals = []
    for job in all_jobs:
      for index in range(0, len(machines[job])):
        if machines[job][index] == machine:
          intervals.append(all_tasks[(job, index)].interval)
    model.AddNoOverlap(intervals)

  # Add precedence contraints.
  for job in all_jobs:
    for index in range(0, len(machines[job]) - 1):
      model.Add(all_tasks[(job, index + 1)].start >= all_tasks[(job,
                                                                index)].end)

  # Makespan objective.
  obj_var = model.NewIntVar(0, horizon, 'makespan')
  model.AddMaxEquality(
      obj_var,
      [all_tasks[(job, len(machines[job]) - 1)].end for job in all_jobs])
  model.Minimize(obj_var)

  # Solve model.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)

  if status == cp_model.OPTIMAL:
    # Print out makespan.
    print('Optimal Schedule Length: %i' % solver.ObjectiveValue())
    print()

    # Create one list of assigned tasks per machine.
    assigned_jobs = [[] for _ in range(machines_count)]
    for job in all_jobs:
      for index in range(len(machines[job])):
        machine = machines[job][index]
        assigned_jobs[machine].append(
            assigned_task_type(
                start=solver.Value(all_tasks[(job, index)].start),
                job=job,
                index=index))

    disp_col_width = 10
    sol_line = ''
    sol_line_tasks = ''

    print('Optimal Schedule', '\n')

    for machine in all_machines:
      # Sort by starting time.
      assigned_jobs[machine].sort()
      sol_line += 'Machine ' + str(machine) + ': '
      sol_line_tasks += 'Machine ' + str(machine) + ': '

      for assigned_task in assigned_jobs[machine]:
        name = 'job_%i_%i' % (assigned_task.job, assigned_task.index)
        # Add spaces to output to align columns.
        sol_line_tasks += name + ' ' * (disp_col_width - len(name))
        start = assigned_task.start
        duration = processing_times[assigned_task.job][assigned_task.index]

        sol_tmp = '[%i,%i]' % (start, start + duration)
        # Add spaces to output to align columns.
        sol_line += sol_tmp + ' ' * (disp_col_width - len(sol_tmp))

      sol_line += '\n'
      sol_line_tasks += '\n'

    print(sol_line_tasks)
    print('Time Intervals for task_types\n')
    print(sol_line)


def main(_):
  print('--- CodeSample ---')
  CodeSample()
  print('--- LiteralSample ---')
  LiteralSample()
  print('--- BoolOrSample ---')
  BoolOrSample()
  print('--- ReifiedSample ---')
  ReifiedSample()
  print('--- RabbitsAndPheasants ---')
  RabbitsAndPheasants()
  print('--- BinpackingProblem ---')
  BinpackingProblem()
  print('--- IntervalSample ---')
  IntervalSample()
  print('--- OptionalIntervalSample ---')
  OptionalIntervalSample()
  print('--- MinimalCpSat ---')
  MinimalCpSat()
  print('--- MinimalCpSatWithTimeLimit ---')
  MinimalCpSatWithTimeLimit()
  print('--- MinimalCpSatPrintIntermediateSolutions ---')
  MinimalCpSatPrintIntermediateSolutions()
  print('--- MinimalCpSatAllSolutions ---')
  MinimalCpSatAllSolutions()
  print('--- SolvingLinearProblem ---')
  SolvingLinearProblem()
  print('--- MinimalJobShop ---')
  MinimalJobShop()


if __name__ == '__main__':
  main(None)
