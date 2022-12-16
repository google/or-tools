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

"""Find the smallest circle to enclose input points in two dimensions.

Input data:

 * n: the number of points to enclose
 * (a_i, b_i): the locations of the points

Decision variables:

* x, y: the location of the center of the enclosing circle
* z: the square of the radius of the circle
* h_i: the horizontal distance from point i to the circle center (can be < 0)
* v_i: the vertical distance from point i to the circle center (can be < 0)

Formulation:

min z
s.t. h_i = a_i - x
     v_i = b_i - y
     h_i^2 + v_i^2 <= z

This is a convex quadratically constrained problem (it can also be viewed as
a second order cone problem).

The smallest circle problem is a standard problem in computational geometry.
Other names from this problem include:
 * the minimum covering circle problem
 * the bounding circle problem
 * the least bounding circle problem
 * the smallest enclosing circle problem
The problem does not require mathematical optimization, it suffices to simply
find the smallest circle for every pair and triple of points, one of these will
enclose all points. Somewhat surprisingly, this problem has a linear time
solution. This problem has also been considered from an optimization
perspective, e.g., see "The minimum covering sphere problem" (1972). See
https://en.wikipedia.org/wiki/Smallest-circle_problem for more details.
"""

from collections.abc import Sequence
import math

from absl import app
from absl import flags
import numpy as np

from ortools.math_opt.python import mathopt

_SOLVER_TYPE = flags.DEFINE_enum_class(
    "solver_type",
    mathopt.SolverType.GUROBI,
    mathopt.SolverType,
    "The solver needs to support quadratic constraints, e.g., gurobi.",
)

_NUM_POINTS = flags.DEFINE_integer(
    "num_points", 100, "The number of points in the problem."
)


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")
    n = _NUM_POINTS.value
    rng = np.random.default_rng()
    points = rng.random(size=(n, 2))
    print(f"points:\n{points}")

    model = mathopt.Model()
    x = model.add_variable(name="x")
    y = model.add_variable(name="y")
    z = model.add_variable(name="z")
    h = [model.add_variable(name=f"h_{i}") for i in range(n)]
    v = [model.add_variable(name=f"v_{i}") for i in range(n)]
    for i in range(n):
        model.add_linear_constraint(h[i] == x - points[i, 0])
        model.add_linear_constraint(v[i] == y - points[i, 1])
        model.add_quadratic_constraint(h[i] * h[i] + v[i] * v[i] <= z)
    model.minimize(z)
    params = mathopt.SolveParameters(enable_output=True)
    result = mathopt.solve(model, _SOLVER_TYPE.value, params=params)
    if result.termination.reason != mathopt.TerminationReason.OPTIMAL:
        raise ValueError("Expected Optimal Solution")
    print(f"circle center x: {result.variable_values(x)}")
    print(f"circle center y: {result.variable_values(y)}")
    radius = math.sqrt(result.variable_values(z))
    print(f"circle radius: {radius}")


if __name__ == "__main__":
    app.run(main)
