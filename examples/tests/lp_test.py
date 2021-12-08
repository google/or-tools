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
"""Tests for ortools.linear_solver.pywraplp."""

import unittest
from ortools.linear_solver import linear_solver_pb2
from ortools.linear_solver import pywraplp


class PyWrapLpTest(unittest.TestCase):
    def RunLinearExampleNaturalLanguageAPI(self, optimization_problem_type):
        """Example of simple linear program with natural language API."""
        solver = pywraplp.Solver('RunLinearExampleNaturalLanguageAPI',
                                 optimization_problem_type)
        infinity = solver.infinity()
        # x1, x2 and x3 are continuous non-negative variables.
        x1 = solver.NumVar(0.0, infinity, 'x1')
        x2 = solver.NumVar(0.0, infinity, 'x2')
        x3 = solver.NumVar(0.0, infinity, 'x3')

        solver.Maximize(10 * x1 + 6 * x2 + 4 * x3)
        c0 = solver.Add(10 * x1 + 4 * x2 + 5 * x3 <= 600, 'ConstraintName0')
        c1 = solver.Add(2 * x1 + 2 * x2 + 6 * x3 <= 300)
        sum_of_vars = sum([x1, x2, x3])
        c2 = solver.Add(sum_of_vars <= 100.0, 'OtherConstraintName')

        self.SolveAndPrint(solver, [x1, x2, x3], [c0, c1, c2])
        # Print a linear expression's solution value.
        print(('Sum of vars: %s = %s' % (sum_of_vars,
                                         sum_of_vars.solution_value())))

    def RunLinearExampleCppStyleAPI(self, optimization_problem_type):
        """Example of simple linear program with the C++ style API."""
        solver = pywraplp.Solver('RunLinearExampleCppStyle',
                                 optimization_problem_type)
        infinity = solver.infinity()
        # x1, x2 and x3 are continuous non-negative variables.
        x1 = solver.NumVar(0.0, infinity, 'x1')
        x2 = solver.NumVar(0.0, infinity, 'x2')
        x3 = solver.NumVar(0.0, infinity, 'x3')

        # Maximize 10 * x1 + 6 * x2 + 4 * x3.
        objective = solver.Objective()
        objective.SetCoefficient(x1, 10)
        objective.SetCoefficient(x2, 6)
        objective.SetCoefficient(x3, 4)
        objective.SetMaximization()

        # x1 + x2 + x3 <= 100.
        c0 = solver.Constraint(-infinity, 100.0, 'c0')
        c0.SetCoefficient(x1, 1)
        c0.SetCoefficient(x2, 1)
        c0.SetCoefficient(x3, 1)

        # 10 * x1 + 4 * x2 + 5 * x3 <= 600.
        c1 = solver.Constraint(-infinity, 600.0, 'c1')
        c1.SetCoefficient(x1, 10)
        c1.SetCoefficient(x2, 4)
        c1.SetCoefficient(x3, 5)

        # 2 * x1 + 2 * x2 + 6 * x3 <= 300.
        c2 = solver.Constraint(-infinity, 300.0, 'c2')
        c2.SetCoefficient(x1, 2)
        c2.SetCoefficient(x2, 2)
        c2.SetCoefficient(x3, 6)

        self.SolveAndPrint(solver, [x1, x2, x3], [c0, c1, c2])

    def RunMixedIntegerExampleCppStyleAPI(self, optimization_problem_type):
        """Example of simple mixed integer program with the C++ style API."""
        solver = pywraplp.Solver('RunMixedIntegerExampleCppStyle',
                                 optimization_problem_type)
        infinity = solver.infinity()
        # x1 and x2 are integer non-negative variables.
        x1 = solver.IntVar(0.0, infinity, 'x1')
        x2 = solver.IntVar(0.0, infinity, 'x2')

        # Maximize x1 + 10 * x2.
        objective = solver.Objective()
        objective.SetCoefficient(x1, 1)
        objective.SetCoefficient(x2, 10)
        objective.SetMaximization()

        # x1 + 7 * x2 <= 17.5.
        c0 = solver.Constraint(-infinity, 17.5, 'c0')
        c0.SetCoefficient(x1, 1)
        c0.SetCoefficient(x2, 7)

        # x1 <= 3.5.
        c1 = solver.Constraint(-infinity, 3.5, 'c1')
        c1.SetCoefficient(x1, 1)
        c1.SetCoefficient(x2, 0)

        self.SolveAndPrint(solver, [x1, x2], [c0, c1])

    def RunBooleanExampleCppStyleAPI(self, optimization_problem_type):
        """Example of simple boolean program with the C++ style API."""
        solver = pywraplp.Solver('RunBooleanExampleCppStyle',
                                 optimization_problem_type)
        # x1 and x2 are integer non-negative variables.
        x1 = solver.BoolVar('x1')
        x2 = solver.BoolVar('x2')

        # Minimize 2 * x1 + x2.
        objective = solver.Objective()
        objective.SetCoefficient(x1, 2)
        objective.SetCoefficient(x2, 1)
        objective.SetMinimization()

        # 1 <= x1 + 2 * x2 <= 3.
        c0 = solver.Constraint(1, 3, 'c0')
        c0.SetCoefficient(x1, 1)
        c0.SetCoefficient(x2, 2)

        self.SolveAndPrint(solver, [x1, x2], [c0])

    def SolveAndPrint(self, solver, variable_list, constraint_list, tolerance=1e-7):
        """Solve the problem and print the solution."""
        print(('Number of variables = %d' % solver.NumVariables()))
        self.assertEqual(solver.NumVariables(), len(variable_list))

        print(('Number of constraints = %d' % solver.NumConstraints()))
        self.assertEqual(solver.NumConstraints(), len(constraint_list))

        result_status = solver.Solve()

        # The problem has an optimal solution.
        self.assertEqual(result_status, pywraplp.Solver.OPTIMAL)

        # The solution looks legit (when using solvers others than
        # GLOP_LINEAR_PROGRAMMING, verifying the solution is highly recommended!).
        self.assertTrue(solver.VerifySolution(tolerance, True))

        print(('Problem solved in %f milliseconds' % solver.wall_time()))

        # The objective value of the solution.
        print(('Optimal objective value = %f' % solver.Objective().Value()))

        # The value of each variable in the solution.
        for variable in variable_list:
            print(('%s = %f' % (variable.name(), variable.solution_value())))

        print('Advanced usage:')
        print(('Problem solved in %d iterations' % solver.iterations()))
        for variable in variable_list:
            print(('%s: reduced cost = %f' % (variable.name(),
                                              variable.reduced_cost())))
        activities = solver.ComputeConstraintActivities()
        for i, constraint in enumerate(constraint_list):
            print(
                ('constraint %d: dual value = %f\n'
                 '               activity = %f' %
                 (i, constraint.dual_value(), activities[constraint.index()])))

    def testApi(self):
        all_names_and_problem_types = (list(
            linear_solver_pb2.MPModelRequest.SolverType.items()))
        for name, problem_type in all_names_and_problem_types:
            with self.subTest(f'{name}: {problem_type}'):
                if not pywraplp.Solver.SupportsProblemType(problem_type):
                    continue
                if name.startswith('GUROBI'):
                    continue
                if name.endswith('LINEAR_PROGRAMMING'):
                    print(('\n------ Linear programming example with %s ------' %
                           name))
                    print('\n*** Natural language API ***')
                    self.RunLinearExampleNaturalLanguageAPI(problem_type)
                    print('\n*** C++ style API ***')
                    self.RunLinearExampleCppStyleAPI(problem_type)
                elif name.endswith('MIXED_INTEGER_PROGRAMMING'):
                    print((
                        '\n------ Mixed Integer programming example with %s ------'
                        % name))
                    print('\n*** C++ style API ***')
                    self.RunMixedIntegerExampleCppStyleAPI(problem_type)
                elif name.endswith('INTEGER_PROGRAMMING'):
                    print(('\n------ Boolean programming example with %s ------' %
                           name))
                    print('\n*** C++ style API ***')
                    self.RunBooleanExampleCppStyleAPI(problem_type)
                else:
                    print('ERROR: %s unsupported' % name)

    def testSetHint(self):
        print('testSetHint')
        solver = pywraplp.Solver('RunBooleanExampleCppStyle',
                                 pywraplp.Solver.GLOP_LINEAR_PROGRAMMING)
        infinity = solver.infinity()
        # x1 and x2 are integer non-negative variables.
        x1 = solver.BoolVar('x1')
        x2 = solver.BoolVar('x2')

        # Minimize 2 * x1 + x2.
        objective = solver.Objective()
        objective.SetCoefficient(x1, 2)
        objective.SetCoefficient(x2, 1)
        objective.SetMinimization()

        # 1 <= x1 + 2 * x2 <= 3.
        c0 = solver.Constraint(1, 3, 'c0')
        c0.SetCoefficient(x1, 1)
        c0.SetCoefficient(x2, 2)

        solver.SetHint([x1, x2], [1.0, 0.0])
        self.assertEqual(2, len(solver.variables()))
        self.assertEqual(1, len(solver.constraints()))

    def testBopInfeasible(self):
        print('testBopInfeasible')
        solver = pywraplp.Solver('test', pywraplp.Solver.BOP_INTEGER_PROGRAMMING)
        solver.EnableOutput()

        x = solver.IntVar(0, 10, "")
        solver.Add(x >= 20)

        result_status = solver.Solve()
        print(result_status) # outputs: 0

    def testSolveFromProto(self):
        solver = pywraplp.Solver('', pywraplp.Solver.GLOP_LINEAR_PROGRAMMING)
        solver.LoadSolutionFromProto(linear_solver_pb2.MPSolutionResponse())


if __name__ == '__main__':
    unittest.main()
