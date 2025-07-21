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

"""Unit tests for ortools.sat.python.cmh."""

import sys

from absl.testing import absltest

from ortools.sat.python import cp_model_helper as cmh
from ortools.util.python import sorted_interval_list


class Callback(cmh.SolutionCallback):

    def __init__(self):
        cmh.SolutionCallback.__init__(self)
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


class CpModelHelperTest(absltest.TestCase):

    def tearDown(self) -> None:
        super().tearDown()
        sys.stdout.flush()

    def test_variable_domain(self):
        model_string = """
      variables { domain: [ -10, 10 ] }
      variables { domain: [ -5, -5, 3, 6 ] }
      """
        model = cmh.CpModelProto()
        self.assertTrue(model.parse_text_format(model_string))

        d0 = cmh.CpSatHelper.variable_domain(model.variables[0])
        d1 = cmh.CpSatHelper.variable_domain(model.variables[1])

        self.assertEqual(d0.flattened_intervals(), [-10, 10])
        self.assertEqual(d1.flattened_intervals(), [-5, -5, 3, 6])

    def test_simple_solve(self):
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
        vars: 2
        coeffs: -1
        scaling_factor: -1
      }"""
        model = cmh.CpModelProto()
        self.assertTrue(model.parse_text_format(model_string))

        solve_wrapper = cmh.SolveWrapper()
        response_wrapper = solve_wrapper.solve_and_return_response_wrapper(model)

        self.assertEqual(cmh.OPTIMAL, response_wrapper.status())
        self.assertEqual(30.0, response_wrapper.objective_value())

    def test_simple_solve_with_core(self):
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
        vars: 2
        coeffs: -1
        scaling_factor: -1
      }"""
        model = cmh.CpModelProto()
        self.assertTrue(model.parse_text_format(model_string))

        parameters = cmh.SatParameters()
        parameters.optimize_with_core = True

        solve_wrapper = cmh.SolveWrapper()
        solve_wrapper.set_parameters(parameters)
        response_wrapper = solve_wrapper.solve_and_return_response_wrapper(model)

        self.assertEqual(cmh.OPTIMAL, response_wrapper.status())
        self.assertEqual(30.0, response_wrapper.objective_value())

    def test_simple_solve_with_proto_api(self):
        model = cmh.CpModelProto()
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
        model.objective.vars.append(2)
        model.objective.coeffs.append(-1)
        model.objective.scaling_factor = -1

        solve_wrapper = cmh.SolveWrapper()
        response_wrapper = solve_wrapper.solve_and_return_response_wrapper(model)

        self.assertEqual(cmh.OPTIMAL, response_wrapper.status())
        self.assertEqual(30.0, response_wrapper.objective_value())
        self.assertEqual(30.0, response_wrapper.best_objective_bound())
        self.assertRaises(TypeError, response_wrapper.value, None)
        self.assertRaises(TypeError, response_wrapper.float_value, None)
        self.assertRaises(TypeError, response_wrapper.boolean_value, None)

    def test_solution_callback(self):
        model_string = """
      variables { domain: 0 domain: 5 }
      variables { domain: 0 domain: 5 }
      constraints {
        linear { vars: 0 vars: 1 coeffs: 1 coeffs: 1 domain: 6 domain: 6 } }
      """
        model = cmh.CpModelProto()
        self.assertTrue(model.parse_text_format(model_string))

        solve_wrapper = cmh.SolveWrapper()
        callback = Callback()
        solve_wrapper.add_solution_callback(callback)
        params = cmh.SatParameters()
        params.enumerate_all_solutions = True
        solve_wrapper.set_parameters(params)
        response_wrapper = solve_wrapper.solve_and_return_response_wrapper(model)

        self.assertEqual(5, callback.solution_count())
        self.assertEqual(cmh.OPTIMAL, response_wrapper.status())

    def test_best_bound_callback(self):
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
        model = cmh.CpModelProto()
        self.assertTrue(model.parse_text_format(model_string))

        solve_wrapper = cmh.SolveWrapper()
        best_bound_callback = BestBoundCallback()
        solve_wrapper.add_best_bound_callback(best_bound_callback.new_best_bound)
        params = cmh.SatParameters()
        params.num_workers = 1
        params.linearization_level = 2
        params.log_search_progress = True
        solve_wrapper.set_parameters(params)
        response_wrapper = solve_wrapper.solve_and_return_response_wrapper(model)

        self.assertEqual(2.6, best_bound_callback.best_bound)
        self.assertEqual(cmh.OPTIMAL, response_wrapper.status())

    def test_model_stats(self):
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
        vars: 2
        coeffs: -1
        scaling_factor: -1
      }
      name: 'testModelStats'
      """
        model = cmh.CpModelProto()
        self.assertTrue(model.parse_text_format(model_string))
        stats = cmh.CpSatHelper.model_stats(model)
        self.assertTrue(stats)

    def test_int_lin_expr(self):
        model = cmh.CpModelProto()
        x = cmh.IntVar(model).with_name("x")
        self.assertTrue(x.is_integer())
        self.assertIsInstance(x, cmh.IntVar)
        self.assertIsInstance(x, cmh.LinearExpr)
        e1 = x + 2
        self.assertTrue(e1.is_integer())
        self.assertEqual(str(e1), "(x + 2)")
        e2 = 3 + x
        self.assertTrue(e2.is_integer())
        self.assertEqual(str(e2), "(x + 3)")
        y = cmh.IntVar(model).with_name("y")
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
        self.assertEqual(str(e6), "(x + (-2 * y))")
        z = cmh.IntVar(model).with_name("z")
        z.domain = sorted_interval_list.Domain.from_values([0, 1])
        e7 = -z
        self.assertTrue(e7.is_integer())
        self.assertEqual(str(e7), "(-z)")
        not_z = ~z
        self.assertTrue(not_z.is_integer())
        self.assertEqual(str(not_z), "not(z)")
        self.assertEqual(not_z.index, -3)

        e8 = cmh.LinearExpr.sum([x, y, z])
        self.assertEqual(str(e8), "(x + y + z)")
        e9 = cmh.LinearExpr.sum([x, y, z, 11])
        self.assertEqual(str(e9), "(x + y + z + 11)")
        e10 = cmh.LinearExpr.weighted_sum([x, y, z], [1, 2, 3])
        self.assertEqual(str(e10), "(x + 2 * y + 3 * z)")
        e11 = cmh.LinearExpr.weighted_sum([x, y, z, 5], [1, 2, 3, -1])
        self.assertEqual(str(e11), "(x + 2 * y + 3 * z - 5)")

        e12 = x - y - 2 * z
        self.assertEqual(str(e12), "(x + (-y) + (-2 * z))")

    def test_float_lin_expr(self):
        model = cmh.CpModelProto()
        x = cmh.IntVar(model).with_name("x")
        self.assertTrue(x.is_integer())
        self.assertIsInstance(x, cmh.IntVar)
        self.assertIsInstance(x, cmh.LinearExpr)
        e1 = x + 2.5
        self.assertFalse(e1.is_integer())
        self.assertEqual(str(e1), "(x + 2.5)")
        e2 = 3.1 + x
        self.assertFalse(e2.is_integer())
        self.assertEqual(str(e2), "(x + 3.1)")
        y = cmh.IntVar(model).with_name("y")
        e3 = y * 5.2
        self.assertFalse(e3.is_integer())
        self.assertEqual(str(e3), "(5.2 * y)")
        e4 = -2.25 * y
        self.assertFalse(e4.is_integer())
        self.assertEqual(str(e4), "(-2.25 * y)")
        e5 = x - 1.1
        self.assertFalse(e5.is_integer())
        self.assertEqual(str(e5), "(x - 1.1)")
        e6 = x + 2.4 * y
        self.assertFalse(e6.is_integer())
        self.assertEqual(str(e6), "(x + (2.4 * y))")
        e7 = x - 2.4 * y
        self.assertFalse(e7.is_integer())
        self.assertEqual(str(e7), "(x + (-(2.4 * y)))")

        z = cmh.IntVar(model).with_name("z")
        e8 = cmh.LinearExpr.sum([x, y, z, -2])
        self.assertTrue(e8.is_integer())
        self.assertEqual(str(e8), "(x + y + z - 2)")
        e9 = cmh.LinearExpr.sum([x, y, z, 1.5])
        self.assertFalse(e9.is_integer())
        self.assertEqual(str(e9), "(x + y + z + 1.5)")
        e10 = cmh.LinearExpr.weighted_sum([x, y, z], [1.0, 2.25, 5.5])
        self.assertFalse(e10.is_integer())
        self.assertEqual(str(e10), "(x + 2.25 * y + 5.5 * z)")
        e11 = cmh.LinearExpr.weighted_sum([x, y, z, 1.5], [1.0, 2.25, 5.5, -1])
        self.assertFalse(e11.is_integer())
        self.assertEqual(str(e11), "(x + 2.25 * y + 5.5 * z - 1.5)")
        e12 = (x + 2) * 3.1
        self.assertFalse(e12.is_integer())
        self.assertEqual(str(e12), "(3.1 * (x + 2))")


class CpModelBuilderTest(absltest.TestCase):

    def test_basic(self):
        model_proto = cmh.CpModelProto()

        # Singular message.
        objective = model_proto.objective

        # Singular int.
        self.assertEqual(objective.offset, 0)
        objective.offset = 123
        self.assertEqual(objective.offset, 123)

        # Set a message.
        new_obj = cmh.CpObjectiveProto()
        new_obj.offset = 456
        model_proto.objective = new_obj
        self.assertEqual(objective.offset, 456)

        # Large int.
        objective.offset = 500000000000
        self.assertEqual(objective.offset, 500000000000)

        # Repeated message.
        my_var = model_proto.variables.add()

        # Singular string.
        self.assertEqual(my_var.name, "")
        my_var.name = "my_var"
        self.assertEqual(my_var.name, "my_var")
        my_var.domain.extend([0, 1])
        domain = list(my_var.domain)
        self.assertLen(domain, 2)
        self.assertEqual(domain[0], 0)
        self.assertEqual(domain[1], 1)

        # Repeated int.
        objective.vars.append(0)
        self.assertLen(objective.vars, 1)
        self.assertEqual(objective.vars[0], 0)
        objective.vars[0] = 42
        self.assertEqual(objective.vars[0], 42)

        # Singular enum
        search_strategy = model_proto.search_strategy.add()
        self.assertEqual(
            search_strategy.variable_selection_strategy,
            cmh.DecisionStrategyProto.CHOOSE_FIRST,
        )
        search_strategy.variable_selection_strategy = (
            cmh.DecisionStrategyProto.CHOOSE_LOWEST_MIN
        )
        self.assertEqual(
            search_strategy.variable_selection_strategy,
            cmh.DecisionStrategyProto.CHOOSE_LOWEST_MIN,
        )


class SatParametersBuilderTest(absltest.TestCase):

    def test_basic_api(self) -> None:
        params = cmh.SatParameters()

        # Test that we can set and get an integer parameter.
        params.num_workers = 10
        self.assertEqual(params.num_workers, 10)

        # Test that we can set and get an enum parameter.
        self.assertEqual(
            params.clause_cleanup_ordering,
            cmh.SatParameters.ClauseOrdering.CLAUSE_ACTIVITY,
        )
        params.clause_cleanup_ordering = cmh.SatParameters.ClauseOrdering.CLAUSE_LBD
        self.assertEqual(
            params.clause_cleanup_ordering,
            cmh.SatParameters.ClauseOrdering.CLAUSE_LBD,
        )

        # Test that we can set and get a repeated string parameter.
        params.subsolvers.append("no_lp")
        self.assertLen(params.subsolvers, 1)
        self.assertEqual(params.subsolvers[0], "no_lp")


if __name__ == "__main__":
    absltest.main()
