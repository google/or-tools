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
"""All different except 0 Google CP Solver.

Decomposition of global constraint alldifferent_except_0.

From Global constraint catalogue:
http://www.emn.fr/x-info/sdemasse/gccat/Calldifferent_except_0.html
'''
Enforce all variables of the collection VARIABLES to take distinct
values, except those variables that are assigned to 0.

Example
   (<5, 0, 1, 9, 0, 3>)

The alldifferent_except_0 constraint holds since all the values
(that are different from 0) 5, 1, 9 and 3 are distinct.
'''

Compare with the following models:
* Comet: http://hakank.org/comet/alldifferent_except_0.co
* ECLiPSe: http://hakank.org/eclipse/alldifferent_except_0.ecl
* Tailor/Essence': http://hakank.org/tailor/alldifferent_except_0.eprime
* Gecode: http://hakank.org/gecode/alldifferent_except_0.cpp
* Gecode/R: http://hakank.org/gecode_r/all_different_except_0.rb
* MiniZinc: http://hakank.org/minizinc/alldifferent_except_0.mzn
* SICStus_ http://hakank.org/sicstus/alldifferent_except_0.pl
* Choco: http://hakank.org/choco/AllDifferentExcept0_test.java
* JaCoP: http://hakank.org/JaCoP/AllDifferentExcept0_test.java
* Zinc: http://hakank.org/minizinc/alldifferent_except_0.zinc

This model was created by Hakan Kjellerstrand (hakank@gmail.com)
Also see my other Google CP Solver models:
http://www.hakank.org/google_or_tools/
"""

from ortools.constraint_solver.python import constraint_solver as cp

#
# Decomposition of alldifferent_except_0
# Thanks to Laurent Perron (Google) for
# suggestions of improvements.
#


def alldifferent_except_0(solver, a):
    n = len(a)
    for i in range(n):
        for j in range(i):
            solver.add((a[i] != 0) * (a[j] != 0) <= (a[i] != a[j]))


# more compact version:


def alldifferent_except_0_b(solver, a):
    n = len(a)
    [
        solver.add((a[i] != 0) * (a[j] != 0) <= (a[i] != a[j]))
        for i in range(n)
        for j in range(i)
    ]


def main(unused_argv):
    # Create the solver.
    solver = cp.Solver("Alldifferent except 0")

    # data
    n = 7

    # declare variables
    x = [solver.new_int_var(0, n - 1, "x%i" % i) for i in range(n)]
    # Number of zeros.
    z = solver.sum([x[i] == 0 for i in range(n)]).var_with_name("z")

    #
    # constraints
    #
    alldifferent_except_0(solver, x)

    # we require 2 0's
    solver.add(z == 2)

    #
    # solution and search
    #
    solution = solver.assignment()
    solution.add([x[i] for i in range(n)])
    solution.add(z)

    collector = solver.all_solution_collector(solution)
    solver.solve(
        solver.phase(
            [x[i] for i in range(n)],
            cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE,
        ),
        [collector],
    )

    num_solutions = collector.solution_count
    for s in range(num_solutions):
        print("x:", [collector.value(s, x[i]) for i in range(n)])
        print("z:", collector.value(s, z))
        print()

    print("num_solutions:", num_solutions)
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)


if __name__ == "__main__":
    main("cp sample")
