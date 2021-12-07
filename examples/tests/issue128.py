#!/usr/bin/env python3
from ortools.constraint_solver import pywrapcp

def test_v0():
  print('test_v0')
  solver = pywrapcp.Solver('')

  # we have two tasks of durations 4 and 7
  task1 = solver.FixedDurationIntervalVar(0, 5, 4, False, "task1")
  task2 = solver.FixedDurationIntervalVar(0, 5, 7, False, "task2")
  tasks = [task1, task2]

  # to each task, a post task of duration 64 is attached
  postTask1 = solver.FixedDurationIntervalVar(4, 74 + 64, 64, False, "postTask1")
  postTask2 = solver.FixedDurationIntervalVar(4, 77 + 64, 64, False, "postTask2")
  postTasks = [postTask1, postTask2]

  solver.Add(postTask1.StartsAtEnd(task1))
  solver.Add(postTask2.StartsAtEnd(task2))

  # two resources are available for the post tasks. There are binary indicator
  # variables to determine which task uses which resource
  postTask1UsesRes1 = solver.IntVar(0, 1, "post task 1 using resource 1")
  postTask1UsesRes2 = solver.IntVar(0, 1, "post task 1 using resource 2")
  postTask2UsesRes1 = solver.IntVar(0, 1, "post task 2 using resource 1")
  postTask2UsesRes2 = solver.IntVar(0, 1, "post task 2 using resource 2")

  indicators = [postTask1UsesRes1, postTask1UsesRes2, postTask2UsesRes1, postTask2UsesRes2]

  # each post task needs exactly one resource
  solver.Add(postTask1UsesRes1 + postTask1UsesRes2 == 1)
  solver.Add(postTask2UsesRes1 + postTask2UsesRes2 == 1)

  # each resource cannot be used simultaneously by more than one post task
  solver.Add(solver.Cumulative(postTasks, [postTask1UsesRes1, postTask2UsesRes1], 1, "cumul1"))
  solver.Add(solver.Cumulative(postTasks, [postTask1UsesRes2, postTask2UsesRes2], 1, "cumul2"))

  # using constant demands instead, the correct solution is found
  #    solver.Add(solver.Cumulative(postTasks, [0, 1], 1, ""))
  #    solver.Add(solver.Cumulative(postTasks, [1, 0], 1, ""))


  # search setup and solving
  dbInterval = solver.Phase(tasks + postTasks, solver.INTERVAL_DEFAULT)
  dbInt = solver.Phase(indicators, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT)

  makespan = solver.Max([task1.EndExpr().Var(), task2.EndExpr().Var()])
  optimize = solver.Optimize(False, makespan, 1)

  solution = solver.Assignment()
  solution.Add([t for t in (tasks + postTasks)])
  solution.Add(indicators)
  collector = solver.LastSolutionCollector(solution)
  phase = solver.Compose([dbInt, dbInterval])
  solver.Solve(phase, [collector, optimize])

  if collector.SolutionCount() > 0:
    for i, task in enumerate(tasks):
      print("task {} runs from {} to {}".format(
          i,
          collector.StartValue(0, task),
          collector.EndValue(0, task)))
    for i, task in enumerate(postTasks):
      print("postTask {} starts at {}".format(i, collector.StartValue(0, task)))
    for indicator in indicators:
      print('{} -> {}'.format(indicator.Name(), collector.Value(0, indicator)))
  else:
    print('No solution')

def test_v1():
  print('test_v1')
  solver = pywrapcp.Solver('')

  # we have two tasks of durations 4 and 7
  task1 = solver.FixedDurationIntervalVar(0, 5, 4, False, "task1")
  task2 = solver.FixedDurationIntervalVar(0, 5, 7, False, "task2")
  tasks = [task1, task2]
  # Create copies for each resource
  task1_r1 = solver.FixedDurationIntervalVar(0, 5, 4, True, "task1_1")
  task2_r1 = solver.FixedDurationIntervalVar(0, 5, 7, True, "task2_1")
  tasks_r1 = [task1_r1, task2_r1]
  task1_r2 = solver.FixedDurationIntervalVar(0, 5, 4, True, "task1_2")
  task2_r2 = solver.FixedDurationIntervalVar(0, 5, 7, True, "task2_2")
  tasks_r2 = [task1_r2, task2_r2]


  # to each task, a post task of duration 64 is attached
  postTask1 = solver.FixedDurationStartSyncedOnEndIntervalVar(task1, 64, 0)
  postTask2 = solver.FixedDurationStartSyncedOnEndIntervalVar(task2, 64, 0)
  postTasks = [postTask1, postTask2]

  # Create copies for each resource
  postTask1_r1 = solver.FixedDurationIntervalVar(4, 9, 64, True, "pTask1_1")
  postTask2_r1 = solver.FixedDurationIntervalVar(4, 11, 64, True, "pTask2_1")
  postTask1_r2 = solver.FixedDurationIntervalVar(4, 9, 64, True, "pTask1_2")
  postTask2_r2 = solver.FixedDurationIntervalVar(4, 11, 64, True, "pTask2_2")

  copies = [ task1_r1, task2_r1, task1_r2, task2_r2,
             postTask1_r1, postTask1_r2, postTask2_r1, postTask2_r2 ]

  # each resource cannot be used simultaneously by more than one post task
  solver.Add(solver.DisjunctiveConstraint(
      [task1_r1, task2_r1, postTask1_r1, postTask2_r1], "disj1"))
  solver.Add(solver.DisjunctiveConstraint(
      [task1_r2, task2_r2, postTask1_r2, postTask2_r2],  "disj1"))

  # Only one resource available
  solver.Add(task1_r1.PerformedExpr() + task1_r2.PerformedExpr() == 1)
  solver.Add(task2_r1.PerformedExpr() + task2_r2.PerformedExpr() == 1)
  solver.Add(postTask1_r1.PerformedExpr() + postTask1_r2.PerformedExpr() == 1)
  solver.Add(postTask2_r1.PerformedExpr() + postTask2_r2.PerformedExpr() == 1)

  # Sync master task with copies
  solver.Add(solver.Cover([task1_r1, task1_r2], task1))
  solver.Add(solver.Cover([task2_r1, task2_r2], task2))
  solver.Add(solver.Cover([postTask1_r1, postTask1_r2], postTask1))
  solver.Add(solver.Cover([postTask2_r1, postTask2_r2], postTask2))

  # Indicators (no need to add both as they are constrained together)
  indicators = [
      task1_r1.PerformedExpr(), task2_r1.PerformedExpr(),
      postTask1_r1.PerformedExpr(), postTask2_r1.PerformedExpr()]

  # search setup and solving
  dbInterval = solver.Phase(tasks + postTasks, solver.INTERVAL_DEFAULT)
  dbInt = solver.Phase(
      indicators, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT)

  makespan = solver.Max([task1.EndExpr(), task2.EndExpr()])
  optimize = solver.Minimize(makespan, 1)

  solution = solver.Assignment()
  solution.Add(tasks)
  solution.Add(postTasks)
  solution.Add(copies)
  solution.AddObjective(makespan)
  collector = solver.LastSolutionCollector(solution)
  phase = solver.Compose([dbInt, dbInterval])
  solver.Solve(phase, [collector, optimize])

  if collector.SolutionCount() > 0:
    print('solution with makespan', collector.ObjectiveValue(0))
    for task in tasks:
      print("task {} runs from {} to {}".format(
          task.Name(),
          collector.StartValue(0, task),
          collector.EndValue(0, task)))
    for task in postTasks:
      print("postTask {} starts at {}".format(
          task.Name(), collector.StartValue(0, task)))
    for task in copies:
      print(task.Name(), collector.PerformedValue(0, task))
  else:
    print('No solution')

test_v0()
test_v1()
