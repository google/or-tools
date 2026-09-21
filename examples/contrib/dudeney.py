# Copyright 2010 Pierre Schaus (pschaus@gmail.com)
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
from ortools.constraint_solver.python import constraint_solver as cp


def dudeney(n):
    solver = cp.Solver("Dudeney")
    x = [solver.new_int_var(list(range(10)), "x" + str(i)) for i in range(n)]
    nb = solver.new_int_var(list(range(3, 10**n)), "nb")
    s = solver.new_int_var(list(range(1, 9 * n + 1)), "s")

    solver.add(nb == s * s * s)
    solver.add(sum([10 ** (n - i - 1) * x[i] for i in range(n)]) == nb)
    solver.add(sum([x[i] for i in range(n)]) == s)

    solution = solver.assignment()
    solution.add(nb)
    collector = solver.all_solution_collector(solution)

    solver.solve(
        solver.phase(
            x,
            cp.IntVarStrategy.INT_VAR_DEFAULT,
            cp.IntValueStrategy.INT_VALUE_DEFAULT,
        ),
        [collector],
    )

    for i in range(collector.solution_count):
        nbsol = collector.value(i, nb)
        print(nbsol)

    print("#fails:", solver.num_failures)
    print("time:", solver.wall_time_ms, "ms")


if __name__ == "__main__":
    dudeney(6)
