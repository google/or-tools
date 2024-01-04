#!/usr/bin/env python3
# Copyright 2010-2024 Google LLC
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
    """solve the cryptarithmic puzzle SEND+MORE=MONEY."""
    model = cp_model.CpModel()

    # Create variables.
    # Since s is a leading digit, it can't be 0.
    s = model.new_int_var(1, 9, "s")
    e = model.new_int_var(0, 9, "e")
    n = model.new_int_var(0, 9, "n")
    d = model.new_int_var(0, 9, "d")
    # Since m is a leading digit, it can't be 0.
    m = model.new_int_var(1, 9, "m")
    o = model.new_int_var(0, 9, "o")
    r = model.new_int_var(0, 9, "r")
    y = model.new_int_var(0, 9, "y")

    # Create carry variables. c0 is true if the first column of addends carries
    # a 1, c2 is true if the second column carries a 1, and so on.
    c0 = model.new_bool_var("c0")
    c1 = model.new_bool_var("c1")
    c2 = model.new_bool_var("c2")
    c3 = model.new_bool_var("c3")

    # Force all letters to take on different values.
    model.add_all_different(s, e, n, d, m, o, r, y)

    # Column 0:
    model.add(c0 == m)

    # Column 1:
    model.add(c1 + s + m == o + 10 * c0)

    # Column 2:
    model.add(c2 + e + o == n + 10 * c1)

    # Column 3:
    model.add(c3 + n + r == e + 10 * c2)

    # Column 4:
    model.add(d + e == y + 10 * c3)

    # solve model.
    solver = cp_model.CpSolver()
    if solver.solve(model) == cp_model.OPTIMAL:
        print("Optimal solution found!")
    print("s:", solver.value(s))
    print("e:", solver.value(e))
    print("n:", solver.value(n))
    print("d:", solver.value(d))
    print("m:", solver.value(m))
    print("o:", solver.value(o))
    print("r:", solver.value(r))
    print("y:", solver.value(y))


def main(_):
    send_more_money()


if __name__ == "__main__":
    app.run(main)
