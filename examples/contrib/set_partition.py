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
"""Set partition problem in Google CP Solver.

Problem formulation from
http://www.koalog.com/resources/samples/PartitionProblem.java.html
'''
 This is a partition problem.
 Given the set S = {1, 2, ..., n},
 it consists in finding two sets A and B such that:

   A U B = S,
   |A| = |B|,
   sum(A) = sum(B),
   sum_squares(A) = sum_squares(B)

'''

This model uses a binary matrix to represent the sets.


Also, compare with other models which uses var sets:
* MiniZinc: http://www.hakank.org/minizinc/set_partition.mzn
* Gecode/R: http://www.hakank.org/gecode_r/set_partition.rb
* Comet: http://hakank.org/comet/set_partition.co
* Gecode: http://hakank.org/gecode/set_partition.cpp
* ECLiPSe: http://hakank.org/eclipse/set_partition.ecl
* SICStus: http://hakank.org/sicstus/set_partition.pl

This model was created by Hakan Kjellerstrand (hakank@gmail.com)
Also see my other Google CP Solver models:
http://www.hakank.org/google_or_tools/
"""

import sys

from ortools.constraint_solver.python import constraint_solver as cp


#
# Partition the sets (binary matrix representation).
#
def partition_sets(x, num_sets, n):
    solver = list(x.values())[0].solver

    for i in range(num_sets):
        for j in range(num_sets):
            if i != j:
                b = solver.sum([x[i, k] * x[j, k] for k in range(n)])
                solver.add(b == 0)

    # ensure that all integers is in
    # (exactly) one partition
    b = [x[i, j] for i in range(num_sets) for j in range(n)]
    solver.add(solver.sum(b) == n)


def main(n=16, num_sets=2):

    # Create the solver.
    solver = cp.Solver("Set partition")

    #
    # data
    #
    print("n:", n)
    print("num_sets:", num_sets)
    print()

    # Check sizes
    assert n % num_sets == 0, "Equal sets is not possible."

    #
    # variables
    #

    # the set
    a = {}
    for i in range(num_sets):
        for j in range(n):
            a[i, j] = solver.new_int_var(0, 1, "a[%i,%i]" % (i, j))

    a_flat = [a[i, j] for i in range(num_sets) for j in range(n)]

    #
    # constraints
    #

    # partition set
    partition_sets(a, num_sets, n)

    for i in range(num_sets):
        for j in range(i, num_sets):

            # same cardinality
            solver.add(
                solver.sum([a[i, k] for k in range(n)])
                == solver.sum([a[j, k] for k in range(n)])
            )

            # same sum
            solver.add(
                solver.sum([k * a[i, k] for k in range(n)])
                == solver.sum([k * a[j, k] for k in range(n)])
            )

            # same sum squared
            solver.add(
                solver.sum([(k * a[i, k]) * (k * a[i, k]) for k in range(n)])
                == solver.sum([(k * a[j, k]) * (k * a[j, k]) for k in range(n)])
            )

    # symmetry breaking for num_sets == 2
    if num_sets == 2:
        solver.add(a[0, 0] == 1)

    #
    # search and result
    #
    db = solver.phase(
        a_flat,
        cp.IntVarStrategy.INT_VAR_DEFAULT,
        cp.IntValueStrategy.INT_VALUE_DEFAULT,
    )

    solver.new_search(db)

    num_solutions = 0
    while solver.next_solution():
        a_val = {}
        for i in range(num_sets):
            for j in range(n):
                a_val[i, j] = a[i, j].value()

        sq = sum([(j + 1) * a_val[0, j] for j in range(n)])
        print("sums:", sq)
        sq2 = sum([((j + 1) * a_val[0, j]) ** 2 for j in range(n)])
        print("sums squared:", sq2)

        for i in range(num_sets):
            if sum([a_val[i, j] for j in range(n)]):
                print(i + 1, ":", end=" ")
                for j in range(n):
                    if a_val[i, j] == 1:
                        print(j + 1, end=" ")
                print()

        print()
        num_solutions += 1

    solver.end_search()

    print()
    print("num_solutions:", num_solutions)
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)


n = 16
num_sets = 2
if __name__ == "__main__":
    if len(sys.argv) > 1:
        n = int(sys.argv[1])
    if len(sys.argv) > 2:
        num_sets = int(sys.argv[2])

    main(n, num_sets)
