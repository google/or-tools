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

"""Tests for ortools.sat.python.swig_helper."""

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
        solution = solve_wrapper.solve(model)

        self.assertEqual(cp_model_pb2.OPTIMAL, solution.status)
        self.assertEqual(30.0, solution.objective_value)

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
        solution = solve_wrapper.solve(model)

        self.assertEqual(cp_model_pb2.OPTIMAL, solution.status)
        self.assertEqual(30.0, solution.objective_value)

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
        solution = solve_wrapper.solve(model)

        self.assertEqual(cp_model_pb2.OPTIMAL, solution.status)
        self.assertEqual(30.0, solution.objective_value)
        self.assertEqual(30.0, solution.best_objective_bound)

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
        solution = solve_wrapper.solve(model)

        self.assertEqual(5, callback.solution_count())
        self.assertEqual(cp_model_pb2.OPTIMAL, solution.status)

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


if __name__ == "__main__":
    absltest.main()
