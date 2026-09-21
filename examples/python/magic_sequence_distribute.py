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

"""Magic sequence problem.

This models aims at building a sequence of numbers such that the number of
occurrences of i in this sequence is equal to the value of the ith number.
It uses an aggregated formulation of the count expression called
distribute().

Usage: python magic_sequence_distribute.py NUMBER
"""

from absl import app, flags

from ortools.constraint_solver.python import constraint_solver as cp

FLAGS = flags.FLAGS


def main(argv):
    # Create the solver.
    solver = cp.Solver("magic sequence")

    # Create an array of IntVars to hold the answers.
    size = int(argv[1]) if len(argv) > 1 else 100
    all_values = list(range(0, size))
    all_vars = [solver.new_int_var(0, size, "vars_%d" % i) for i in all_values]

    # The number of variables equal to j shall be the value of all_vars[j].
    solver.add(solver.add_distribute(all_vars, all_values, all_vars))

    # The sum of all the values shall be equal to the size.
    # (This constraint is redundant, but speeds up the search.)
    solver.add(solver.sum(all_vars) == size)

    solver.new_search(
        solver.phase(
            all_vars,
            cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE,
        )
    )
    solver.next_solution()
    print(all_vars)
    solver.end_search()


if __name__ == "__main__":
    app.run(main)
