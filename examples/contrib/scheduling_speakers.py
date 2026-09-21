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
"""Scheduling speakers problem in Google CP Solver.

From Rina Dechter, Constraint Processing, page 72
Scheduling of 6 speakers in 6 slots.

Compare with the following models:
* MiniZinc: http://www.hakank.org/minizinc/scheduling_speakers.mzn
* SICStus Prolog: http://www.hakank.org/sicstus/scheduling_speakers.pl
* ECLiPSe: http://hakank.org/eclipse/scheduling_speakers.ecl
* Gecode: http://hakank.org/gecode/scheduling_speakers.cpp

This model was created by Hakan Kjellerstrand (hakank@gmail.com)
Also see my other Google CP Solver models:
http://www.hakank.org/google_or_tools/
"""

from ortools.constraint_solver.python import constraint_solver as cp


def main():

    # Create the solver.
    solver = cp.Solver("Scheduling speakers")

    #
    # data
    #
    n = 6  # number of speakers

    # slots available to speak
    available = [
        # Reasoning:
        [3, 4, 5, 6],  # 2) the only one with 6 after speaker F -> 1
        [3, 4],  # 5) 3 or 4
        [2, 3, 4, 5],  # 3) only with 5 after F -> 1 and A -> 6
        [2, 3, 4],  # 4) only with 2 after C -> 5 and F -> 1
        [3, 4],  # 5) 3 or 4
        [1, 2, 3, 4, 5, 6],  # 1) the only with 1
    ]

    #
    # variables
    #
    x = [solver.new_int_var(1, n, "x[%i]" % i) for i in range(n)]

    #
    # constraints
    #
    solver.add_all_different(x)

    for i in range(n):
        solver.add_member_ct(x[i], available[i])

    #
    # search and result
    #
    db = solver.phase(
        x,
        cp.IntVarStrategy.INT_VAR_DEFAULT,
        cp.IntValueStrategy.INT_VALUE_DEFAULT,
    )

    solver.new_search(db)

    num_solutions = 0

    while solver.next_solution():
        num_solutions += 1
        print("x:", [x[i].value() for i in range(n)])

    solver.end_search()

    print()
    print("num_solutions:", num_solutions)
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms, "ms")


if __name__ == "__main__":
    main()
