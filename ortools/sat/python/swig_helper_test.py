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

"""Unit tests for ortools.sat.python.swig_helper."""

from absl.testing import absltest
from google.protobuf import text_format
from ortools.sat import cp_model_pb2
from ortools.sat import sat_parameters_pb2
from ortools.sat.python import swig_helper


class Callback(swig_helper.SolutionCallback):

    def __init__(self):
        swig_helper.SolutionCallback.__init__(self)
        self.__solution_count = 0

    def OnSolutionCallback(self):
        print("New Solution")
        self.__solution_count += 1

    def solution_count(self):
        return self.__solution_count


class BestBoundCallback:

    def __init__(self):
        self.best_bound: float = 0.0

    def new_best_bound(self, bb: float):
        self.best_bound = bb


class TestIntVar(swig_helper.BaseIntVar):

    def __init__(self, index: int, name: str, is_boolean: bool = False) -> None:
        swig_helper.BaseIntVar.__init__(self, index, is_boolean)
        self._name = name

    def __str__(self) -> str:
        return self._name

    def __repr__(self) -> str:
        return self._name


class SwigHelperTest(absltest.TestCase):

    def testVariableDomain(self):
        model_string = """
      variables { domain: [ -10, 10 ] }
      variables { domain: [ -5, -5, 3, 6 ] }
      """
        model = cp_model_pb2.CpModelProto()
        self.assertTrue(text_format.Parse(model_string, model))

        d0 = swig_helper.CpSatHelper.variable_domain(model.variables[0])
        d1 = swig_helper.CpSatHelper.variable_domain(model.variables[1])

        self.assertEqual(d0.flattened_intervals(), [-10, 10])
        self.assertEqual(d1.flattened_intervals(), [-5, -5, 3, 6])

    def testSimpleSolve(self):
        model_string = """
      variables { domain: -10 domain: 10 }
      variables { domain: -10 domain: 10 }
      variables { domain: -461168601842738790 domain: 461168601842738790 }
      constraints {
        linear {
          vars: 0
          vars: 1
          coeffs: 1
          coeffs: 1
          domain: -9223372036854775808
          domain: 9223372036854775807
        }
      }
      constraints {
        linear {
          vars: 0
          vars: 1
          vars: 2
          coeffs: 1
          coeffs: 2
          coeffs: -1
          domain: 0
          domain: 9223372036854775807
        }
      }
      objective {
        vars: -3
        coeffs: 1
        scaling_factor: -1
      }"""
        model = cp_model_pb2.CpModelProto()
        self.assertTrue(text_format.Parse(model_string, model))

        solve_wrapper = swig_helper.SolveWrapper()
        response_wrapper = solve_wrapper.solve(model)

        self.assertEqual(cp_model_pb2.OPTIMAL, response_wrapper.status())
        self.assertEqual(30.0, response_wrapper.objective_value())

    def testSimpleSolveWithCore(self):
        model_string = """
      variables { domain: -10 domain: 10 }
      variables { domain: -10 domain: 10 }
      variables { domain: -461168601842738790 domain: 461168601842738790 }
      constraints {
        linear {
          vars: 0
          vars: 1
          coeffs: 1
          coeffs: 1
          domain: -9223372036854775808
          domain: 9223372036854775807
        }
      }
      constraints {
        linear {
          vars: 0
          vars: 1
          vars: 2
          coeffs: 1
          coeffs: 2
          coeffs: -1
          domain: 0
          domain: 9223372036854775807
        }
      }
      objective {
        vars: -3
        coeffs: 1
        scaling_factor: -1
      }"""
        model = cp_model_pb2.CpModelProto()
        self.assertTrue(text_format.Parse(model_string, model))

        parameters = sat_parameters_pb2.SatParameters(optimize_with_core=True)

        solve_wrapper = swig_helper.SolveWrapper()
        solve_wrapper.set_parameters(parameters)
        response_wrapper = solve_wrapper.solve(model)

        self.assertEqual(cp_model_pb2.OPTIMAL, response_wrapper.status())
        self.assertEqual(30.0, response_wrapper.objective_value())

    def testSimpleSolveWithProtoApi(self):
        model = cp_model_pb2.CpModelProto()
        x = model.variables.add()
        x.domain.extend([-10, 10])
        y = model.variables.add()
        y.domain.extend([-10, 10])
        obj_var = model.variables.add()
        obj_var.domain.extend([-461168601842738790, 461168601842738790])
        ct = model.constraints.add()
        ct.linear.vars.extend([0, 1, 2])
        ct.linear.coeffs.extend([1, 2, -1])
        ct.linear.domain.extend([0, 0])
        model.objective.vars.append(-3)
        model.objective.coeffs.append(1)
        model.objective.scaling_factor = -1

        solve_wrapper = swig_helper.SolveWrapper()
        response_wrapper = solve_wrapper.solve(model)

        self.assertEqual(cp_model_pb2.OPTIMAL, response_wrapper.status())
        self.assertEqual(30.0, response_wrapper.objective_value())
        self.assertEqual(30.0, response_wrapper.best_objective_bound())

    def testSolutionCallback(self):
        model_string = """
      variables { domain: 0 domain: 5 }
      variables { domain: 0 domain: 5 }
      constraints {
        linear { vars: 0 vars: 1 coeffs: 1 coeffs: 1 domain: 6 domain: 6 } }
      """
        model = cp_model_pb2.CpModelProto()
        self.assertTrue(text_format.Parse(model_string, model))

        solve_wrapper = swig_helper.SolveWrapper()
        callback = Callback()
        solve_wrapper.add_solution_callback(callback)
        params = sat_parameters_pb2.SatParameters()
        params.enumerate_all_solutions = True
        solve_wrapper.set_parameters(params)
        response_wrapper = solve_wrapper.solve(model)

        self.assertEqual(5, callback.solution_count())
        self.assertEqual(cp_model_pb2.OPTIMAL, response_wrapper.status())

    def testBestBoundCallback(self):
        model_string = """
      variables { domain: 0 domain: 1 }
      variables { domain: 0 domain: 1 }
      variables { domain: 0 domain: 1 }
      variables { domain: 0 domain: 1 }
      constraints { bool_or { literals: [0, 1, 2, 3] } }
      objective {
        vars: [0, 1, 2, 3]
        coeffs: [3, 2, 4, 5]
        offset: 0.6
      }
      """
        model = cp_model_pb2.CpModelProto()
        self.assertTrue(text_format.Parse(model_string, model))

        solve_wrapper = swig_helper.SolveWrapper()
        best_bound_callback = BestBoundCallback()
        solve_wrapper.add_best_bound_callback(best_bound_callback.new_best_bound)
        params = sat_parameters_pb2.SatParameters()
        params.num_workers = 1
        params.linearization_level = 2
        params.log_search_progress = True
        solve_wrapper.set_parameters(params)
        response_wrapper = solve_wrapper.solve(model)

        self.assertEqual(2.6, best_bound_callback.best_bound)
        self.assertEqual(cp_model_pb2.OPTIMAL, response_wrapper.status())

    def testModelStats(self):
        model_string = """
      variables { domain: -10 domain: 10 }
      variables { domain: -10 domain: 10 }
      variables { domain: -1000 domain: 1000 }
      constraints {
        linear {
          vars: 0
          vars: 1
          coeffs: 1
          coeffs: 1
          domain: -1000
          domain: 1000
        }
      }
      constraints {
        linear {
          vars: 0
          vars: 1
          vars: 2
          coeffs: 1
          coeffs: 2
          coeffs: -1
          domain: 0
          domain: 1000
        }
      }
      objective {
        vars: -3
        coeffs: 1
        scaling_factor: -1
      }
      name: 'testModelStats'
      """
        model = cp_model_pb2.CpModelProto()
        self.assertTrue(text_format.Parse(model_string, model))
        stats = swig_helper.CpSatHelper.model_stats(model)
        self.assertTrue(stats)

    def testIntLinExpr(self):
        x = TestIntVar(0, "x")
        self.assertTrue(x.is_integer())
        self.assertIsInstance(x, swig_helper.BaseIntVar)
        self.assertIsInstance(x, swig_helper.LinearExpr)
        e1 = x + 2
        self.assertTrue(e1.is_integer())
        self.assertEqual(str(e1), "(x + 2)")
        e2 = 3 + x
        self.assertTrue(e2.is_integer())
        self.assertEqual(str(e2), "(x + 3)")
        y = TestIntVar(1, "y")
        e3 = y * 5
        self.assertTrue(e3.is_integer())
        self.assertEqual(str(e3), "(5 * y)")
        e4 = -2 * y
        self.assertTrue(e4.is_integer())
        self.assertEqual(str(e4), "(-2 * y)")
        e5 = x - 1
        self.assertTrue(e5.is_integer())
        self.assertEqual(str(e5), "(x - 1)")
        e6 = x - 2 * y
        self.assertTrue(e6.is_integer())
        self.assertEqual(str(e6), "(x - (2 * y))")
        z = TestIntVar(2, "z", True)
        e7 = -z
        self.assertTrue(e7.is_integer())
        self.assertEqual(str(e7), "(-z)")
        not_z = ~z
        self.assertTrue(not_z.is_integer())
        self.assertEqual(str(not_z), "not(z)")
        self.assertEqual(not_z.index, -3)

        e8 = swig_helper.LinearExpr.sum([x, y, z])
        self.assertEqual(str(e8), "(x + y + z)")
        e9 = swig_helper.LinearExpr.sum([x, y, z], 11)
        self.assertEqual(str(e9), "(x + y + z + 11)")
        e10 = swig_helper.LinearExpr.weighted_sum([x, y, z], [1, 2, 3])
        self.assertEqual(str(e10), "(x + 2 * y + 3 * z)")
        e11 = swig_helper.LinearExpr.weighted_sum([x, y, z], [1, 2, 3], -5)
        self.assertEqual(str(e11), "(x + 2 * y + 3 * z - 5)")

    def testFloatLinExpr(self):
        x = TestIntVar(0, "x")
        self.assertTrue(x.is_integer())
        self.assertIsInstance(x, TestIntVar)
        self.assertIsInstance(x, swig_helper.LinearExpr)
        self.assertIsInstance(x, swig_helper.FloatLinearExpr)
        e1 = x + 2.5
        self.assertFalse(e1.is_integer())
        self.assertEqual(str(e1), "(x + 2.5)")
        e2 = 3.1 + x
        self.assertFalse(e2.is_integer())
        self.assertEqual(str(e2), "(x + 3.1)")
        y = TestIntVar(1, "y")
        e3 = y * 5.2
        self.assertFalse(e3.is_integer())
        self.assertEqual(str(e3), "(5.2 * y)")
        e4 = -2.2 * y
        self.assertFalse(e4.is_integer())
        self.assertEqual(str(e4), "(-2.2 * y)")
        e5 = x - 1.1
        self.assertFalse(e5.is_integer())
        self.assertEqual(str(e5), "(x - 1.1)")
        e6 = x + 2.4 * y
        self.assertFalse(e6.is_integer())
        self.assertEqual(str(e6), "(x + (2.4 * y))")
        e7 = x - 2.4 * y
        self.assertFalse(e7.is_integer())
        self.assertEqual(str(e7), "(x - (2.4 * y))")

        z = TestIntVar(2, "z")
        e8 = swig_helper.FloatLinearExpr.sum([x, y, z])
        self.assertFalse(e8.is_integer())
        self.assertEqual(str(e8), "(x + y + z)")
        e9 = swig_helper.FloatLinearExpr.sum([x, y, z], 1.5)
        self.assertFalse(e9.is_integer())
        self.assertEqual(str(e9), "(x + y + z + 1.5)")
        e10 = swig_helper.FloatLinearExpr.weighted_sum([x, y, z], [1.0, 2.2, 3.3])
        self.assertFalse(e10.is_integer())
        self.assertEqual(str(e10), "(x + 2.2 * y + 3.3 * z)")
        e11 = swig_helper.FloatLinearExpr.weighted_sum([x, y, z], [1.0, 2.2, 3.3], 1.5)
        self.assertFalse(e11.is_integer())
        self.assertEqual(str(e11), "(x + 2.2 * y + 3.3 * z + 1.5)")
        e12 = (x + 2) * 3.1
        self.assertFalse(e12.is_integer())
        self.assertEqual(str(e12), "(3.1 * (x + 2))")


if __name__ == "__main__":
    absltest.main()
