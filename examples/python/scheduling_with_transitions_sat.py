# -*- coding: utf-8 -*-
"""Scheduling problem with transition time between tasks and transitions costs.

@author: CSLiu2
"""

from __future__ import print_function
from __future__ import absolute_import
from __future__ import division

import collections
from ortools.sat.python import cp_model


def main():
  """Solves the scheduling with transitions problem."""

  #----------------------------------------------------------------------------
  # Intermediate solution printer
  class SolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self):
      cp_model.CpSolverSolutionCallback.__init__(self)
      self.__solution_count = 0

    def OnSolutionCallback(self):
      print('Solution %i, time = %f s, objective = %i, makespan = %i' %
            (self.__solution_count, self.WallTime(), self.ObjectiveValue(),
             self.Value(makespan)))
      self.__solution_count += 1

  #----------------------------------------------------------------------------
  jobs = [[[(100, 0, 'R6'), (2, 1, 'R6')]], [[(2, 0, 'R3'), (100, 1, 'R3')]],
          [[(100, 0, 'R1'), (16, 1, 'R1')]], [[(1, 0, 'R1'), (38, 1, 'R1')]],
          [[(14, 0, 'R1'), (10, 1, 'R1')]], [[(16, 0, 'R3'), (17, 1, 'R3')]],
          [[(14, 0, 'R3'), (14, 1, 'R3')]], [[(14, 0, 'R3'), (15, 1, 'R3')]],
          [[(14, 0, 'R3'), (13, 1, 'R3')]], [[(100, 0, 'R1'), (38, 1, 'R1')]]]

  #----------------------------------------------------------------------------
  # Helper data
  num_jobs = len(jobs)
  all_jobs = range(num_jobs)
  num_machines = 2
  all_machines = range(num_machines)

  #----------------------------------------------------------------------------
  # Model
  model = cp_model.CpModel()

  #----------------------------------------------------------------------------
  # Sum each lot longest process time for max makespan
  horizon = 0
  for job in jobs:
    for task in job:
      max_task_duration = 0
      for alternative in task:
        max_task_duration = max(max_task_duration, alternative[0])
      horizon += max_task_duration

  print('Horizon = %i' % horizon)

  #----------------------------------------------------------------------------
  # Scan the jobs and create the relevant variables and intervals.
  intervals_per_machines = collections.defaultdict(list)
  presences_per_machines = collections.defaultdict(list)
  starts_per_machines = collections.defaultdict(list)
  ends_per_machines = collections.defaultdict(list)
  resources_per_machines = collections.defaultdict(list)
  job_starts = {}  # indexed by (job_id, task_id).
  job_presences = {}  # indexed by (job_id, task_id, alt_id).
  job_ends = []

  for job_id in all_jobs:
    job = jobs[job_id]
    num_tasks = len(job)
    previous_end = None
    for task_id in range(num_tasks):
      task = job[task_id]

      min_duration = task[0][0]
      max_duration = task[0][0]

      num_alternatives = len(task)
      all_alternatives = range(num_alternatives)

      for alt_id in range(1, num_alternatives):
        alt_duration = task[alt_id][0]
        min_duration = min(min_duration, alt_duration)
        max_duration = max(max_duration, alt_duration)

      # Create main interval for the task
      suffix_name = '_j%i_t%i' % (job_id, task_id)
      start = model.NewIntVar(0, horizon, 'start' + suffix_name)
      duration = model.NewIntVar(min_duration, max_duration,
                                 'duration' + suffix_name)
      end = model.NewIntVar(0, horizon, 'end' + suffix_name)
      interval = model.NewIntervalVar(start, duration, end,
                                      'interval' + suffix_name)

      # Store the start for the solution
      job_starts[(job_id, task_id)] = start

      # Add precedence with previous task in the same job
      if previous_end:
        model.Add(start >= previous_end)
      previous_end = end

      # Create alternative intervals
      if num_alternatives > 1:
        l_presences = []
        for alt_id in all_alternatives:
          ### add to link interval with eqp constraint
          ### process time = -1 cannot be processed at this machine
          if jobs[job_id][task_id][alt_id][0] == -1:
            continue
          alt_suffix = '_j%i_t%i_a%i' % (job_id, task_id, alt_id)
          l_presence = model.NewBoolVar('presence' + alt_suffix)
          l_start = model.NewIntVar(0, horizon, 'start' + alt_suffix)
          l_duration = task[alt_id][0]
          l_end = model.NewIntVar(0, horizon, 'end' + alt_suffix)
          l_interval = model.NewOptionalIntervalVar(
              l_start, l_duration, l_end, l_presence, 'interval' + alt_suffix)
          l_presences.append(l_presence)
          l_machine = task[alt_id][1]
          l_type = task[alt_id][2]

          # Link the master variables with the local ones
          model.Add(start == l_start).OnlyEnforceIf(l_presence)
          model.Add(duration == l_duration).OnlyEnforceIf(l_presence)
          model.Add(end == l_end).OnlyEnforceIf(l_presence)

          # Add the local variables to the right machine
          intervals_per_machines[l_machine].append(l_interval)
          starts_per_machines[l_machine].append(l_start)
          ends_per_machines[l_machine].append(l_end)
          presences_per_machines[l_machine].append(l_presence)
          resources_per_machines[l_machine].append(l_type)

          # Store the presences for the solution.
          job_presences[(job_id, task_id, alt_id)] = l_presence

        # Only one machine can process each lot
        model.Add(sum(l_presences) == 1)
      else:
        intervals_per_machines[task[0][1]].append(interval)
        job_presences[(job_id, task_id, 0)] = model.NewIntVar(1, 1, '')

    job_ends.append(previous_end)

  #----------------------------------------------------------------------------
  # Create machines constraints nonoverlap process
  for machine_id in all_machines:
    intervals = intervals_per_machines[machine_id]
    if len(intervals) > 1:
      model.AddNoOverlap(intervals)

  #----------------------------------------------------------------------------
  # Transition times and transition costs using a circuit constraints.
  switch_literals = []
  for machine_id in all_machines:
    machine_starts = starts_per_machines[machine_id]
    machine_ends = ends_per_machines[machine_id]
    machine_presences = presences_per_machines[machine_id]
    machine_resources = resources_per_machines[machine_id]
    intervals = intervals_per_machines[machine_id]
    arcs = []
    num_machine_tasks = len(machine_starts)
    all_machine_tasks = range(num_machine_tasks)

    for i in all_machine_tasks:
      arcs.append([0, i + 1, model.NewBoolVar('')])
      arcs.append([i + 1, 0, model.NewBoolVar('')])
      arcs.append([i + 1, i + 1, machine_presences[i].Not()])  # Self arc.
      for j in all_machine_tasks:
        lit = model.NewBoolVar('%i follows %i' % (j, i))
        if i == j:
          model.Add(lit == 0)
        else:
          arcs.append([i + 1, j + 1, lit])
          model.AddImplication(lit, machine_presences[i])
          model.AddImplication(lit, machine_presences[j])
          # Compute the transition time if task j is the successor of task i.
          if machine_resources[i] != machine_resources[j]:
            transition_time = 3
            switch_literals.append(lit)
          else:
            transition_time = 0
          # We add the reified transition to link the literals with the times
          # of the tasks.
          model.Add(machine_starts[j] >= machine_ends[i] +
                    transition_time).OnlyEnforceIf(lit)

  model.AddCircuit(arcs)

  #----------------------------------------------------------------------------
  # Objective
  makespan = model.NewIntVar(0, horizon, 'makespan')
  model.AddMaxEquality(makespan, job_ends)
  makespan_weight = 1
  transition_weight = 5
  model.Minimize(makespan * makespan_weight +
                 sum(switch_literals) * transition_weight)

  #----------------------------------------------------------------------------
  # Solve
  solver = cp_model.CpSolver()
  solver.parameters.max_time_in_seconds = 60 * 60 * 2
  solution_printer = SolutionPrinter()
  status = solver.SolveWithSolutionCallback(model, solution_printer)

  #----------------------------------------------------------------------------
  # Print solution
  if status == cp_model.FEASIBLE or status == cp_model.OPTIMAL:
    for job_id in all_jobs:
      for task_id in range(len(jobs[job_id])):
        start_value = solver.Value(job_starts[(job_id, task_id)])
        machine = 0
        duration = 0
        select = 0

        for alt_id in range(len(jobs[job_id][task_id])):
          if solver.BooleanValue(job_presences[(job_id, task_id, alt_id)]):
            duration = jobs[job_id][task_id][alt_id][0]
            machine = jobs[job_id][task_id][alt_id][1]
            select = alt_id

        print('  Job %i starts at %i (alt %i, machine %i, duration %i)' %
              (job_id, start_value, select, machine, duration))

    print('Solve status: %s' % solver.StatusName(status))
    print('Objective value: %i' % solver.ObjectiveValue())
    print('Makespan: %i' % solver.Value(makespan))


if __name__ == '__main__':
  main()
