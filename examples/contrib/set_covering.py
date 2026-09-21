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

Placing of firestations, from Winston 'Operations Research', page 486.

Compare with the following models:
* MiniZinc: http://www.hakank.org/minizinc/set_covering.mzn
* ECLiPSe : http://www.hakank.org/eclipse/set_covering.ecl
* Comet   : http://www.hakank.org/comet/set_covering.co
* Gecode  : http://www.hakank.org/gecode/set_covering.cpp
* SICStus : http://www.hakank.org/sicstus/set_covering.pl


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
    min_distance = 15
    num_cities = 6

    distance = [
        [0, 10, 20, 30, 30, 20],
        [10, 0, 25, 35, 20, 10],
        [20, 25, 0, 15, 30, 20],
        [30, 35, 15, 0, 15, 25],
        [30, 20, 30, 15, 0, 14],
        [20, 10, 20, 25, 14, 0],
    ]

    #
    # declare variables
    #
    x = [solver.new_int_var(0, 1, "x[%i]" % i) for i in range(num_cities)]

    #
    # constraints
    #

    # objective to minimize
    z = solver.sum(x)

    # ensure that all cities are covered
    for i in range(num_cities):
        b = [x[j] for j in range(num_cities) if distance[i][j] <= min_distance]
        solver.add_sum_greater_or_equal(b, 1)

    objective = solver.minimize(z, 1)

    #
    # solution and search
    #
    solution = solver.assignment()
    solution.add(x)
    solution.add_objective(z)

    collector = solver.last_solution_collector(solution)
    solver.solve(
        solver.phase(
            x + [z],
            cp.IntVarStrategy.INT_VAR_DEFAULT,
            cp.IntValueStrategy.INT_VALUE_DEFAULT,
        ),
        [collector, objective],
    )

    print("z:", collector.objective_value(0))
    print("x:", [collector.value(0, x[i]) for i in range(num_cities)])

    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)


if __name__ == "__main__":
    main("cp sample")
