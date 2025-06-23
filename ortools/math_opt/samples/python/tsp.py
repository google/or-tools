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

"""Solve the traveling salesperson problem (TSP) with MIP.

In the Euclidean Traveling Salesperson Problem (TSP), you are given a list of
n cities, each with an (x, y) coordinate, and you must find an order to visit
the cities in to minimize the (Euclidean) travel distance.

The MIP "cutset" formulation for the problem is as follows:
  * Data:
      n: An integer, the number of cities
      (x_i, y_i): a pair of floats for each i in 1..n, the location of each
          city
      d_ij for all (i, j) pairs of cities, the distance between city i and j.
  * Decision variables:
      x_ij: A binary variable, indicates if the edge connecting i and j is
          used. Note that x_ij == x_ji, because the problem is symmetric. We
          only create variables for i < j, and have x_ji as an alias for
          x_ij.
  * MIP model:
      minimize sum_{i=1}^n sum_{j=1, j < i}^n d_ij * x_ij
      s.t. sum_{j=1, j != i}^n x_ij = 2 for all i = 1..n
           sum_{i in S} sum_{j not in S} x_ij >= 2 for all S subset {1,...,n}
                                                   |S| >= 3, |S| <= n - 3
           x_ij in {0, 1}
The first set of constraints are called the degree constraints, and the
second set of constraints are called the cutset constraints. There are
exponentially many cutset, so we cannot add them all at the start of the
solve. Instead, we will use a solver callback to view each integer solution
and add any violated cutset constraints that exist.

Note that, while there are exponentially many cutset constraints, we can
quickly identify violated ones by exploiting that the solution is integer
and the degree constraints are all already in the model and satisfied. As a
result, the graph n nodes and edges when x_ij = 1 will be a degree two graph,
so it will be a collection of cycles. If it is a single large cycle, then the
solution is feasible, and if there multiple cycles, then taking the nodes of
any cycle as S produces a violated cutset constraint.

Note that this is a minimal TSP solution, more sophisticated MIP methods are
possible.
"""
import itertools
import math
import random
from typing import Dict, List, Optional, Tuple

from absl import app
from absl import flags
import svgwrite

from ortools.math_opt.python import mathopt

_NUM_CITIES = flags.DEFINE_integer(
    "num_cities", 100, "The size of the TSP instance."
)
_OUTPUT = flags.DEFINE_string(
    "output", "", "Where to write an output SVG, if nonempty"
)
_TEST_INSTANCE = flags.DEFINE_boolean(
    "test_instance",
    False,
    "Use a small test instance instead of a random instance.",
)
_SOLVE_LOGS = flags.DEFINE_boolean(
    "solve_logs", False, "Have the solver print logs to standard out."
)

Cities = List[Tuple[float, float]]


def _random_cities(num_cities: int) -> Cities:
  """Returns a list random entries distributed U[0,1]^2 i.i.d."""
  return [(random.random(), random.random()) for _ in range(num_cities)]


def _test_instance() -> Cities:
  return [
      (0.0, 0.0),
      (0.1, 0.0),
      (0.0, 0.1),
      (0.1, 0.1),
      (0.0, 0.9),
      (0.1, 0.9),
      (0.0, 1.0),
      (0.1, 1.0),
  ]


def _distance_matrix(cities: Cities) -> List[List[float]]:
  """Converts a list of (x,y) pairs into a a matrix of Eucledian distances."""
  n = len(cities)
  res = [[0.0] * n for _ in range(n)]
  for i in range(n):
    for j in range(i + 1, n):
      xi, yi = cities[i]
      xj, yj = cities[j]
      dx = xi - xj
      dy = yi - yj
      dist = math.sqrt(dx * dx + dy * dy)
      res[i][j] = dist
      res[j][i] = dist
  return res


def _edge_values(
    edge_vars: List[List[Optional[mathopt.Variable]]],
    var_values: Dict[mathopt.Variable, float],
) -> List[List[bool]]:
  """Converts edge decision variables into an adjacency matrix."""
  n = len(edge_vars)
  res = [[False] * n for _ in range(n)]
  for i in range(n):
    for j in range(n):
      if i != j:
        res[i][j] = var_values[edge_vars[i][j]] > 0.5
  return res


def _find_cycles(edges: List[List[bool]]) -> List[List[int]]:
  """Finds the cycle decomposition for a degree two graph as adjacenty matrix."""
  n = len(edges)
  cycles = []
  visited = [False] * n
  # Algorithm: maintain a "visited" bit for each city indicating if we have
  # formed a cycle containing this city. Consider the cities in order. When you
  # find an unvisited city, start a new cycle beginning at this city. Then,
  # build the cycle by finding an unvisited neighbor until no such neighbor
  # exists (every city will have two neighbors, but eventually both will be
  # visited). To find the "unvisited neighbor", we simply do a linear scan
  # over the cities, checking both the adjacency matrix and the visited bit.
  #
  # Note that for this algorithm, in each cycle, the city with lowest index
  # will be first, and the cycles will be sorted by their city of lowest index.
  # This is an implementation detail and should not be relied upon.
  for i in range(n):
    if visited[i]:
      continue
    cycle = []
    next_city = i
    while next_city is not None:
      cycle.append(next_city)
      visited[next_city] = True
      current = next_city
      next_city = None
      # Scan for an unvisited neighbor. We can start at i+1 since we know that
      # everything from i back is visited.
      for j in range(i + 1, n):
        if (not visited[j]) and edges[current][j]:
          next_city = j
          break
    cycles.append(cycle)
  return cycles


def solve_tsp(cities: Cities) -> List[int]:
  """Solves the traveling salesperson problem and returns the best route."""
  n = len(cities)
  dist = _distance_matrix(cities)
  model = mathopt.Model(name="tsp")
  edges = [[None] * n for _ in range(n)]
  for i in range(n):
    for j in range(i + 1, n):
      v = model.add_binary_variable(name=f"x_{i}_{j}")
      edges[i][j] = v
      edges[j][i] = v
  obj = 0
  for i in range(n):
    obj += sum(dist[i][j] * edges[i][j] for j in range(i + 1, n))
  model.minimize(obj)
  for i in range(n):
    model.add_linear_constraint(
        sum(edges[i][j] for j in range(n) if j != i) == 2.0
    )

  def cb(cb_data: mathopt.CallbackData) -> mathopt.CallbackResult:
    assert cb_data.solution is not None
    cycles = _find_cycles(_edge_values(edges, cb_data.solution))
    result = mathopt.CallbackResult()
    if len(cycles) > 1:
      for cycle in cycles:
        cycle_as_set = set(cycle)
        not_in_cycle = [i for i in range(n) if i not in cycle_as_set]
        result.add_lazy_constraint(
            sum(
                edges[i][j] for (i, j) in itertools.product(cycle, not_in_cycle)
            )
            >= 2.0
        )
    return result

  result = mathopt.solve(
      model,
      mathopt.SolverType.GUROBI,
      params=mathopt.SolveParameters(enable_output=_SOLVE_LOGS.value),
      callback_reg=mathopt.CallbackRegistration(
          events={mathopt.Event.MIP_SOLUTION}, add_lazy_constraints=True
      ),
      cb=cb,
  )
  assert (
      result.termination.reason == mathopt.TerminationReason.OPTIMAL
  ), result.termination
  assert result.solutions[0].primal_solution is not None
  print(f"Route length: {result.solutions[0].primal_solution.objective_value}")
  cycles = _find_cycles(
      _edge_values(edges, result.solutions[0].primal_solution.variable_values)
  )
  assert len(cycles) == 1, len(cycles)
  route = cycles[0]
  assert len(route) == n, (len(route), n)
  return route


def route_svg(filename: str, cities: Cities, route: List[int]):
  """Draws the route as an SVG and writes to disk (or prints if no filename)."""
  resolution = 1000
  r = 5
  drawing = svgwrite.Drawing(
      filename=filename,
      size=(resolution + 2 * r, resolution + 2 * r),
      profile="tiny",
  )
  polygon_points = []
  scale = lambda x: int(round(x * resolution)) + r
  for city in route:
    raw_x, raw_y = cities[city]
    c = (scale(raw_x), scale(raw_y))
    polygon_points.append(c)
    drawing.add(drawing.circle(center=c, r=r, fill="blue"))
  drawing.add(
      drawing.polygon(points=polygon_points, stroke="blue", fill="none")
  )
  if not filename:
    print(drawing.tostring())
  else:
    drawing.save()


def main(args):
  del args  # Unused.
  if _TEST_INSTANCE.value:
    cities = _test_instance()
  else:
    cities = _random_cities(_NUM_CITIES.value)
  route = solve_tsp(cities)
  route_svg(_OUTPUT.value, cities, route)


if __name__ == "__main__":
  app.run(main)
