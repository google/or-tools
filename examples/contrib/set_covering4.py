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
"""Set partition and set covering in Google CP Solver.

 Example from the Swedish book
 Lundgren, Roennqvist, Vaebrand
 'Optimeringslaera' (translation: 'Optimization theory'),
 page 408.

 * Set partition:
   We want to minimize the cost of the alternatives which covers all the
   objects, i.e. all objects must be choosen. The requirement is than an
   object may be selected _exactly_ once.

   Note: This is 1-based representation

   Alternative        Cost        Object
   1                  19           1,6
   2                  16           2,6,8
   3                  18           1,4,7
   4                  13           2,3,5
   5                  15           2,5
   6                  19           2,3
   7                  15           2,3,4
   8                  17           4,5,8
   9                  16           3,6,8
   10                 15           1,6,7

   The problem has a unique solution of z = 49 where alternatives
        3, 5, and 9
   is selected.

 * Set covering:
   If we, however, allow that an object is selected _more than one time_,
   then the solution is z = 45 (i.e. less cost than the first problem),
   and the alternatives
        4, 8, and 10
   is selected, where object 5 is selected twice (alt. 4 and 8).
   It's an unique solution as well.


Compare with the following models:
 * MiniZinc: http://www.hakank.org/minizinc/set_covering4.mzn
 * Comet   : http://www.hakank.org/comet/set_covering4.co
 * ECLiPSe : http://www.hakank.org/eclipse/set_covering4.ecl
 * SICStus : http://www.hakank.org/sicstus/set_covering4.pl
 * Gecode  : http://www.hakank.org/gecode/set_covering4.cpp


 This model was created by Hakan Kjellerstrand (hakank@gmail.com)
 Also see my other Google CP Solver models:
 http://www.hakank.org/google_or_tools/
"""

from ortools.constraint_solver.python import constraint_solver as cp


def main(set_partition=1):

    # Create the solver.
    solver = cp.Solver("Set partition and set covering")

    #
    # data
    #
    num_alternatives = 10
    num_objects = 8

    # costs for the alternatives
    costs = [19, 16, 18, 13, 15, 19, 15, 17, 16, 15]

    # the alternatives, and their objects
    a = [
        # 1 2 3 4 5 6 7 8    the objects
        [1, 0, 0, 0, 0, 1, 0, 0],  # alternative 1
        [0, 1, 0, 0, 0, 1, 0, 1],  # alternative 2
        [1, 0, 0, 1, 0, 0, 1, 0],  # alternative 3
        [0, 1, 1, 0, 1, 0, 0, 0],  # alternative 4
        [0, 1, 0, 0, 1, 0, 0, 0],  # alternative 5
        [0, 1, 1, 0, 0, 0, 0, 0],  # alternative 6
        [0, 1, 1, 1, 0, 0, 0, 0],  # alternative 7
        [0, 0, 0, 1, 1, 0, 0, 1],  # alternative 8
        [0, 0, 1, 0, 0, 1, 0, 1],  # alternative 9
        [1, 0, 0, 0, 0, 1, 1, 0],  # alternative 10
    ]

    #
    # declare variables
    #
    x = [solver.new_int_var(0, 1, "x[%i]" % i) for i in range(num_alternatives)]

    #
    # constraints
    #

    # sum the cost of the choosen alternative,
    # to be minimized
    z = solver.weighted_sum(x, costs)

    #
    for j in range(num_objects):
        if set_partition == 1:
            solver.add_sum_equality(
                [x[i] * a[i][j] for i in range(num_alternatives)], 1
            )
        else:
            solver.add_sum_greater_or_equal(
                [x[i] * a[i][j] for i in range(num_alternatives)], 1
            )

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
            [x[i] for i in range(num_alternatives)],
            cp.IntVarStrategy.INT_VAR_DEFAULT,
            cp.IntValueStrategy.INT_VALUE_DEFAULT,
        ),
        [collector, objective],
    )

    print("z:", collector.objective_value(0))
    print(
        "selected alternatives:",
        [i + 1 for i in range(num_alternatives) if collector.value(0, x[i]) == 1],
    )

    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)


if __name__ == "__main__":
    print("Set partition:")
    main(1)

    print("\nSet covering:")
    main(0)
