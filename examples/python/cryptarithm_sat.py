#!/usr/bin/env python3
# Copyright 2010-2022 Google LLC
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

"""Use CP-SAT to solve a simple cryptarithmetic problem: SEND+MORE=MONEY.
"""

from absl import app
from ortools.sat.python import cp_model


def send_more_money():
    """Solve the cryptarithmic puzzle SEND+MORE=MONEY."""
    model = cp_model.CpModel()

    # Create variables.
    # Since s is a leading digit, it can't be 0.
    s = model.NewIntVar(1, 9, "s")
    e = model.NewIntVar(0, 9, "e")
    n = model.NewIntVar(0, 9, "n")
    d = model.NewIntVar(0, 9, "d")
    # Since m is a leading digit, it can't be 0.
    m = model.NewIntVar(1, 9, "m")
    o = model.NewIntVar(0, 9, "o")
    r = model.NewIntVar(0, 9, "r")
    y = model.NewIntVar(0, 9, "y")

    # Create carry variables. c0 is true if the first column of addends carries
    # a 1, c2 is true if the second column carries a 1, and so on.
    c0 = model.NewBoolVar("c0")
    c1 = model.NewBoolVar("c1")
    c2 = model.NewBoolVar("c2")
    c3 = model.NewBoolVar("c3")

    # Force all letters to take on different values.
    model.AddAllDifferent(s, e, n, d, m, o, r, y)

    # Column 0:
    model.Add(c0 == m)

    # Column 1:
    model.Add(c1 + s + m == o + 10 * c0)

    # Column 2:
    model.Add(c2 + e + o == n + 10 * c1)

    # Column 3:
    model.Add(c3 + n + r == e + 10 * c2)

    # Column 4:
    model.Add(d + e == y + 10 * c3)

    # Solve model.
    solver = cp_model.CpSolver()
    if solver.Solve(model) == cp_model.OPTIMAL:
        print("Optimal solution found!")
    print("s:", solver.Value(s))
    print("e:", solver.Value(e))
    print("n:", solver.Value(n))
    print("d:", solver.Value(d))
    print("m:", solver.Value(m))
    print("o:", solver.Value(o))
    print("r:", solver.Value(r))
    print("y:", solver.Value(y))


def main(_):
    send_more_money()


if __name__ == "__main__":
    app.run(main)
