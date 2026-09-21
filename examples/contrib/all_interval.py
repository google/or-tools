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
"""All interval problem in Google CP Solver.

 CSPLib problem number 7
 http://www.cs.st-andrews.ac.uk/~ianm/CSPLib/prob/prob007/index.html
 '''
 Given the twelve standard pitch-classes (c, c , d, ...), represented by
 numbers 0,1,...,11, find a series in which each pitch-class occurs exactly
 once and in which the musical intervals between neighbouring notes cover
 the full set of intervals from the minor second (1 semitone) to the major
 seventh (11 semitones). That is, for each of the intervals, there is a
 pair of neigbhouring pitch-classes in the series, between which this
 interval appears. The problem of finding such a series can be easily
 formulated as an instance of a more general arithmetic problem on Z_n,
 the set of integer residues modulo n. Given n in N, find a vector
 s = (s_1, ..., s_n), such that (i) s is a permutation of
 Z_n = {0,1,...,n-1}; and (ii) the interval vector
 v = (|s_2-s_1|, |s_3-s_2|, ... |s_n-s_{n-1}|) is a permutation of
 Z_n-{0} = {1,2,...,n-1}. A vector v satisfying these conditions is
 called an all-interval series of size n; the problem of finding such
 a series is the all-interval series problem of size n. We may also be
 interested in finding all possible series of a given size.
 '''

Compare with the following models:
 * MiniZinc: http://www.hakank.org/minizinc/all_interval.mzn
 * Comet   : http://www.hakank.org/comet/all_interval.co
 * Gecode/R: http://www.hakank.org/gecode_r/all_interval.rb
 * ECLiPSe : http://www.hakank.org/eclipse/all_interval.ecl
 * SICStus : http://www.hakank.org/sicstus/all_interval.pl


 This model was created by Hakan Kjellerstrand (hakank@gmail.com)
 Also see my other Google CP Solver models:
 http://www.hakank.org/google_or_tools/
"""

import sys

from ortools.constraint_solver.python import constraint_solver as cp


def main(n=12):

    # Create the solver.
    solver = cp.Solver("All interval")

    #
    # data
    #
    print("n:", n)

    #
    # declare variables
    #
    x = [solver.new_int_var(1, n, "x[%i]" % i) for i in range(n)]
    diffs = [solver.new_int_var(1, n - 1, "diffs[%i]" % i) for i in range(n - 1)]

    #
    # constraints
    #
    solver.add_all_different(x)
    solver.add_all_different(diffs)

    for k in range(n - 1):
        solver.add(diffs[k] == abs(x[k + 1] - x[k]))

    # symmetry breaking
    solver.add(x[0] < x[n - 1])
    solver.add(diffs[0] < diffs[1])

    #
    # solution and search
    #
    solution = solver.assignment()
    solution.add(x)
    solution.add(diffs)

    db = solver.phase(
        x,
        cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
        cp.IntValueStrategy.ASSIGN_MIN_VALUE,
    )

    solver.new_search(db)
    num_solutions = 0
    while solver.next_solution():
        print("x:", [x[i].value() for i in range(n)])
        print("diffs:", [diffs[i].value() for i in range(n - 1)])
        num_solutions += 1
        print()

    print("num_solutions:", num_solutions)
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)


n = 12
if __name__ == "__main__":
    if len(sys.argv) > 1:
        n = int(sys.argv[1])
    main(n)
