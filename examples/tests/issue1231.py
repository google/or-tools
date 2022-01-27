#!/usr/bin/env python3
# Copyright 2010-2021 Google
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

"""
Cryptarithmetic puzzle

First attempt to solve equation CP + IS + FUN = TRUE
where each letter represents a unique digit.

This problem has 72 different solutions in base 10.
"""

from ortools.constraint_solver import pywrapcp
from os import abort

def CPIsFun():
  # Constraint programming engine
  solver = pywrapcp.Solver('CP is fun!');

  kBase = 10

  # Decision variables.
  digits = list(range(0, kBase))
  digits_without_zero = list(range(1, kBase))

  c = solver.IntVar(digits_without_zero, 'C');
  p = solver.IntVar(digits, 'P');
  i = solver.IntVar(digits_without_zero, 'I');
  s = solver.IntVar(digits, 'S');
  f = solver.IntVar(digits_without_zero, 'F');
  u = solver.IntVar(digits, 'U');
  n = solver.IntVar(digits, 'N');
  t = solver.IntVar(digits_without_zero, 'T');
  r = solver.IntVar(digits, 'R');
  e = solver.IntVar(digits, 'E');

  # We need to group variables in a list to use the constraint AllDifferent.
  letters = [c, p, i, s, f, u, n, t, r, e]

  # Verify that we have enough digits.
  assert kBase >= len(letters)

  # Define constraints.
  solver.Add(solver.AllDifferent(letters))

  # CP + IS + FUN = TRUE
  solver.Add (p + s + n + kBase * (c + i + u) + kBase * kBase * f ==
              e + kBase * u + kBase * kBase * r + kBase * kBase * kBase * t)

  db = solver.Phase(letters, solver.INT_VAR_DEFAULT,
                             solver.INT_VALUE_DEFAULT)
  solver.NewSearch(db)

  while solver.NextSolution():
    print(letters)
    # Is CP + IS + FUN = TRUE?
    assert (kBase*c.Value() +  p.Value() + kBase*i.Value() + s.Value() +
            kBase*kBase*f.Value() + kBase*u.Value() + n.Value() ==
            kBase*kBase*kBase*t.Value() + kBase*kBase*r.Value() +
            kBase*u.Value() + e.Value())

  solver.EndSearch()

  return


if __name__ == '__main__':
  CPIsFun()
