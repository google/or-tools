#!/usr/bin/env python3
# Copyright 2010-2021 Google LLC
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
from ortools.constraint_solver import pywrapcp
# [END import]


def main():
    # Constraint programming engine
    # [START solver]
    solver = pywrapcp.Solver('CP is fun!')
    # [END solver]

    # [START variables]
    base = 10

    # Decision variables.
    digits = list(range(0, base))
    digits_without_zero = list(range(1, base))
    c = solver.IntVar(digits_without_zero, 'C')
    p = solver.IntVar(digits, 'P')
    i = solver.IntVar(digits_without_zero, 'I')
    s = solver.IntVar(digits, 'S')
    f = solver.IntVar(digits_without_zero, 'F')
    u = solver.IntVar(digits, 'U')
    n = solver.IntVar(digits, 'N')
    t = solver.IntVar(digits_without_zero, 'T')
    r = solver.IntVar(digits, 'R')
    e = solver.IntVar(digits, 'E')

    # We need to group variables in a list to use the constraint AllDifferent.
    letters = [c, p, i, s, f, u, n, t, r, e]

    # Verify that we have enough digits.
    assert base >= len(letters)
    # [END variables]

    # Define constraints.
    # [START constraints]
    solver.Add(solver.AllDifferent(letters))

    # CP + IS + FUN = TRUE
    solver.Add(p + s + n + base * (c + i + u) + base * base * f == e +
               base * u + base * base * r + base * base * base * t)
    # [END constraints]

    # [START solve]
    solution_count = 0
    db = solver.Phase(letters, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT)
    solver.NewSearch(db)
    while solver.NextSolution():
        print(letters)
        # Is CP + IS + FUN = TRUE?
        assert (base * c.Value() + p.Value() + base * i.Value() + s.Value() +
                base * base * f.Value() + base * u.Value() +
                n.Value() == base * base * base * t.Value() +
                base * base * r.Value() + base * u.Value() + e.Value())
        solution_count += 1
    solver.EndSearch()
    print(f'Number of solutions found: {solution_count}')
    # [END solve]


if __name__ == '__main__':
    main()
# [END program]
