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

"""Simple unit tests for python/linear_solver.swig. Not exhaustive."""

import unittest

from google.protobuf import text_format

from ortools.linear_solver import linear_solver_pb2, pywraplp

TEXT_MODEL = """
variable {
  lower_bound: 1.0
  upper_bound: 10.0
  objective_coefficient: 2.0
}
variable {
  lower_bound: 1.0
  upper_bound: 10.0
  objective_coefficient: 1.0
}
constraint {
  lower_bound: -10000.0
  upper_bound: 4.0
  var_index: 0
  var_index: 1
  coefficient: 1.0
  coefficient: 2.0
}
"""


class PyWrapLp(unittest.TestCase):

    def test_proto(self):
        input_proto = linear_solver_pb2.MPModelProto()
        text_format.Merge(TEXT_MODEL, input_proto)
        solver = pywraplp.Solver.CreateSolver("GLOP")
        if not solver:
            return
        # For now, create the model from the proto by parsing the proto
        error = solver.LoadModelFromProto(input_proto)
        self.assertEqual(error, "")
        solver.Solve()
        # Fill solution
        solution = linear_solver_pb2.MPSolutionResponse()
        solver.FillSolutionResponseProto(solution)
        self.assertEqual(solution.objective_value, 3.0)
        self.assertEqual(solution.variable_value[0], 1.0)
        self.assertEqual(solution.variable_value[1], 1.0)
        self.assertEqual(solution.best_objective_bound, 0.0)

    def test_external_api(self):
        solver = pywraplp.Solver.CreateSolver("GLOP")
        infinity = solver.Infinity()
        infinity2 = solver.infinity()
        self.assertEqual(infinity, infinity2)
        # x1, x2 and x3 are continuous non-negative variables.
        x1 = solver.NumVar(0.0, infinity, "x1")
        x2 = solver.NumVar(0.0, infinity, "x2")
        x3 = solver.NumVar(0.0, infinity, "x3")
        self.assertEqual(x1.Lb(), 0)
        self.assertEqual(x1.Ub(), infinity)
        self.assertFalse(x1.Integer())
        solver.Maximize(10 * x1 + 6 * x2 + 4 * x3 + 5)
        self.assertEqual(solver.Objective().Offset(), 5)
        c0 = solver.Add(10 * x1 + 4 * x2 + 5 * x3 <= 600, "ConstraintName0")
        c1 = solver.Add(2 * x1 + 2 * x2 + 6 * x3 <= 300)
        sum_of_vars = sum([x1, x2, x3])
        solver.Add(sum_of_vars <= 100.0, "OtherConstraintName")
        self.assertEqual(c1.Lb(), -infinity)
        self.assertEqual(c1.Ub(), 300)
        c1.SetLb(-100000)
        self.assertEqual(c1.Lb(), -100000)
        c1.SetUb(301)
        self.assertEqual(c1.Ub(), 301)

        solver.SetTimeLimit(10000)
        result_status = solver.Solve()

        # The problem has an optimal solution.
        self.assertEqual(result_status, pywraplp.Solver.OPTIMAL)
        self.assertAlmostEqual(x1.ReducedCost(), 0.0)
        self.assertAlmostEqual(c0.DualValue(), 0.6666666666666667)

    def run_linear_example_natural_language_api(self, optimization_problem_type):
        """Example of simple linear program with natural language API."""
        solver = pywraplp.Solver(
            "RunLinearExampleNaturalLanguageAPI", optimization_problem_type
        )
        infinity = solver.infinity()
        # x1, x2 and x3 are continuous non-negative variables.
        x1 = solver.NumVar(0.0, infinity, "x1")
        x2 = solver.NumVar(0.0, infinity, "x2")
        x3 = solver.NumVar(0.0, infinity, "x3")

        solver.Maximize(10 * x1 + 6 * x2 + 4 * x3)
        c0 = solver.Add(10 * x1 + 4 * x2 + 5 * x3 <= 600, "ConstraintName0")
        c1 = solver.Add(2 * x1 + 2 * x2 + 6 * x3 <= 300)
        sum_of_vars = sum([x1, x2, x3])
        c2 = solver.Add(sum_of_vars <= 100.0, "OtherConstraintName")

        self.solve_and_print(
            solver,
            [x1, x2, x3],
            [c0, c1, c2],
            optimization_problem_type != pywraplp.Solver.PDLP_LINEAR_PROGRAMMING,
        )
        self.assertAlmostEqual(sum_of_vars.solution_value(), 100.0)

    def run_linear_example_cpp_style_api(self, optimization_problem_type):
        """Example of simple linear program with the C++ style API."""
        solver = pywraplp.Solver("RunLinearExampleCppStyle", optimization_problem_type)
        infinity = solver.infinity()
        # x1, x2 and x3 are continuous non-negative variables.
        x1 = solver.NumVar(0.0, infinity, "x1")
        x2 = solver.NumVar(0.0, infinity, "x2")
        x3 = solver.NumVar(0.0, infinity, "x3")

        # Maximize 10 * x1 + 6 * x2 + 4 * x3.
        objective = solver.Objective()
        objective.SetCoefficient(x1, 10)
        objective.SetCoefficient(x2, 6)
        objective.SetCoefficient(x3, 4)
        objective.SetMaximization()

        # x1 + x2 + x3 <= 100.
        c0 = solver.Constraint(-infinity, 100.0, "c0")
        c0.SetCoefficient(x1, 1)
        c0.SetCoefficient(x2, 1)
        c0.SetCoefficient(x3, 1)

        # 10 * x1 + 4 * x2 + 5 * x3 <= 600.
        c1 = solver.Constraint(-infinity, 600.0, "c1")
        c1.SetCoefficient(x1, 10)
        c1.SetCoefficient(x2, 4)
        c1.SetCoefficient(x3, 5)

        # 2 * x1 + 2 * x2 + 6 * x3 <= 300.
        c2 = solver.Constraint(-infinity, 300.0, "c2")
        c2.SetCoefficient(x1, 2)
        c2.SetCoefficient(x2, 2)
        c2.SetCoefficient(x3, 6)

        self.solve_and_print(
            solver,
            [x1, x2, x3],
            [c0, c1, c2],
            optimization_problem_type != pywraplp.Solver.PDLP_LINEAR_PROGRAMMING,
        )

    def run_mixed_integer_example_cpp_style_api(self, optimization_problem_type):
        """Example of simple mixed integer program with the C++ style API."""
        solver = pywraplp.Solver(
            "RunMixedIntegerExampleCppStyle", optimization_problem_type
        )
        infinity = solver.infinity()
        # x1 and x2 are integer non-negative variables.
        x1 = solver.IntVar(0.0, infinity, "x1")
        x2 = solver.IntVar(0.0, infinity, "x2")

        # Maximize x1 + 10 * x2.
        objective = solver.Objective()
        objective.SetCoefficient(x1, 1)
        objective.SetCoefficient(x2, 10)
        objective.SetMaximization()

        # x1 + 7 * x2 <= 17.5.
        c0 = solver.Constraint(-infinity, 17.5, "c0")
        c0.SetCoefficient(x1, 1)
        c0.SetCoefficient(x2, 7)

        # x1 <= 3.5.
        c1 = solver.Constraint(-infinity, 3.5, "c1")
        c1.SetCoefficient(x1, 1)
        c1.SetCoefficient(x2, 0)

        self.solve_and_print(solver, [x1, x2], [c0, c1], True)

    def run_boolean_example_cpp_style_api(self, optimization_problem_type):
        """Example of simple boolean program with the C++ style API."""
        solver = pywraplp.Solver("RunBooleanExampleCppStyle", optimization_problem_type)
        # x1 and x2 are integer non-negative variables.
        x1 = solver.BoolVar("x1")
        x2 = solver.BoolVar("x2")

        # Minimize 2 * x1 + x2.
        objective = solver.Objective()
        objective.SetCoefficient(x1, 2)
        objective.SetCoefficient(x2, 1)
        objective.SetMinimization()

        # 1 <= x1 + 2 * x2 <= 3.
        c0 = solver.Constraint(1, 3, "c0")
        c0.SetCoefficient(x1, 1)
        c0.SetCoefficient(x2, 2)

        self.solve_and_print(solver, [x1, x2], [c0], True)

    def solve_and_print(self, solver, variable_list, constraint_list, is_precise):
        """Solve the problem and verify the solution."""
        self.assertEqual(solver.NumVariables(), len(variable_list))
        self.assertEqual(solver.NumConstraints(), len(constraint_list))

        result_status = solver.Solve()

        # The problem has an optimal solution.
        self.assertEqual(result_status, pywraplp.Solver.OPTIMAL)

        # The solution looks legit (when using solvers other than
        # GLOP_LINEAR_PROGRAMMING, verifying the solution is highly recommended!).
        if is_precise:
            self.assertTrue(solver.VerifySolution(1e-7, True))

        if not solver.IsMip():
            activities = solver.ComputeConstraintActivities()
            for constraint in constraint_list:
                self.assertIsNotNone(constraint.dual_value())
                self.assertIsNotNone(activities[constraint.index()])

    def test_api(self):
        all_names_and_problem_types = list(
            linear_solver_pb2.MPModelRequest.SolverType.items()
        )
        for name, problem_type in all_names_and_problem_types:
            with self.subTest(f"{name}: {problem_type}"):
                if not pywraplp.Solver.SupportsProblemType(problem_type):
                    continue
                if name.startswith("GUROBI"):
                    continue
                if name.startswith("KNAPSACK"):
                    continue
                if not name.startswith("SCIP"):
                    continue
                if name.endswith("LINEAR_PROGRAMMING"):
                    self.run_linear_example_natural_language_api(problem_type)
                    self.run_linear_example_cpp_style_api(problem_type)
                elif name.endswith("MIXED_INTEGER_PROGRAMMING"):
                    self.run_mixed_integer_example_cpp_style_api(problem_type)
                elif name.endswith("INTEGER_PROGRAMMING"):
                    self.run_boolean_example_cpp_style_api(problem_type)

    def test_set_hint(self):
        solver = pywraplp.Solver(
            "RunBooleanExampleCppStyle", pywraplp.Solver.GLOP_LINEAR_PROGRAMMING
        )
        # x1 and x2 are integer non-negative variables.
        x1 = solver.BoolVar("x1")
        x2 = solver.BoolVar("x2")

        # Minimize 2 * x1 + x2.
        objective = solver.Objective()
        objective.SetCoefficient(x1, 2)
        objective.SetCoefficient(x2, 1)
        objective.SetMinimization()

        # 1 <= x1 + 2 * x2 <= 3.
        c0 = solver.Constraint(1, 3, "c0")
        c0.SetCoefficient(x1, 1)
        c0.SetCoefficient(x2, 2)

        solver.SetHint([x1, x2], [1.0, 0.0])
        self.assertEqual(2, len(solver.variables()))
        self.assertEqual(1, len(solver.constraints()))

    def test_bop_infeasible(self):
        solver = pywraplp.Solver("test", pywraplp.Solver.BOP_INTEGER_PROGRAMMING)
        solver.EnableOutput()

        x = solver.IntVar(0, 10, "")
        solver.Add(x >= 20)

        result_status = solver.Solve()
        self.assertEqual(result_status, pywraplp.Solver.OPTIMAL)

    def test_load_solution_from_proto(self):
        solver = pywraplp.Solver("", pywraplp.Solver.GLOP_LINEAR_PROGRAMMING)
        self.assertFalse(
            solver.LoadSolutionFromProto(linear_solver_pb2.MPSolutionResponse())
        )

    def test_solve_from_proto(self):
        request_str = """
        model {
            maximize: false
            objective_offset: 0
            variable {
                lower_bound: 0
                upper_bound: 4
                objective_coefficient: 1
                is_integer: false
                name: "XONE"
            }
            variable {
                lower_bound: -1
                upper_bound: 1
                objective_coefficient: 4
                is_integer: false
                name: "YTWO"
            }
            variable {
                lower_bound: 0
                upper_bound: inf
                objective_coefficient: 9
                is_integer: false
                name: "ZTHREE"
            }
            constraint {
                lower_bound: -inf
                upper_bound: 5
                name: "LIM1"
                var_index: 0
                var_index: 1
                coefficient: 1
                coefficient: 1
            }
            constraint {
                lower_bound: 10
                upper_bound: inf
                name: "LIM2"
                var_index: 0
                var_index: 2
                coefficient: 1
                coefficient: 1
            }
            constraint {
                lower_bound: 7
                upper_bound: 7
                name: "MYEQN"
                var_index: 1
                var_index: 2
                coefficient: -1
                coefficient: 1
            }
            name: "NAME_LONGER_THAN_8_CHARACTERS"
        }
        solver_type: GLOP_LINEAR_PROGRAMMING
        solver_time_limit_seconds: 1.0
        solver_specific_parameters: ""
    """
        request = linear_solver_pb2.MPModelRequest()
        text_format.Parse(request_str, request)
        self.assertEqual(len(request.model.variable), 3)
        response = linear_solver_pb2.MPSolutionResponse()
        pywraplp.Solver.SolveWithProto(model_request=request, response=response)
        self.assertEqual(
            linear_solver_pb2.MPSolverResponseStatus.MPSOLVER_OPTIMAL,
            response.status,
        )

    def test_export_to_mps(self):
        """Test MPS export."""
        solver = pywraplp.Solver("ExportMps", pywraplp.Solver.GLOP_LINEAR_PROGRAMMING)
        infinity = solver.infinity()
        # x1, x2 and x3 are continuous non-negative variables.
        x1 = solver.NumVar(0.0, infinity, "x1")
        x2 = solver.NumVar(0.0, infinity, "x2")
        x3 = solver.NumVar(0.0, infinity, "x3")

        solver.Maximize(10 * x1 + 6 * x2 + 4 * x3)
        solver.Add(10 * x1 + 4 * x2 + 5 * x3 <= 600, "ConstraintName0")
        solver.Add(2 * x1 + 2 * x2 + 6 * x3 <= 300)
        sum_of_vars = sum([x1, x2, x3])
        solver.Add(sum_of_vars <= 100.0, "OtherConstraintName")

        mps_str = solver.ExportModelAsMpsFormat(fixed_format=False, obfuscate=False)
        self.assertIn("ExportMps", mps_str)


if __name__ == "__main__":
    unittest.main()
