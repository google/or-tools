# Copyright 2010 Hakan Kjellerstrand hakank@gmail.com
#
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
"""Least diff problem in Google CP Solver.

This model solves the following problem:

What is the smallest difference between two numbers X - Y
if you must use all the digits (0..9) exactly once.

Compare with the following models:
* Choco   : http://www.hakank.org/choco/LeastDiff2.java
* ECLiPSE : http://www.hakank.org/eclipse/least_diff2.ecl
* Comet   : http://www.hakank.org/comet/least_diff.co
* Tailor/Essence': http://www.hakank.org/tailor/leastDiff.eprime
* Gecode  : http://www.hakank.org/gecode/least_diff.cpp
* Gecode/R: http://www.hakank.org/gecode_r/least_diff.rb
* JaCoP   : http://www.hakank.org/JaCoP/LeastDiff.java
* MiniZinc: http://www.hakank.org/minizinc/least_diff.mzn
* SICStus : http://www.hakank.org/sicstus/least_diff.pl
* Zinc    : http://hakank.org/minizinc/least_diff.zinc

This model was created by Hakan Kjellerstrand (hakank@gmail.com)
Also see my other Google CP Solver models:
http://www.hakank.org/google_cp_solver/
"""

from ortools.constraint_solver.python import constraint_solver as cp


def main(unused_argv):
    # Create the solver.
    solver = cp.Solver("Least diff")

    #
    # declare variables
    #
    digits = list(range(0, 10))
    a = solver.new_int_var(digits, "a")
    b = solver.new_int_var(digits, "b")
    c = solver.new_int_var(digits, "c")
    d = solver.new_int_var(digits, "d")
    e = solver.new_int_var(digits, "e")

    f = solver.new_int_var(digits, "f")
    g = solver.new_int_var(digits, "g")
    h = solver.new_int_var(digits, "h")
    i = solver.new_int_var(digits, "i")
    j = solver.new_int_var(digits, "j")

    letters = [a, b, c, d, e, f, g, h, i, j]

    digit_vector = [10000, 1000, 100, 10, 1]
    x = solver.weighted_sum(letters[0:5], digit_vector)
    y = solver.weighted_sum(letters[5:], digit_vector)
    diff = x - y

    #
    # constraints
    #
    solver.add(diff > 0)
    solver.add_all_different(letters)

    # objective
    objective = solver.minimize(diff, 1)

    #
    # solution
    #
    solution = solver.assignment()
    solution.add(letters)
    solution.add(x)
    solution.add(y)
    solution.add(diff)

    # last solution since it's a minimization problem
    collector = solver.last_solution_collector(solution)
    search_log = solver.search_log(100, diff)
    # Note: I'm not sure what CHOOSE_PATH do, but it is fast:
    #       find the solution in just 4 steps
    solver.solve(
        solver.phase(
            letters,
            cp.IntVarStrategy.CHOOSE_PATH,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE,
        ),
        [objective, search_log, collector],
    )

    # get the first (and only) solution

    xval = collector.value(0, x)
    yval = collector.value(0, y)
    diffval = collector.value(0, diff)
    print("x:", xval)
    print("y:", yval)
    print("diff:", diffval)
    print(xval, "-", yval, "=", diffval)
    print([("abcdefghij"[i], collector.value(0, letters[i])) for i in range(10)])
    print()
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)
    print()


if __name__ == "__main__":
    main("cp sample")
