from __future__ import print_function

import argparse
from collections import defaultdict
from ortools.sat.python import cp_model
from ortools.data import rcpsp_pb2
from ortools.data import pywraprcpsp
import time

parser = argparse.ArgumentParser()

parser.add_argument('--input', default = "",
                    help = 'Input file to parse and solve.')
parser.add_argument('--output_proto', default = "",
                    help = 'Output file to write the cp_model proto to.')


class SolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self):
    self.__solution_count = 0
    self.__start_time = time.time()

  def NewSolution(self):
    current_time = time.time()
    objective = self.ObjectiveValue()
    print('Solution %i, time = %f s, objective = %i' %
          (self.__solution_count, current_time - self.__start_time, objective))
    self.__solution_count += 1


def SolveRcpsp(problem, proto_file):
  # Determine problem type.
  problem_type = ('Resource investment' if problem.is_resource_investment
                                        else 'RCPSP')

  if problem.is_rcpsp_max:
    problem_type += '/Max delay'
  if problem.is_consumer_producer:
    print ('Solving %s with %i reservoir resources and %i tasks' % (
        problem_type, len(problem.resources), len(problem.tasks)))
  else:
    print ('Solving %s with %i resources and %i tasks' % (
        problem_type, len(problem.resources), len(problem.tasks)))

  # Create the model.
  model = cp_model.CpModel()

  num_tasks = len(problem.tasks)
  num_resources = len(problem.resources)

  all_tasks = range(num_tasks)
  all_active_tasks = range(1, num_tasks - 1)
  all_resources = range(num_resources)

  horizon = problem.deadline if problem.deadline != -1 else problem.horizon
  if horizon == -1:  # Naive computation.
    horizon = sum(max(r.duration for r in t.recipes) for t in problem.tasks)
    if problem.is_rcpsp_max:
      for t in problem.tasks:
        for sd in t.successor_delays:
          for rd in sd.recipe_delays:
            for d in rd.min_delays:
              horizon += abs(d)
  print('  - horizon = %i' % horizon)

  # Containers used to build resources.
  intervals_per_resource = defaultdict(list)
  demands_per_resource = defaultdict(list)
  presences_per_resource = defaultdict(list)
  starts_per_resource = defaultdict(list)

  # Starts and ends for master interval variables.
  task_starts = {}
  task_ends = {}

  # Containers for per-recipe per task variables.
  alternatives_per_task = defaultdict(list)
  presences_per_task = defaultdict(list)
  starts_per_task = defaultdict(list)
  ends_per_task = defaultdict(list)

  # Create tasks.
  for t in all_active_tasks:
    task = problem.tasks[t]

    if len(task.recipes) == 1:
      # Create interval.
      recipe = task.recipes[0]
      task_starts[t] = model.NewIntVar(0, horizon, 'start_of_task_%i' % t)
      task_ends[t] = model.NewIntVar(0, horizon, 'end_of_task_%i' % t)
      interval = model.NewIntervalVar(task_starts[t], recipe.duration,
                                      task_ends[t], 'interval_%i' % t)

      # Store for later.
      alternatives_per_task[t].append(interval)
      starts_per_task[t].append(task_starts[t])
      ends_per_task[t].append(task_ends[t])
      presences_per_task[t].append(1)

      # Register for resources.
      for i in range(len(recipe.demands)):
        demand = recipe.demands[i]
        res = recipe.resources[i]
        demands_per_resource[res].append(demand)
        if problem.resources[res].renewable:
          intervals_per_resource[res].append(interval)
        else:
          starts_per_resource[res].append(task_starts[t])
          presences_per_resource[res].append(1)
    else:
      all_recipes = range(len(task.recipes))

      # Compute duration range.
      min_size = min(recipe.duration for recipe in task.recipes)
      max_size = max(recipe.duration for recipe in task.recipes)

      # Create one optional interval per recipe.
      for r in all_recipes:
        recipe = task.recipes[r]
        is_present = model.NewBoolVar('is_present_%i_r%i' % (t, r))
        start = model.NewOptionalIntVar(0, horizon, is_present,
                                        'start_%i_r%i' % (t, r))
        end = model.NewOptionalIntVar(0, horizon, is_present,
                                      'end_%i_r%i' % (t, r))
        interval = model.NewOptionalIntervalVar(
            start, recipe.duration, end, is_present, 'interval_%i_r%i' % (t, r))

        # Store variables.
        alternatives_per_task[t].append(interval)
        starts_per_task[t].append(start)
        ends_per_task[t].append(end)
        presences_per_task[t].append(is_present)

        # Register intervals in resources.
        for i in range(len(recipe.demands)):
          demand = recipe.demands[i]
          res = recipe.resources[i]
          demands_per_resource[res].append(demand)
          if problem.resources[res].renewable:
            intervals_per_resource[res].append(interval)
          else:
            starts_per_resource[res].append(start)
            presences_per_resource[res].append(is_present)

      # Create the master interval for the task.
      task_starts[t] = model.NewIntVar(0, horizon, 'start_of_task_%i' % t)
      task_ends[t] = model.NewIntVar(0, horizon, 'end_of_task_%i' % t)
      duration = model.NewIntVar(min_size, max_size, 'duration_of_task_%i' % t)
      interval = model.NewIntervalVar(task_starts[t], duration,
                                      task_ends[t], 'interval_%i' % t)

      # Link with optional per-recipe copies.
      for r in all_recipes:
        p = presences_per_task[t][r]
        model.Add(task_starts[t] == starts_per_task[t][r]).OnlyEnforceIf(p)
        model.Add(task_ends[t] == ends_per_task[t][r]).OnlyEnforceIf(p)
        model.Add(duration == task.recipes[r].duration).OnlyEnforceIf(p)
      model.Add(sum(presences_per_task[t]) == 1)


  # Create makespan variable
  makespan = model.NewIntVar(0, horizon, 'makespan')

  # Add precedences.
  if problem.is_rcpsp_max:
    for t in all_active_tasks:
      task = problem.tasks[t]
      num_modes = len(task.recipes)
      num_successors = len(task.successors)

      for s in range(num_successors):
        n = task.successors[s]
        delay_matrix = task.successor_delays[s]
        num_other_modes = len(problem.tasks[n].recipes)
        for m1 in range(num_modes):
          s1 = starts_per_task[t][m1]
          if n == num_tasks - 1:
            delay = delay_matrix.recipe_delays[m1].min_delays[0]
            model.Add(s1 + delay <= makespan)
          else:
            for m2 in range(num_other_modes):
              delay = delay_matrix.recipe_delays[m1].min_delays[m2]
              s2 = starts_per_task[n][m2]
              model.Add(s1 + delay <= s2)
  else:  # Normal dependencies (task ends before the start of successors).
    for t in all_active_tasks:
      for n in problem.tasks[t].successors:
        if n == num_tasks - 1:
          model.Add(task_ends[t] <= makespan)
        else:
          model.Add(task_ends[t] <= task_starts[n])

  # Containers for resource investment problems.
  capacities = []
  max_cost = 0

  # Create resources.
  for r in all_resources:
    resource = problem.resources[r]
    c = resource.max_capacity
    if c == -1:
      c = sum(demands_per_resource[r])

    if problem.is_resource_investment:
      capacity = model.NewIntVar(0, c, 'capacity_of_%i' % r)
      model.AddCumulative(
          intervals_per_resource[r], demands_per_resource[r], capacity)
      capacities.append(capacity)
      max_cost += c * resource.unit_cost
    elif resource.renewable:
      if intervals_per_resource[r]:
        model.AddCumulative(
            intervals_per_resource[r], demands_per_resource[r], c)
    elif presences_per_resource[r]:  # Non empty non renewable resource.
      if problem.is_consumer_producer:
        model.AddReservoirConstraint(
            starts_per_resource[r], demands_per_resource[r],
            resource.min_capacity, resource.max_capacity)
      else:
        model.Add(sum(presences_per_resource[r][i] * demands_per_resource[r][i]
                  for i in range(len(presences_per_resource[r]))) <= c)

  # Objective.
  if problem.is_resource_investment:
    objective = model.NewIntVar(0, max_cost, 'capacity_costs')
    model.Add(objective == sum(problem.resources[i].unit_cost * capacities[i]
                               for i in range(len(capacities))))
  else:
    objective = makespan

  model.Minimize(objective)

  if proto_file:
    print('Writing proto to %s' % proto_file)
    text_file = open(proto_file, 'w')
    text_file.write(str(model))
    text_file.close()

  # Solve model.
  solver = cp_model.CpSolver()
  solution_printer = SolutionPrinter()
  status = solver.SolveWithSolutionObserver(model, solution_printer)
  print('Solve status: %s' % solver.StatusName(status))
  print('Optimal objective value: %i' % solver.ObjectiveValue())
  print('Statistics')
  print('  - conflicts : %i' % solver.NumConflicts())
  print('  - branches  : %i' % solver.NumBranches())
  print('  - wall time : %f s' % solver.WallTime())


def main(args):
  parser = pywraprcpsp.RcpspParser()
  parser.ParseFile(args.input)
  problem = parser.Problem()
  SolveRcpsp(problem, args.output_proto)


if __name__ == '__main__':
  main(parser.parse_args())
