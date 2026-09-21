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
"""Max flow problem in Google CP Solver.

From Taha 'Introduction to Operations Research', Example 6.4-2

Translated from the AMPL code at
http://taha.ineg.uark.edu/maxflo.txt

Compare with the following model:
* MiniZinc: http://www.hakank.org/minizinc/max_flow_taha.mzn

This model was created by Hakan Kjellerstrand (hakank@gmail.com)
Also see my other Google CP Solver models:
http://www.hakank.org/google_or_tools/
"""

from ortools.constraint_solver.python import constraint_solver as cp


def main():

    # Create the solver.
    solver = cp.Solver("Max flow problem, Taha")

    #
    # data
    #
    n = 5
    start = 0
    end = n - 1

    nodes = list(range(n))

    # cost matrix
    c = [
        [0, 20, 30, 10, 0],
        [0, 0, 40, 0, 30],
        [0, 0, 0, 10, 20],
        [0, 0, 5, 0, 20],
        [0, 0, 0, 0, 0],
    ]

    #
    # declare variables
    #
    x = {}
    for i in nodes:
        for j in nodes:
            x[i, j] = solver.new_int_var(0, c[i][j], "x[%i,%i]" % (i, j))

    x_flat = [x[i, j] for i in nodes for j in nodes]
    out_flow = [solver.new_int_var(0, 10000, "out_flow[%i]" % i) for i in nodes]
    in_flow = [solver.new_int_var(0, 10000, "in_flow[%i]" % i) for i in nodes]

    total = solver.new_int_var(0, 10000, "z")

    #
    # constraints
    #
    cost_sum = solver.sum([x[start, j] for j in nodes if c[start][j] > 0])
    solver.add(total == cost_sum)

    for i in nodes:
        in_flow_sum = solver.sum([x[j, i] for j in nodes if c[j][i] > 0])
        solver.add(in_flow[i] == in_flow_sum)

        out_flow_sum = solver.sum([x[i, j] for j in nodes if c[i][j] > 0])
        solver.add(out_flow[i] == out_flow_sum)

    # in_flow == out_flow
    for i in nodes:
        if i != start and i != end:
            solver.add(out_flow[i] - in_flow[i] == 0)

    s1 = [x[i, start] for i in nodes if c[i][start] > 0]
    if len(s1) > 0:
        solver.add(solver.sum([x[i, start] for i in nodes if c[i][start] > 0] == 0))

    s2 = [x[end, j] for j in nodes if c[end][j] > 0]
    if len(s2) > 0:
        solver.add(solver.sum([x[end, j] for j in nodes if c[end][j] > 0]) == 0)

    # objective: maximize total cost
    objective = solver.maximize(total, 1)

    #
    # solution and search
    #
    db = solver.phase(
        x_flat,
        cp.IntVarStrategy.INT_VAR_DEFAULT,
        cp.IntValueStrategy.ASSIGN_MAX_VALUE,
    )

    solver.new_search(db, [objective])
    num_solutions = 0
    while solver.next_solution():
        num_solutions += 1
        print("total:", total.value())
        print("in_flow:", [in_flow[i].value() for i in nodes])
        print("out_flow:", [out_flow[i].value() for i in nodes])
        for i in nodes:
            for j in nodes:
                print("%2i" % x[i, j].value(), end=" ")
            print()
        print()

    print("num_solutions:", num_solutions)
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms, "ms")


if __name__ == "__main__":
    main()
