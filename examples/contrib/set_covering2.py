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
"""Set covering in Google CP Solver.

Example 9.1-2, page 354ff, from
Taha 'Operations Research - An Introduction'
Minimize the number of security telephones in street
corners on a campus.

Compare with the following models:
* MiniZinc: http://www.hakank.org/minizinc/set_covering2.mzn
* Comet   : http://www.hakank.org/comet/set_covering2.co
* ECLiPSe : http://www.hakank.org/eclipse/set_covering2.ecl
* SICStus: http://hakank.org/sicstus/set_covering2.pl
* Gecode: http://hakank.org/gecode/set_covering2.cpp

This model was created by Hakan Kjellerstrand (hakank@gmail.com)
Also see my other Google CP Solver models:
http://www.hakank.org/google_or_tools/
"""

from ortools.constraint_solver.python import constraint_solver as cp


def main(unused_argv):

    # Create the solver.
    solver = cp.Solver("Set covering")

    #
    # data
    #
    n = 8  # maximum number of corners
    num_streets = 11  # number of connected streets

    # corners of each street
    # Note: 1-based (handled below)
    corner = [
        [1, 2],
        [2, 3],
        [4, 5],
        [7, 8],
        [6, 7],
        [2, 6],
        [1, 6],
        [4, 7],
        [2, 4],
        [5, 8],
        [3, 5],
    ]

    #
    # declare variables
    #
    x = [solver.new_int_var(0, 1, "x[%i]" % i) for i in range(n)]

    #
    # constraints
    #

    # number of telephones, to be minimized
    z = solver.sum(x)

    # ensure that all corners are covered
    for i in range(num_streets):
        # also, convert to 0-based
        solver.add_sum_greater_or_equal([x[j - 1] for j in corner[i]], 1)

    objective = solver.minimize(z.var(), 1)

    #
    # solution and search
    #
    solution = solver.assignment()
    solution.add(x)
    solution.add_objective(z.var())

    collector = solver.last_solution_collector(solution)
    solver.solve(
        solver.phase(
            x,
            cp.IntVarStrategy.INT_VAR_DEFAULT,
            cp.IntValueStrategy.INT_VALUE_DEFAULT,
        ),
        [collector, objective],
    )

    print("z:", collector.objective_value(0))
    print("x:", [collector.value(0, x[i]) for i in range(n)])

    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)


if __name__ == "__main__":
    main("cp sample")
