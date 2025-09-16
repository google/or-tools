#!/usr/bin/env python3
# Copyright 2010-2025 Google LLC
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

# [START program]
"""Compute the index of the first Boolean variable set to true."""

from ortools.sat.python import cp_model


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, index: cp_model.IntVar, boolvars: list[cp_model.IntVar]):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__index = index
        self.__boolvars = boolvars

    def on_solution_callback(self) -> None:
        line = ""
        for v in self.__boolvars:
            line += f"{self.value(v)}"
        line += f" -> {self.value(self.__index)}"
        print(line)


def index_of_first_bool_at_true_sample_sat():
    """Compute the index of the first Boolean variable set to true."""

    # Model.
    model = cp_model.CpModel()

    # Variables
    num_bool_vars = 5
    bool_vars = [model.new_bool_var(f"{i}") for i in range(num_bool_vars)]
    index = model.new_int_var(0, num_bool_vars, "index")

    # Channeling between the index and the Boolean variables.
    model.add_min_equality(
        index,
        [
            num_bool_vars - bool_vars[i] * (num_bool_vars - i)
            for i in range(num_bool_vars)
        ],
    )

    # Flip bool_vars in increasing order.
    model.add_decision_strategy(
        bool_vars, cp_model.CHOOSE_FIRST, cp_model.SELECT_MIN_VALUE
    )

    # Create a solver and solve with a fixed search.
    solver = cp_model.CpSolver()

    # Force the solver to follow the decision strategy exactly.
    solver.parameters.search_branching = cp_model.FIXED_SEARCH

    # Search and print out all solutions.
    solver.parameters.enumerate_all_solutions = True
    solution_printer = VarArraySolutionPrinter(index, bool_vars)
    solver.solve(model, solution_printer)


index_of_first_bool_at_true_sample_sat()
# [END program]
