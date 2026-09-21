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
"""Divisible by 9 through 1 puzzle in Google CP Solver.

From http://msdn.microsoft.com/en-us/vcsharp/ee957404.aspx
' Solving Combinatory Problems with LINQ'
'''
Find a number consisting of 9 digits in which each of the digits
from 1 to 9 appears only once. This number must also satisfy these
divisibility requirements:

 1. The number should be divisible by 9.
 2. If the rightmost digit is removed, the remaining number should
    be divisible by 8.
 3. If the rightmost digit of the new number is removed, the remaining
    number should be divisible by 7.
 4. And so on, until there's only one digit (which will necessarily
    be divisible by 1).
'''

Also, see
'Intel Parallel Studio: Great for Serial Code Too (Episode 1)'
http://software.intel.com/en-us/blogs/2009/12/07/intel-parallel-studio-great-for-serial-code-too-episode-1/


This model is however generalized to handle any base, for reasonable limits.
The 'reasonable limit' for this model is that base must be between 2..16.

Compare with the following models:
* MiniZinc: http://www.hakank.org/minizinc/divisible_by_9_through_1.mzn
* Comet   : http://www.hakank.org/comet/divisible_by_9_through_1.co
* ECLiPSe : http://www.hakank.org/eclipse/divisible_by_9_through_1.ecl
* Gecode  : http://www.hakank.org/gecode/divisible_by_9_through_1.cpp


This model was created by Hakan Kjellerstrand (hakank@gmail.com)
Also see my other Google CP Solver models:
http://www.hakank.org/google_or_tools/
"""

import sys

from ortools.constraint_solver.python import constraint_solver as cp

#
# Decomposition of modulo constraint
#
# This implementation is based on the ECLiPSe version
# mentioned in
# - A Modulo propagator for ECLiPSE'
#   http://www.hakank.org/constraint_programming_blog/2010/05/a_modulo_propagator_for_eclips.html
# The ECLiPSe source code:
# http://www.hakank.org/eclipse/modulo_propagator.ecl
#


def my_mod(solver, x, y, r):

    if not isinstance(y, int):
        solver.add(y != 0)

    lbx = x.min()
    ubx = x.max()
    ubx_neg = -ubx
    lbx_neg = -lbx
    min_x = min(lbx, ubx_neg)
    max_x = max(ubx, lbx_neg)

    d = solver.new_int_var(max(0, min_x), max_x, "d")

    if not isinstance(r, int):
        solver.add(r >= 0)
        solver.add(x * r >= 0)

    if not isinstance(r, int) and not isinstance(r, int):
        solver.add(-abs(y) < r)
        solver.add(r < abs(y))

    solver.add(min_x <= d)
    solver.add(d <= max_x)
    solver.add(x == y * d + r)


#
# converts a number (s) <-> an array of integers (t) in the specific base.
#
def toNum(solver, t, s, base):
    tlen = len(t)
    solver.add(s == solver.sum([(base ** (tlen - i - 1)) * t[i] for i in range(tlen)]))


def main(base=10):

    # Create the solver.
    solver = cp.Solver("Divisible by 9 through 1")

    # data
    m = base ** (base - 1) - 1
    n = base - 1

    digits_str = "_0123456789ABCDEFGH"

    print("base:", base)

    # declare variables

    # the digits
    x = [solver.new_int_var(1, base - 1, "x[%i]" % i) for i in range(n)]

    # the numbers, t[0] contains the answer
    t = [solver.new_int_var(0, m, "t[%i]" % i) for i in range(n)]

    #
    # constraints
    #
    solver.add_all_different(x)

    for i in range(n):
        mm = base - i - 1
        toNum(solver, [x[j] for j in range(mm)], t[i], base)
        my_mod(solver, t[i], mm, 0)

    #
    # solution and search
    #
    solution = solver.assignment()
    solution.add(x)
    solution.add(t)

    db = solver.phase(
        x,
        cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
        cp.IntValueStrategy.ASSIGN_MIN_VALUE,
    )

    solver.new_search(db)
    num_solutions = 0
    while solver.next_solution():
        print("x: ", [x[i].value() for i in range(n)])
        print("t: ", [t[i].value() for i in range(n)])
        print(
            "number base 10: %i base %i: %s"
            % (
                t[0].value(),
                base,
                "".join([digits_str[x[i].value() + 1] for i in range(n)]),
            )
        )
        print()
        num_solutions += 1
    solver.end_search()

    print("num_solutions:", num_solutions)
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)


base = 10
default_base = 10
max_base = 16
if __name__ == "__main__":
    if len(sys.argv) > 1:
        base = int(sys.argv[1])
        if base > max_base:
            print(
                "Sorry, max allowed base is %i. Setting base to %i..."
                % (max_base, default_base)
            )
            base = default_base
    main(base)

    # for base in range(2, 17):
    #     print
    #     main(base)
