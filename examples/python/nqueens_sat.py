#!/usr/bin/env python3
# Copyright 2010-2024 Google LLC
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

"""CP/SAT model for the N-queens problem."""

import time

from absl import app
from absl import flags
from ortools.sat.python import cp_model

_SIZE = flags.DEFINE_integer("size", 8, "Number of queens.")


class NQueenSolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, queens: list[cp_model.IntVar]):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__queens = queens
        self.__solution_count = 0
        self.__start_time = time.time()

    @property
    def solution_count(self) -> int:
        return self.__solution_count

    def on_solution_callback(self) -> None:
        current_time = time.time()
        print(
            "Solution %i, time = %f s"
            % (self.__solution_count, current_time - self.__start_time)
        )
        self.__solution_count += 1

        all_queens = range(len(self.__queens))
        for i in all_queens:
            for j in all_queens:
                if self.value(self.__queens[j]) == i:
                    # There is a queen in column j, row i.
                    print("Q", end=" ")
                else:
                    print("_", end=" ")
            print()
        print()


def main(_):
    board_size = _SIZE.value

    ### Creates the solver.
    model = cp_model.CpModel()

    ### Creates the variables.
    # The array index is the column, and the value is the row.
    queens = [
        model.new_int_var(0, board_size - 1, "x%i" % i) for i in range(board_size)
    ]

    ### Creates the constraints.

    # All columns must be different because the indices of queens are all
    # different, so we just add the all different constraint on the rows.
    model.add_all_different(queens)

    # No two queens can be on the same diagonal.
    diag1 = []
    diag2 = []
    for i in range(board_size):
        q1 = model.new_int_var(0, 2 * board_size, "diag1_%i" % i)
        q2 = model.new_int_var(-board_size, board_size, "diag2_%i" % i)
        diag1.append(q1)
        diag2.append(q2)
        model.add(q1 == queens[i] + i)
        model.add(q2 == queens[i] - i)
    model.add_all_different(diag1)
    model.add_all_different(diag2)

    ### Solve model.
    solver = cp_model.CpSolver()
    solution_printer = NQueenSolutionPrinter(queens)
    # Enumerate all solutions.
    solver.parameters.enumerate_all_solutions = True
    # solve.
    solver.solve(model, solution_printer)

    print()
    print("Statistics")
    print("  - conflicts       : %i" % solver.num_conflicts)
    print("  - branches        : %i" % solver.num_branches)
    print("  - wall time       : %f s" % solver.wall_time)
    print("  - solutions found : %i" % solution_printer.solution_count)


if __name__ == "__main__":
    app.run(main)
