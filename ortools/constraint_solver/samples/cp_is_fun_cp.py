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

# [START program]
"""Cryptarithmetic puzzle.

First attempt to solve equation CP + IS + FUN = TRUE
where each letter represents a unique digit.

This problem has 72 different solutions in base 10.
"""
# [START import]
from ortools.constraint_solver.python import constraint_solver as cp

# [END import]


def main():
    # Constraint programming engine
    # [START solver]
    solver = cp.Solver("CP is fun!")
    # [END solver]

    # [START variables]
    base = 10

    # Decision variables.
    digits = list(range(0, base))
    digits_without_zero = list(range(1, base))
    c = solver.new_int_var(digits_without_zero, "C")
    p = solver.new_int_var(digits, "P")
    i = solver.new_int_var(digits_without_zero, "I")
    s = solver.new_int_var(digits, "S")
    f = solver.new_int_var(digits_without_zero, "F")
    u = solver.new_int_var(digits, "U")
    n = solver.new_int_var(digits, "N")
    t = solver.new_int_var(digits_without_zero, "T")
    r = solver.new_int_var(digits, "R")
    e = solver.new_int_var(digits, "E")

    # We need to group variables in a list to use the constraint AllDifferent.
    letters = [c, p, i, s, f, u, n, t, r, e]

    # Verify that we have enough digits.
    assert base >= len(letters)
    # [END variables]

    # Define constraints.
    # [START constraints]
    solver.add_all_different(letters)

    # CP + IS + FUN = TRUE
    solver.add(
        p + s + n + base * (c + i + u) + base * base * f
        == e + base * u + base * base * r + base * base * base * t
    )
    # [END constraints]

    # [START solve]
    solution_count = 0
    db = solver.phase(
        letters,
        cp.IntVarStrategy.INT_VAR_DEFAULT,
        cp.IntValueStrategy.INT_VALUE_DEFAULT,
    )
    solver.new_search(db)
    while solver.next_solution():
        print(list(f"{var.name}:{var.value()}" for var in letters))
        # Is CP + IS + FUN = TRUE?
        assert (
            base * c.value()
            + p.value()
            + base * i.value()
            + s.value()
            + base * base * f.value()
            + base * u.value()
            + n.value()
            == base * base * base * t.value()
            + base * base * r.value()
            + base * u.value()
            + e.value()
        )
        solution_count += 1
    solver.end_search()
    print(f"Number of solutions found: {solution_count}")
    # [END solve]


if __name__ == "__main__":
    main()
# [END program]
