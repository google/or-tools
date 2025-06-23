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

from absl.testing import absltest
from ortools.math_opt import solution_pb2
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import model
from ortools.math_opt.python import solution
from ortools.math_opt.python.testing import compare_proto


class SolutionStatusTest(absltest.TestCase):

  def test_optional_status_round_trip(self):
    for status in solution_pb2.SolutionStatusProto.values():
      self.assertEqual(
          status,
          solution.optional_solution_status_to_proto(
              solution.parse_optional_solution_status(status)
          ),
      )


class ParsePrimalSolutionTest(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):

  def test_empty_primal_solution_proto_round_trip(self) -> None:
    mod = model.Model(name="test_model")
    empty_solution = solution.PrimalSolution(
        objective_value=2.0,
        feasibility_status=solution.SolutionStatus.UNDETERMINED,
    )
    empty_proto = empty_solution.to_proto()
    expected_proto = solution_pb2.PrimalSolutionProto()
    expected_proto.objective_value = 2.0
    expected_proto.feasibility_status = (
        solution_pb2.SOLUTION_STATUS_UNDETERMINED
    )
    self.assert_protos_equiv(expected_proto, empty_proto)
    round_trip_solution = solution.parse_primal_solution(empty_proto, mod)
    self.assertEmpty(round_trip_solution.variable_values)
    self.assertEmpty(round_trip_solution.auxiliary_objective_values)

  def test_primal_solution_proto_round_trip(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    mod.add_binary_variable(name="y")
    z = mod.add_binary_variable(name="z")
    a = mod.add_auxiliary_objective(priority=1)
    proto = solution_pb2.PrimalSolutionProto(
        objective_value=2.0,
        feasibility_status=solution_pb2.SOLUTION_STATUS_FEASIBLE,
    )
    proto.variable_values.ids[:] = [0, 2]
    proto.variable_values.values[:] = [1.0, 0.0]
    proto.auxiliary_objective_values[0] = 12.1
    actual = solution.parse_primal_solution(proto, mod)
    self.assertDictEqual({x: 1.0, z: 0.0}, actual.variable_values)
    self.assertEqual(2.0, actual.objective_value)
    self.assertEqual(
        solution.SolutionStatus.FEASIBLE, actual.feasibility_status
    )
    self.assertDictEqual(actual.auxiliary_objective_values, {a: 12.1})
    self.assert_protos_equiv(proto, actual.to_proto())

  def test_primal_solution_unspecified_feasibility(self) -> None:
    mod = model.Model(name="test_model")
    proto = solution_pb2.PrimalSolutionProto(
        objective_value=2.0,
        feasibility_status=solution_pb2.SOLUTION_STATUS_UNSPECIFIED,
    )
    with self.assertRaisesRegex(
        ValueError, "Primal solution feasibility.*UNSPECIFIED"
    ):
      solution.parse_primal_solution(proto, mod)

  def test_id_validation_variables(self) -> None:
    mod = model.Model(name="test_model")
    proto = solution_pb2.PrimalSolutionProto(
        objective_value=2.0,
        feasibility_status=solution_pb2.SOLUTION_STATUS_FEASIBLE,
    )
    proto.variable_values.ids[:] = [2]
    proto.variable_values.values[:] = [4.0]
    actual = solution.parse_primal_solution(proto, mod, validate=False)
    bad_var = mod.get_variable(2, validate=False)
    self.assertDictEqual(actual.variable_values, {bad_var: 4.0})
    with self.assertRaises(KeyError):
      solution.parse_primal_solution(proto, mod, validate=True)

  def test_id_validation_auxiliary_objectives(self) -> None:
    mod = model.Model(name="test_model")
    proto = solution_pb2.PrimalSolutionProto(
        objective_value=2.0,
        feasibility_status=solution_pb2.SOLUTION_STATUS_FEASIBLE,
    )
    proto.auxiliary_objective_values[2] = 12.1
    actual = solution.parse_primal_solution(proto, mod, validate=False)
    bad_obj = mod.get_auxiliary_objective(2, validate=False)
    self.assertDictEqual(actual.auxiliary_objective_values, {bad_obj: 12.1})
    with self.assertRaises(KeyError):
      solution.parse_primal_solution(proto, mod, validate=True)


class PrimalRayTest(compare_proto.MathOptProtoAssertions, absltest.TestCase):

  def test_proto_round_trip(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    ray = solution.PrimalRay(variable_values={x: 1.0, y: 1.0})
    ray_proto = solution_pb2.PrimalRayProto()
    ray_proto.variable_values.ids[:] = [0, 1]
    ray_proto.variable_values.values[:] = [1.0, 1.0]

    # Test proto -> model
    parsed_ray = solution.parse_primal_ray(ray_proto, mod)
    self.assertDictEqual({x: 1.0, y: 1.0}, parsed_ray.variable_values)

    # Test model -> proto
    exported_ray = ray.to_proto()
    self.assert_protos_equiv(exported_ray, ray_proto)

  def test_id_validation(self) -> None:
    mod = model.Model(name="test_model")
    proto = solution_pb2.PrimalRayProto()
    proto.variable_values.ids[:] = [2]
    proto.variable_values.values[:] = [4.0]
    actual = solution.parse_primal_ray(proto, mod, validate=False)
    bad_var = mod.get_variable(2, validate=False)
    self.assertDictEqual(actual.variable_values, {bad_var: 4.0})
    with self.assertRaises(KeyError):
      solution.parse_primal_ray(proto, mod, validate=True)


class ParseDualSolutionTest(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):

  def test_empty_primal_solution_proto_round_trip(self) -> None:
    mod = model.Model(name="test_model")
    empty_solution = solution.DualSolution(
        objective_value=2.0,
        feasibility_status=solution.SolutionStatus.UNDETERMINED,
    )
    empty_proto = empty_solution.to_proto()
    expected_proto = solution_pb2.DualSolutionProto()
    expected_proto.objective_value = 2.0
    expected_proto.feasibility_status = (
        solution_pb2.SOLUTION_STATUS_UNDETERMINED
    )
    self.assert_protos_equiv(expected_proto, empty_proto)
    round_trip_solution = solution.parse_dual_solution(empty_proto, mod)
    self.assertEmpty(round_trip_solution.dual_values)
    self.assertEmpty(round_trip_solution.reduced_costs)

  def test_no_obj(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    d = mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")
    e = mod.add_quadratic_constraint(name="e")
    f = mod.add_quadratic_constraint(name="f")
    proto = solution_pb2.DualSolutionProto()
    proto.dual_values.ids[:] = [0, 1]
    proto.dual_values.values[:] = [0.0, 1.0]
    proto.quadratic_dual_values.ids[:] = [0, 1]
    proto.quadratic_dual_values.values[:] = [100.0, 101.0]
    proto.reduced_costs.ids[:] = [0, 1]
    proto.reduced_costs.values[:] = [10.0, 0.0]
    proto.feasibility_status = solution_pb2.SOLUTION_STATUS_FEASIBLE
    actual = solution.parse_dual_solution(proto, mod)
    self.assertDictEqual({x: 10.0, y: 0.0}, actual.reduced_costs)
    self.assertDictEqual({c: 0.0, d: 1.0}, actual.dual_values)
    self.assertDictEqual({e: 100, f: 101}, actual.quadratic_dual_values)
    self.assertIsNone(actual.objective_value)
    self.assertEqual(
        solution.SolutionStatus.FEASIBLE, actual.feasibility_status
    )
    self.assert_protos_equiv(proto, actual.to_proto())

  def test_with_obj(self) -> None:
    mod = model.Model(name="test_model")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    proto = solution_pb2.DualSolutionProto(objective_value=3.0)
    proto.dual_values.ids[:] = [0]
    proto.dual_values.values[:] = [5.0]
    proto.feasibility_status = solution_pb2.SOLUTION_STATUS_INFEASIBLE
    actual = solution.parse_dual_solution(proto, mod)
    self.assertEmpty(actual.reduced_costs)
    self.assertDictEqual({c: 5.0}, actual.dual_values)
    self.assertEqual(3.0, actual.objective_value)
    self.assertEqual(
        solution.SolutionStatus.INFEASIBLE, actual.feasibility_status
    )
    self.assert_protos_equiv(proto, actual.to_proto())

  def test_dual_solution_unspecified_feasibility(self) -> None:
    mod = model.Model(name="test_model")
    proto = solution_pb2.DualSolutionProto(
        objective_value=2.0,
        feasibility_status=solution_pb2.SOLUTION_STATUS_UNSPECIFIED,
    )
    with self.assertRaisesRegex(
        ValueError, "Dual solution feasibility.*UNSPECIFIED"
    ):
      solution.parse_dual_solution(proto, mod)

  def test_id_validation_reduced_costs(self) -> None:
    mod = model.Model(name="test_model")
    proto = solution_pb2.DualSolutionProto(
        objective_value=2.0,
        feasibility_status=solution_pb2.SOLUTION_STATUS_FEASIBLE,
    )
    proto.reduced_costs.ids[:] = [2]
    proto.reduced_costs.values[:] = [4.0]
    actual = solution.parse_dual_solution(proto, mod, validate=False)
    bad_var = mod.get_variable(2, validate=False)
    self.assertDictEqual(actual.reduced_costs, {bad_var: 4.0})
    with self.assertRaises(KeyError):
      solution.parse_dual_solution(proto, mod, validate=True)

  def test_id_validation_dual_values(self) -> None:
    mod = model.Model(name="test_model")
    proto = solution_pb2.DualSolutionProto(
        objective_value=2.0,
        feasibility_status=solution_pb2.SOLUTION_STATUS_FEASIBLE,
    )
    proto.dual_values.ids[:] = [2]
    proto.dual_values.values[:] = [4.0]
    actual = solution.parse_dual_solution(proto, mod, validate=False)
    bad_lin_con = mod.get_linear_constraint(2, validate=False)
    self.assertDictEqual(actual.dual_values, {bad_lin_con: 4.0})
    with self.assertRaises(KeyError):
      solution.parse_dual_solution(proto, mod, validate=True)

  def test_id_validation_quadratic_dual_values(self) -> None:
    mod = model.Model(name="test_model")
    proto = solution_pb2.DualSolutionProto(
        objective_value=2.0,
        feasibility_status=solution_pb2.SOLUTION_STATUS_FEASIBLE,
    )
    proto.quadratic_dual_values.ids[:] = [2]
    proto.quadratic_dual_values.values[:] = [4.0]
    actual = solution.parse_dual_solution(proto, mod, validate=False)
    bad_quad_con = mod.get_quadratic_constraint(2, validate=False)
    self.assertDictEqual(actual.quadratic_dual_values, {bad_quad_con: 4.0})
    with self.assertRaises(KeyError):
      solution.parse_dual_solution(proto, mod, validate=True)


class DualRayTest(compare_proto.MathOptProtoAssertions, absltest.TestCase):

  def test_proto_round_trip(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    d = mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")

    dual_ray = solution.DualRay(
        dual_values={c: 0.0, d: 1.0}, reduced_costs={x: 10.0, y: 0.0}
    )

    dual_ray_proto = solution_pb2.DualRayProto()
    dual_ray_proto.dual_values.ids[:] = [0, 1]
    dual_ray_proto.dual_values.values[:] = [0.0, 1.0]
    dual_ray_proto.reduced_costs.ids[:] = [0, 1]
    dual_ray_proto.reduced_costs.values[:] = [10.0, 0.0]

    # Test proto -> dual ray
    parsed_ray = solution.parse_dual_ray(dual_ray_proto, mod)
    self.assertDictEqual(dual_ray.reduced_costs, parsed_ray.reduced_costs)
    self.assertDictEqual(dual_ray.dual_values, parsed_ray.dual_values)

    # Test dual ray -> proto
    exported_proto = dual_ray.to_proto()
    self.assert_protos_equiv(exported_proto, dual_ray_proto)

  def test_id_validation_reduced_costs(self) -> None:
    mod = model.Model(name="test_model")
    proto = solution_pb2.DualRayProto()
    proto.reduced_costs.ids[:] = [2]
    proto.reduced_costs.values[:] = [4.0]
    actual = solution.parse_dual_ray(proto, mod, validate=False)
    bad_var = mod.get_variable(2, validate=False)
    self.assertDictEqual(actual.reduced_costs, {bad_var: 4.0})
    with self.assertRaises(KeyError):
      solution.parse_dual_ray(proto, mod, validate=True)

  def test_id_validation_dual_values(self) -> None:
    mod = model.Model(name="test_model")
    proto = solution_pb2.DualRayProto()
    proto.dual_values.ids[:] = [2]
    proto.dual_values.values[:] = [4.0]
    actual = solution.parse_dual_ray(proto, mod, validate=False)
    bad_lin_con = mod.get_linear_constraint(2, validate=False)
    self.assertDictEqual(actual.dual_values, {bad_lin_con: 4.0})
    with self.assertRaises(KeyError):
      solution.parse_dual_ray(proto, mod, validate=True)


class BasisTest(compare_proto.MathOptProtoAssertions, absltest.TestCase):

  def test_empty_basis_proto_round_trip(self) -> None:
    mod = model.Model(name="test_model")
    empty_basis = solution.Basis()
    empty_proto = empty_basis.to_proto()
    expected_proto = solution_pb2.BasisProto()
    self.assert_protos_equiv(expected_proto, empty_proto)
    round_trip_basis = solution.parse_basis(empty_proto, mod)
    self.assertEmpty(round_trip_basis.constraint_status)
    self.assertEmpty(round_trip_basis.variable_status)
    self.assertIsNone(round_trip_basis.basic_dual_feasibility)

  def test_basis_proto_round_trip(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    d = mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")
    basis = solution.Basis()
    basis.variable_status[x] = solution.BasisStatus.AT_LOWER_BOUND
    basis.variable_status[y] = solution.BasisStatus.BASIC
    basis.constraint_status[c] = solution.BasisStatus.BASIC
    basis.constraint_status[d] = solution.BasisStatus.AT_UPPER_BOUND
    basis.basic_dual_feasibility = solution.SolutionStatus.FEASIBLE
    basis_proto = basis.to_proto()
    expected_proto = solution_pb2.BasisProto()
    expected_proto.constraint_status.ids[:] = [0, 1]
    expected_proto.constraint_status.values[:] = [
        solution_pb2.BASIS_STATUS_BASIC,
        solution_pb2.BASIS_STATUS_AT_UPPER_BOUND,
    ]
    expected_proto.variable_status.ids[:] = [0, 1]
    expected_proto.variable_status.values[:] = [
        solution_pb2.BASIS_STATUS_AT_LOWER_BOUND,
        solution_pb2.BASIS_STATUS_BASIC,
    ]
    expected_proto.basic_dual_feasibility = (
        solution_pb2.SOLUTION_STATUS_FEASIBLE
    )
    self.assert_protos_equiv(expected_proto, basis_proto)
    round_trip_basis = solution.parse_basis(basis_proto, mod)
    self.assertDictEqual(
        {c: solution.BasisStatus.BASIC, d: solution.BasisStatus.AT_UPPER_BOUND},
        round_trip_basis.constraint_status,
    )
    self.assertDictEqual(
        {x: solution.BasisStatus.AT_LOWER_BOUND, y: solution.BasisStatus.BASIC},
        round_trip_basis.variable_status,
    )

  def test_constraint_status_unspecified(self) -> None:
    mod = model.Model(name="test_model")
    mod.add_binary_variable(name="x")
    mod.add_binary_variable(name="y")
    mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")
    basis_proto = solution_pb2.BasisProto()
    basis_proto.constraint_status.ids[:] = [0, 1]
    basis_proto.constraint_status.values[:] = [
        solution_pb2.BASIS_STATUS_UNSPECIFIED,
        solution_pb2.BASIS_STATUS_AT_UPPER_BOUND,
    ]
    basis_proto.variable_status.ids[:] = [0, 1]
    basis_proto.variable_status.values[:] = [
        solution_pb2.BASIS_STATUS_AT_LOWER_BOUND,
        solution_pb2.BASIS_STATUS_BASIC,
    ]
    basis_proto.basic_dual_feasibility = solution_pb2.SOLUTION_STATUS_FEASIBLE
    with self.assertRaisesRegex(ValueError, "Constraint basis.*UNSPECIFIED"):
      solution.parse_basis(basis_proto, mod)

  def test_variable_status_unspecified(self) -> None:
    mod = model.Model(name="test_model")
    mod.add_binary_variable(name="x")
    mod.add_binary_variable(name="y")
    mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")
    basis_proto = solution_pb2.BasisProto()
    basis_proto.constraint_status.ids[:] = [0, 1]
    basis_proto.constraint_status.values[:] = [
        solution_pb2.BASIS_STATUS_BASIC,
        solution_pb2.BASIS_STATUS_AT_UPPER_BOUND,
    ]
    basis_proto.variable_status.ids[:] = [0, 1]
    basis_proto.variable_status.values[:] = [
        solution_pb2.BASIS_STATUS_UNSPECIFIED,
        solution_pb2.BASIS_STATUS_BASIC,
    ]
    basis_proto.basic_dual_feasibility = solution_pb2.SOLUTION_STATUS_FEASIBLE
    with self.assertRaisesRegex(ValueError, "Variable basis.*UNSPECIFIED"):
      solution.parse_basis(basis_proto, mod)

  def test_basic_dual_feasibility_unspecified(self) -> None:
    mod = model.Model(name="test_model")
    basis_proto = solution_pb2.BasisProto()
    basis = solution.parse_basis(basis_proto, mod)
    self.assertIsNone(basis.basic_dual_feasibility)

  def test_variable_id_validation(self) -> None:
    mod = model.Model(name="test_model")
    basis_proto = solution_pb2.BasisProto()
    basis_proto.variable_status.ids[:] = [2]
    basis_proto.variable_status.values[:] = [
        solution_pb2.BASIS_STATUS_BASIC,
    ]
    basis = solution.parse_basis(basis_proto, mod, validate=False)
    bad_var = mod.get_variable(2, validate=False)
    self.assertDictEqual(
        basis.variable_status, {bad_var: solution.BasisStatus.BASIC}
    )
    with self.assertRaises(KeyError):
      solution.parse_basis(basis_proto, mod, validate=True)

  def test_linear_constraint_id_validation(self) -> None:
    mod = model.Model(name="test_model")
    basis_proto = solution_pb2.BasisProto()
    basis_proto.constraint_status.ids[:] = [2]
    basis_proto.constraint_status.values[:] = [
        solution_pb2.BASIS_STATUS_BASIC,
    ]
    basis = solution.parse_basis(basis_proto, mod, validate=False)
    bad_con = mod.get_linear_constraint(2, validate=False)
    self.assertDictEqual(
        basis.constraint_status, {bad_con: solution.BasisStatus.BASIC}
    )
    with self.assertRaises(KeyError):
      solution.parse_basis(basis_proto, mod, validate=True)


class ParseSolutionTest(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):

  def test_solution_proto_round_trip(self) -> None:
    mod = model.Model(name="test_model")
    mod.add_variable()
    mod.add_variable()
    mod.add_linear_constraint()
    mod.add_linear_constraint()
    primal_solution = solution_pb2.PrimalSolutionProto(
        objective_value=2.0,
        feasibility_status=solution_pb2.SOLUTION_STATUS_INFEASIBLE,
    )
    primal_solution.variable_values.ids[:] = [0, 1]
    primal_solution.variable_values.values[:] = [1.0, 0.0]
    dual_solution = solution_pb2.DualSolutionProto(
        objective_value=2.0,
        feasibility_status=solution_pb2.SOLUTION_STATUS_FEASIBLE,
    )
    dual_solution.dual_values.ids[:] = [0, 1]
    dual_solution.dual_values.values[:] = [0.0, 1.0]
    dual_solution.reduced_costs.ids[:] = [0, 1]
    dual_solution.reduced_costs.values[:] = [10.0, 0.0]
    basis = solution_pb2.BasisProto()
    basis.constraint_status.ids[:] = [0, 1]
    basis.constraint_status.values[:] = [
        solution_pb2.BASIS_STATUS_BASIC,
        solution_pb2.BASIS_STATUS_AT_UPPER_BOUND,
    ]
    basis.variable_status.ids[:] = [0, 1]
    basis.variable_status.values[:] = [
        solution_pb2.BASIS_STATUS_AT_LOWER_BOUND,
        solution_pb2.BASIS_STATUS_BASIC,
    ]
    basis.basic_dual_feasibility = solution_pb2.SOLUTION_STATUS_FEASIBLE
    proto = solution_pb2.SolutionProto(
        primal_solution=primal_solution,
        dual_solution=dual_solution,
        basis=basis,
    )
    actual = solution.parse_solution(proto, mod)
    self.assert_protos_equiv(proto, actual.to_proto())

  def test_basis_id_validation(self) -> None:
    mod = model.Model(name="test_model")
    proto = solution_pb2.SolutionProto(
        basis=solution_pb2.BasisProto(
            constraint_status=solution_pb2.SparseBasisStatusVector(
                ids=[2], values=[solution_pb2.BASIS_STATUS_BASIC]
            )
        )
    )
    sol = solution.parse_solution(proto, mod, validate=False)
    bad_con = mod.get_linear_constraint(2, validate=False)
    # TODO: b/215588365 - make a local variable so pytype is happy
    basis = sol.basis
    self.assertIsNotNone(basis)
    self.assertDictEqual(
        basis.constraint_status, {bad_con: solution.BasisStatus.BASIC}
    )
    with self.assertRaises(KeyError):
      solution.parse_solution(proto, mod, validate=True)

  def test_primal_solution_id_validation(self) -> None:
    mod = model.Model(name="test_model")
    proto = solution_pb2.SolutionProto(
        primal_solution=solution_pb2.PrimalSolutionProto(
            variable_values=sparse_containers_pb2.SparseDoubleVectorProto(
                ids=[2], values=[4.0]
            ),
            feasibility_status=solution_pb2.SOLUTION_STATUS_FEASIBLE,
        )
    )
    sol = solution.parse_solution(proto, mod, validate=False)
    bad_var = mod.get_variable(2, validate=False)
    # TODO: b/215588365 - make a local variable so pytype is happy
    primal = sol.primal_solution
    self.assertIsNotNone(primal)
    self.assertDictEqual(primal.variable_values, {bad_var: 4.0})
    with self.assertRaises(KeyError):
      solution.parse_solution(proto, mod, validate=True)

  def test_dual_solution_id_validation(self) -> None:
    mod = model.Model(name="test_model")
    proto = solution_pb2.SolutionProto(
        dual_solution=solution_pb2.DualSolutionProto(
            reduced_costs=sparse_containers_pb2.SparseDoubleVectorProto(
                ids=[2], values=[4.0]
            ),
            feasibility_status=solution_pb2.SOLUTION_STATUS_FEASIBLE,
        )
    )
    sol = solution.parse_solution(proto, mod, validate=False)
    bad_var = mod.get_variable(2, validate=False)
    # TODO: b/215588365 - make a local variable so pytype is happy
    dual = sol.dual_solution
    self.assertIsNotNone(dual)
    self.assertDictEqual(dual.reduced_costs, {bad_var: 4.0})
    with self.assertRaises(KeyError):
      solution.parse_solution(proto, mod, validate=True)


if __name__ == "__main__":
  absltest.main()
