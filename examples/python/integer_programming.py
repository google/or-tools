#!/usr/bin/env python3
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
"""Integer programming examples that show how to use the APIs."""

from ortools.linear_solver import pywraplp
from ortools.init import pywrapinit


def Announce(solver, api_type):
    print('---- Integer programming example with ' + solver + ' (' + api_type +
          ') -----')


def RunIntegerExampleNaturalLanguageAPI(optimization_problem_type):
    """Example of simple integer program with natural language API."""

    solver = pywraplp.Solver.CreateSolver(optimization_problem_type)
    if not solver:
        return

    Announce(optimization_problem_type, 'natural language API')

    infinity = solver.infinity()
    # x1 and x2 are integer non-negative variables.
    x1 = solver.IntVar(0.0, infinity, 'x1')
    x2 = solver.IntVar(0.0, infinity, 'x2')

    solver.Minimize(x1 + 2 * x2)
    solver.Add(3 * x1 + 2 * x2 >= 17)

    SolveAndPrint(solver, [x1, x2])


def RunIntegerExampleCppStyleAPI(optimization_problem_type):
    """Example of simple integer program with the C++ style API."""
    solver = pywraplp.Solver.CreateSolver(optimization_problem_type)
    if not solver:
        return

    Announce(optimization_problem_type, 'C++ style API')

    infinity = solver.infinity()
    # x1 and x2 are integer non-negative variables.
    x1 = solver.IntVar(0.0, infinity, 'x1')
    x2 = solver.IntVar(0.0, infinity, 'x2')

    # Minimize x1 + 2 * x2.
    objective = solver.Objective()
    objective.SetCoefficient(x1, 1)
    objective.SetCoefficient(x2, 2)

    # 2 * x2 + 3 * x1 >= 17.
    ct = solver.Constraint(17, infinity)
    ct.SetCoefficient(x1, 3)
    ct.SetCoefficient(x2, 2)

    SolveAndPrint(solver, [x1, x2])


def SolveAndPrint(solver, variable_list):
    """Solve the problem and print the solution."""
    print('Number of variables = %d' % solver.NumVariables())
    print('Number of constraints = %d' % solver.NumConstraints())

    result_status = solver.Solve()

    # The problem has an optimal solution.
    assert result_status == pywraplp.Solver.OPTIMAL

    # The solution looks legit (when using solvers others than
    # GLOP_LINEAR_PROGRAMMING, verifying the solution is highly recommended!).
    assert solver.VerifySolution(1e-7, True)

    print('Problem solved in %f milliseconds' % solver.wall_time())

    # The objective value of the solution.
    print('Optimal objective value = %f' % solver.Objective().Value())

    # The value of each variable in the solution.
    for variable in variable_list:
        print('%s = %f' % (variable.name(), variable.solution_value()))

    print('Advanced usage:')
    print('Problem solved in %d branch-and-bound nodes' % solver.nodes())


def RunAllIntegerExampleNaturalLanguageAPI():
    RunIntegerExampleNaturalLanguageAPI('GLPK')
    RunIntegerExampleNaturalLanguageAPI('CBC')
    RunIntegerExampleNaturalLanguageAPI('SCIP')
    RunIntegerExampleNaturalLanguageAPI('SAT')
    RunIntegerExampleNaturalLanguageAPI('Gurobi')


def RunAllIntegerExampleCppStyleAPI():
    RunIntegerExampleCppStyleAPI('GLPK')
    RunIntegerExampleCppStyleAPI('CBC')
    RunIntegerExampleCppStyleAPI('SCIP')
    RunIntegerExampleCppStyleAPI('SAT')
    RunIntegerExampleCppStyleAPI('Gurobi')


def main():
    RunAllIntegerExampleNaturalLanguageAPI()
    RunAllIntegerExampleCppStyleAPI()


if __name__ == '__main__':
    pywrapinit.CppBridge.InitLogging('integer_programming.py')
    cpp_flags = pywrapinit.CppFlags()
    cpp_flags.logtostderr = True
    cpp_flags.log_prefix = False
    pywrapinit.CppBridge.SetFlags(cpp_flags)
    main()
