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

"""Solves the facility location and processes feasible solutions in a callback.

In the (uncapacitated) facility location problem, we are given a list of
customers, a list of facilities we can open, and a limit on how many of these
facilities we get to open. Each customer must be served from a facility, and
there is a cost to serving this customer from each facility. The goal is to pick
the set of facilities to open that minimizes the cost of serving all customers.

In this example, customers and facilities each have a location on the 2d plane,
and we use Euclidean distance as our cost function.

Two modes are supported. If run with --plot, the solutions are drawn to a GUI
with Matplotlib. Otherwise, the solutions are printed to standard output.

Adapted from: https://youtu.be/H9EhVIxyLt8
"""

from collections.abc import Sequence
import dataclasses
import math
import os
import queue
import random
import threading
from typing import Dict, Tuple

from absl import app
from absl import flags
import matplotlib
import matplotlib.pyplot as plt  # pylint:disable=g-import-not-at-top

from ortools.math_opt.python import mathopt

_NUM_FACILITIES = flags.DEFINE_integer(
    "num_facilities", 10, "How many facility sites to consider."
)

_FACILITY_LIMIT = flags.DEFINE_integer(
    "facility_limit", 5, "How many facilities to open."
)

_NUM_CUSTOMERS = flags.DEFINE_integer(
    "num_customers", 50, "How many customers to serve."
)

_PLOT = flags.DEFINE_bool("plot", False, "Generate an image with matplotlib.")

_SOLVER = flags.DEFINE_enum_class(
    "solver",
    mathopt.SolverType.CP_SAT,
    mathopt.SolverType,
    "The solver to use.",
)


@dataclasses.dataclass(frozen=True)
class FacilityLocationSolution:
  """A solution to a FacilityLocationInstance.

  Attributes:
    facility_open: For each facility, a bool indicating if it is open.
    customer_assignment: For each customer, the index of the facility serving
      that customer.
    terminal: Is the last solution returned by _solve_facility_location.
  """

  facility_open: Tuple[bool, ...]
  customer_assignment: Tuple[int, ...]
  terminal: bool


@dataclasses.dataclass(frozen=True)
class FacilityLocationInstance:
  """Data for a facility location instance.

  Falicities and customers are points in the 2D plane and the cost function is
  Euclidean distance.

  Attributes:
    facilities: The locations of potential facility sites as (x, y) coordinates.
    customers: The locations of customers to serve as (x, y) coordinates.
    facility_limit: A limit on the number of facilities that can be opened.
  """

  facilities: Tuple[Tuple[float, float], ...]
  customers: Tuple[Tuple[float, float], ...]
  facility_limit: int

  def distance(self, facility: int, customer: int) -> float:
    """Returns the distance between a facility and a customer."""
    fx, fy = self.facilities[facility]
    cx, cy = self.customers[customer]
    dx = fx - cx
    dy = fy - cy
    return math.sqrt(dx * dx + dy * dy)

  def evaluate(self, solution: FacilityLocationSolution) -> float:
    """Returns the objective value of a solution, or throws if infeasible."""
    open_fac = sum(solution.facility_open)
    if open_fac > self.facility_limit:
      raise ValueError(
          f"Too many open facilities: {open_fac}, limit was:"
          f" {self.facility_limit}"
      )
    obj = 0
    for cust, f in enumerate(solution.customer_assignment):
      if not solution.facility_open[f]:
        raise ValueError(
            f"Solution used facility: {f} for customer: {cust}, but it was not"
            " open"
        )
      obj += self.distance(f, cust)
    return obj


def _rand_instance(
    num_facilities: int, num_customers: int, facility_limit: int
) -> FacilityLocationInstance:
  rand_point = lambda: (random.random(), random.random())
  return FacilityLocationInstance(
      facilities=tuple(rand_point() for _ in range(num_facilities)),
      customers=tuple(rand_point() for _ in range(num_customers)),
      facility_limit=facility_limit,
  )


def _draw(
    instance: FacilityLocationInstance,
    solution: FacilityLocationSolution,
    header: str,
) -> None:
  """Draws the solution to this instance with matplotlib."""
  ax = plt.gca()
  ax.clear()
  ax.set_aspect("equal")
  obj = instance.evaluate(solution)
  ax.set_title(f"{header}, objective value: {obj:.2f}")
  for i, location in enumerate(instance.facilities):
    color = "g" if solution.facility_open[i] else "r"
    ax.add_patch(plt.Circle(location, 0.03, color=color))

  for j, location in enumerate(instance.customers):
    ax.add_patch(plt.Circle(location, 0.01, color="b"))
    cx, cy = location
    facility = solution.customer_assignment[j]
    fx, fy = instance.facilities[facility]
    ax.add_line(plt.Line2D([cx, fx], [cy, fy]))
  plt.show(block=solution.terminal)
  if not solution.terminal:
    plt.pause(0.5)


def _solve_facility_location(
    instance: FacilityLocationInstance,
    solution_queue: queue.Queue[FacilityLocationSolution],
) -> None:
  """Solves instance pushing observed solutions into solution_queue."""
  m = len(instance.facilities)
  n = len(instance.customers)
  # The optimization model for the facility location problem is:
  #
  # Data:
  #   c_ij: The cost of using facility i to serve customer j.
  #   L: a limit on how many facilities can be opened.
  #
  # Variables:
  #   x_i: A binary variable, if facility i is opened.
  #   y_ij: Is customer j served from facility i, implied integer.
  #
  # Model:
  #   min    sum_i sum_j c_ij y_ij
  #   s.t.   sum_i x_i <= L                        (facility limit)
  #          sum_i y_ij >= 1 for all j             (serve each customer)
  #          y_ij <= x_i     for all i for all j   (only use open facilities)
  #          x_i in {0, 1}   for all i
  #          y_ij >= 0       for all i, for all j
  model = mathopt.Model()
  # Variable x_i: do we open facility i
  x = tuple(model.add_binary_variable(name=f"x_{i}") for i in range(m))

  # Variable y_ij: is customer j served from faciltiy i
  y = tuple(
      tuple(model.add_variable(lb=0, name=f"y_{i}_{j}") for j in range(n))
      for i in range(m)
  )

  # Objective: minimize distance to serve customers
  total_distance = mathopt.LinearExpression()
  for i in range(m):
    for j in range(n):
      total_distance += y[i][j] * instance.distance(i, j)
  model.minimize(total_distance)

  # Constraint (1): sum_i x_i <= L
  model.add_linear_constraint(mathopt.fast_sum(x) <= instance.facility_limit)

  # Constraint (2): sum_i y_ij >= 1   for all j
  for j in range(n):
    model.add_linear_constraint(
        mathopt.fast_sum(y[i][j] for i in range(m)) >= 1
    )

  # Constraint (3): y_ij <= x_i      for all i, for all j
  for i in range(m):
    for j in range(n):
      model.add_linear_constraint(y[i][j] <= x[i])

  def extract_solution(
      var_values: Dict[mathopt.Variable, float], terminal: bool
  ) -> FacilityLocationSolution:
    is_open = tuple(var_values[x[i]] > 0.5 for i in range(m))
    customer_assignment = []
    for j in range(n):
      for i in range(m):
        if var_values[y[i][j]] > 0.5:
          customer_assignment.append(i)
          break
    assert len(customer_assignment) == n
    return FacilityLocationSolution(
        facility_open=is_open,
        customer_assignment=tuple(customer_assignment),
        terminal=terminal,
    )

  def draw_cb(cb_data: mathopt.CallbackData) -> mathopt.CallbackResult:
    if cb_data.event != mathopt.Event.MIP_SOLUTION:
      raise ValueError(f"event should be MIP_SOLUTION was: {cb_data.event}")
    assert cb_data.solution is not None
    assert solution_queue is not None
    solution = extract_solution(cb_data.solution, terminal=False)
    solution_queue.put(solution)
    return mathopt.CallbackResult()

  cb_reg = mathopt.CallbackRegistration(events={mathopt.Event.MIP_SOLUTION})
  result = mathopt.solve(
      model,
      _SOLVER.value,
      callback_reg=cb_reg,
      cb=draw_cb,
  )
  if result.termination.reason != mathopt.TerminationReason.OPTIMAL:
    raise ValueError(f"expected optimal solution, found {result.termination}")
  solution_queue.put(extract_solution(result.variable_values(), terminal=True))


def _plot_solution(
    instance: FacilityLocationInstance,
    solution: FacilityLocationSolution,
    sol_num: int,
) -> None:
  header = "Solved" if solution.terminal else f"solution: {sol_num}"
  _draw(instance, solution, header)


def _log_solution(
    instance: FacilityLocationInstance,
    solution: FacilityLocationSolution,
    sol_num: int,
) -> None:
  obj = instance.evaluate(solution)
  if solution.terminal:
    print(f"best solution: {obj}")
  else:
    print(f"solution {sol_num}: {obj}")


def main(argv: Sequence[str]) -> None:
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")
  instance = _rand_instance(
      _NUM_FACILITIES.value, _NUM_CUSTOMERS.value, _FACILITY_LIMIT.value
  )
  consumer = _plot_solution if _PLOT.value else _log_solution
  solution_queue = queue.Queue()
  threading.Thread(
      target=lambda: _solve_facility_location(instance, solution_queue)
  ).start()
  sol_num = 0
  while True:
    solution = solution_queue.get()
    sol_num += 1
    consumer(instance, solution, sol_num)
    if solution.terminal:
      break


if __name__ == "__main__":
  app.run(main)
