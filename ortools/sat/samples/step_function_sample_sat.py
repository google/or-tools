# Copyright 2010-2018 Google LLC
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
"""Implements a step function."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, variables):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__variables = variables
        self.__solution_count = 0

    def on_solution_callback(self):
        self.__solution_count += 1
        for v in self.__variables:
            print('%s=%i' % (v, self.Value(v)), end=' ')
        print()

    def solution_count(self):
        return self.__solution_count


def step_function_sample_sat():
    """Encode the step function."""

    # Model.
    model = cp_model.CpModel()

    # Declare our primary variable.
    x = model.NewIntVar(0, 20, 'x')

    # Create the expression variable and implement the step function
    # Note it is not defined for x == 2.
    #
    #        -               3
    # -- --      ---------   2
    #                        1
    #      -- ---            0
    # 0 ================ 20
    #
    expr = model.NewIntVar(0, 3, 'expr')

    # expr == 0 on [5, 6] U [8, 10]
    b0 = model.NewBoolVar('b0')
    model.AddLinearExpressionInDomain(x,
                                      cp_model.Domain.FromIntervals(
                                          [(5, 6), (8, 10)])).OnlyEnforceIf(b0)
    model.Add(expr == 0).OnlyEnforceIf(b0)

    # expr == 2 on [0, 1] U [3, 4] U [11, 20]
    b2 = model.NewBoolVar('b2')
    model.AddLinearExpressionInDomain(x,
                                      cp_model.Domain.FromIntervals(
                                          [(0, 1), (3, 4),
                                           (11, 20)])).OnlyEnforceIf(b2)
    model.Add(expr == 2).OnlyEnforceIf(b2)

    # expr == 3 when x == 7
    b3 = model.NewBoolVar('b3')
    model.Add(x == 7).OnlyEnforceIf(b3)
    model.Add(expr == 3).OnlyEnforceIf(b3)

    # At least one bi is true. (we could use a sum == 1).
    model.AddBoolOr([b0, b2, b3])

    # Search for x values in increasing order.
    model.AddDecisionStrategy([x], cp_model.CHOOSE_FIRST,
                              cp_model.SELECT_MIN_VALUE)

    # Create a solver and solve with a fixed search.
    solver = cp_model.CpSolver()

    # Force the solver to follow the decision strategy exactly.
    solver.parameters.search_branching = cp_model.FIXED_SEARCH

    # Search and print out all solutions.
    solution_printer = VarArraySolutionPrinter([x, expr])
    solver.SearchForAllSolutions(model, solution_printer)


step_function_sample_sat()
