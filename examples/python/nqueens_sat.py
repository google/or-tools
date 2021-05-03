# Copyright 2010-2021 Google LLC
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
"""OR-Tools solution to the N-queens problem."""
import time
import sys
from ortools.sat.python import cp_model


class NQueenSolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, queens):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__queens = queens
        self.__solution_count = 0
        self.__start_time = time.time()

    def solution_count(self):
        return self.__solution_count

    def on_solution_callback(self):
        current_time = time.time()
        print('Solution %i, time = %f s' % (self.__solution_count,
                                            current_time - self.__start_time))
        self.__solution_count += 1

        all_queens = range(len(self.__queens))
        for i in all_queens:
            for j in all_queens:
                if self.Value(self.__queens[j]) == i:
                    # There is a queen in column j, row i.
                    print('Q', end=' ')
                else:
                    print('_', end=' ')
            print()
        print()


def main(board_size):
    # Creates the solver.
    model = cp_model.CpModel()
    # Creates the variables.
    # The array index is the column, and the value is the row.
    queens = [
        model.NewIntVar(0, board_size - 1, 'x%i' % i) for i in range(board_size)
    ]
    # Creates the constraints.

    # All rows must be different.
    model.AddAllDifferent(queens)

    # All columns must be different because the indices of queens are all
    # different.

    # No two queens can be on the same diagonal.
    diag1 = []
    diag2 = []
    for i in range(board_size):
        q1 = model.NewIntVar(0, 2 * board_size, 'diag1_%i' % i)
        q2 = model.NewIntVar(-board_size, board_size, 'diag2_%i' % i)
        diag1.append(q1)
        diag2.append(q2)
        model.Add(q1 == queens[i] + i)
        model.Add(q2 == queens[i] - i)
    model.AddAllDifferent(diag1)
    model.AddAllDifferent(diag2)

    ### Solve model.
    solver = cp_model.CpSolver()
    solution_printer = NQueenSolutionPrinter(queens)
    solver.parameters.enumerate_all_solutions = True
    status = solver.Solve(model, solution_printer)

    print()
    print('Statistics')
    print('  - conflicts       : %i' % solver.NumConflicts())
    print('  - branches        : %i' % solver.NumBranches())
    print('  - wall time       : %f s' % solver.WallTime())
    print('  - solutions found : %i' % solution_printer.solution_count())


# By default, solve the 8x8 problem.
board_size = 8

if __name__ == '__main__':
    if len(sys.argv) > 1:
        board_size = int(sys.argv[1])
    main(board_size)
