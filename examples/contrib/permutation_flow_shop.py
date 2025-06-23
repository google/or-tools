#!/usr/bin/env python3
# Copyright 2010-2025 Google LLC
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

"""This model implements the permutation flow shop problem (PFSP).

In the PFSP, a set of jobs has to be processed on a set of machines. Each job
must be processed on each machine in sequence and all jobs have to be processed
in the same order on every machine. The objective is to minimize the makespan.
"""

from typing import Sequence
from dataclasses import dataclass
from itertools import product

import numpy as np

from absl import app
from absl import flags
from google.protobuf import text_format
from ortools.sat.python import cp_model

_PARAMS = flags.DEFINE_string(
    "params",
    "num_search_workers:16",
    "Sat solver parameters.",
)

_TIME_LIMIT = flags.DEFINE_float(
    "time_limit",
    60.0,
    "Time limit in seconds. Default is 60s.",
)

_LOG = flags.DEFINE_boolean(
    "log",
    False,
    "Whether to log the solver output.",
)


@dataclass
class TaskType:
  """
  Small wrapper to hold the start, end, and interval variables of a task.
  """

  start: cp_model.IntVar
  end: cp_model.IntVar
  interval: cp_model.IntervalVar


def permutation_flow_shop(
    processing_times: np.ndarray, time_limit: float, log: bool, params: str
):
  """
  Solves the given permutation flow shop problem instance with OR-Tools.

  Parameters
  ----------
  processing_times
      An n-by-m matrix of processing times of the jobs on the machines.
  time_limit
      The time limit in seconds. If not set, the solver runs until an
      optimal solution is found.
  log
      Whether to log the solver output. Default is False.

  Raises
  ------
  ValueError
      If the number of lines is greater than 1, i.e., the instance is a
      distributed permutation flow shop problem.
  """
  m = cp_model.CpModel()
  num_jobs, num_machines = processing_times.shape
  horizon = processing_times.sum()

  # Create interval variables for all tasks (each job/machine pair).
  tasks = {}
  for job, machine in product(range(num_jobs), range(num_machines)):
    start = m.new_int_var(0, horizon, "")
    end = m.new_int_var(0, horizon, "")
    duration = processing_times[job][machine]
    interval = m.new_interval_var(start, duration, end, "")
    tasks[job, machine] = TaskType(start, end, interval)

  # No overlap for all job intervals on this machine.
  for machine in range(num_machines):
    intervals = [tasks[job, machine].interval for job in range(num_jobs)]
    m.add_no_overlap(intervals)

  # Add precedence constraints between tasks of the same job.
  for job, machine in product(range(num_jobs), range(num_machines - 1)):
    pred = tasks[job, machine]
    succ = tasks[job, machine + 1]
    m.add(pred.end <= succ.start)

  # Create arcs for circuit constraints.
  arcs = []
  for idx1 in range(num_jobs):
    arcs.append((0, idx1 + 1, m.new_bool_var("start")))
    arcs.append((idx1 + 1, 0, m.new_bool_var("end")))

  lits = {}
  for idx1, idx2 in product(range(num_jobs), repeat=2):
    if idx1 != idx2:
      lit = m.new_bool_var(f"{idx1} -> {idx2}")
      lits[idx1, idx2] = lit
      arcs.append((idx1 + 1, idx2 + 1, lit))

  m.add_circuit(arcs)

  # Enforce that the permutation of jobs is the same on all machines.
  for machine in range(num_machines):
    starts = [tasks[job, machine].start for job in range(num_jobs)]
    ends = [tasks[job, machine].end for job in range(num_jobs)]

    for idx1, idx2 in product(range(num_jobs), repeat=2):
      if idx1 == idx2:
        continue

      # Since all machines share the same arc literals, if the literal
      # i -> j is True, this enforces that job i is always scheduled
      # before job j on all machines.
      lit = lits[idx1, idx2]
      m.add(ends[idx1] <= starts[idx2]).only_enforce_if(lit)

  # Set minimizing makespan as objective.
  obj_var = m.new_int_var(0, horizon, "makespan")
  completion_times = [
      tasks[(job, num_machines - 1)].end for job in range(num_jobs)
  ]
  m.add_max_equality(obj_var, completion_times)
  m.minimize(obj_var)

  solver = cp_model.CpSolver()
  if params:
    text_format.Parse(params, solver.parameters)
  solver.parameters.log_search_progress = log
  solver.parameters.max_time_in_seconds = time_limit

  status_code = solver.Solve(m)
  status = solver.StatusName(status_code)

  print(f"Status: {status}")
  print(f"Makespan: {solver.ObjectiveValue()}")

  if status in ["OPTIMAL", "FEASIBLE"]:
    start = [solver.Value(tasks[job, 0].start) for job in range(num_jobs)]
    solution = np.argsort(start) + 1
    print(f"Solution: {solution}")


def main(argv: Sequence[str]) -> None:
  """Creates the data and calls the solving procedure."""
  # VRF_10_5_2 instance from http://soa.iti.es/problem-instances.
  # Optimal makespan is 698.
  processing_times = [
      [79, 67, 10, 48, 52],
      [40, 40, 57, 21, 54],
      [48, 93, 49, 11, 79],
      [16, 23, 19, 2, 38],
      [38, 90, 57, 73, 3],
      [76, 13, 99, 98, 55],
      [73, 85, 40, 20, 85],
      [34, 6, 27, 53, 21],
      [38, 6, 35, 28, 44],
      [32, 11, 11, 34, 27],
  ]

  permutation_flow_shop(
      np.array(processing_times), _TIME_LIMIT.value, _LOG.value, _PARAMS.value
  )


if __name__ == "__main__":
  app.run(main)
