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

"""Tests that the primary and auxiliary objectives are correct for model.py."""

from typing import Dict, Tuple

from absl.testing import absltest
from ortools.math_opt import model_pb2
from ortools.math_opt import model_update_pb2
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import model
from ortools.math_opt.python import objectives
from ortools.math_opt.python import variables
from ortools.math_opt.python.testing import compare_proto


def _lin_terms(obj: objectives.Objective) -> Dict[variables.Variable, float]:
    return {term.variable: term.coefficient for term in obj.linear_terms()}


def _quad_terms(
    obj: objectives.Objective,
) -> Dict[Tuple[variables.Variable, variables.Variable], float]:
    return {
        (term.key.first_var, term.key.second_var): term.coefficient
        for term in obj.quadratic_terms()
    }


class ModelSetObjectiveTest(absltest.TestCase):

    def test_maximize(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        mod.objective.set_linear_coefficient(x, 10.0)
        mod.objective.set_linear_coefficient(y, 11.0)

        mod.maximize(3 * x * x + 2 * x + 1)

        self.assertTrue(mod.objective.is_maximize)
        self.assertEqual(mod.objective.offset, 1.0)
        self.assertDictEqual(_lin_terms(mod.objective), {x: 2.0})
        self.assertDictEqual(_quad_terms(mod.objective), {(x, x): 3.0})

    def test_maximize_linear_obj(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        mod.objective.set_linear_coefficient(x, 10.0)
        mod.objective.set_linear_coefficient(y, 11.0)

        mod.maximize_linear_objective(2 * x + 1)

        self.assertTrue(mod.objective.is_maximize)
        self.assertEqual(mod.objective.offset, 1.0)
        self.assertDictEqual(_lin_terms(mod.objective), {x: 2.0})
        self.assertEmpty(_quad_terms(mod.objective))

    def test_maximize_linear_obj_type_error_quadratic(self) -> None:
        mod = model.Model()
        x = mod.add_variable()

        with self.assertRaisesRegex(TypeError, "Quadratic"):
            mod.maximize_linear_objective(x * x)

    def test_maximize_quadratic_objective(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        mod.objective.set_linear_coefficient(x, 10.0)
        mod.objective.set_linear_coefficient(y, 11.0)

        mod.maximize_quadratic_objective(3 * x * x + 2 * x + 1)

        self.assertTrue(mod.objective.is_maximize)
        self.assertEqual(mod.objective.offset, 1.0)
        self.assertDictEqual(_lin_terms(mod.objective), {x: 2.0})
        self.assertDictEqual(_quad_terms(mod.objective), {(x, x): 3.0})

    def test_minimize(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        mod.objective.set_linear_coefficient(x, 10.0)
        mod.objective.set_linear_coefficient(y, 11.0)
        mod.objective.is_maximize = True

        mod.minimize(3 * x * x + 2 * x + 1)

        self.assertFalse(mod.objective.is_maximize)
        self.assertEqual(mod.objective.offset, 1.0)
        self.assertDictEqual(_lin_terms(mod.objective), {x: 2.0})
        self.assertDictEqual(_quad_terms(mod.objective), {(x, x): 3.0})

    def test_minimize_linear_obj(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        mod.objective.set_linear_coefficient(x, 10.0)
        mod.objective.set_linear_coefficient(y, 11.0)
        mod.objective.is_maximize = True

        mod.minimize_linear_objective(2 * x + 1)

        self.assertFalse(mod.objective.is_maximize)
        self.assertEqual(mod.objective.offset, 1.0)
        self.assertDictEqual(_lin_terms(mod.objective), {x: 2.0})
        self.assertEmpty(_quad_terms(mod.objective))

    def test_minimize_linear_obj_type_error_quadratic(self) -> None:
        mod = model.Model()
        x = mod.add_variable()

        with self.assertRaisesRegex(TypeError, "Quadratic"):
            mod.minimize_linear_objective(x * x)

    def test_minimize_quadratic_objective(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        mod.objective.set_linear_coefficient(x, 10.0)
        mod.objective.set_linear_coefficient(y, 11.0)
        mod.objective.is_maximize = True

        mod.minimize_quadratic_objective(3 * x * x + 2 * x + 1)

        self.assertFalse(mod.objective.is_maximize)
        self.assertEqual(mod.objective.offset, 1.0)
        self.assertDictEqual(_lin_terms(mod.objective), {x: 2.0})
        self.assertDictEqual(_quad_terms(mod.objective), {(x, x): 3.0})

    def test_set_objective(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        mod.objective.set_linear_coefficient(x, 10.0)
        mod.objective.set_linear_coefficient(y, 11.0)

        mod.set_objective(3 * x * x + 2 * x + 1, is_maximize=True)

        self.assertTrue(mod.objective.is_maximize)
        self.assertEqual(mod.objective.offset, 1.0)
        self.assertDictEqual(_lin_terms(mod.objective), {x: 2.0})
        self.assertDictEqual(_quad_terms(mod.objective), {(x, x): 3.0})

    def test_set_objective_linear_obj(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        mod.objective.set_linear_coefficient(x, 10.0)
        mod.objective.set_linear_coefficient(y, 11.0)
        mod.objective.is_maximize = True

        mod.set_linear_objective(2 * x + 1, is_maximize=False)

        self.assertFalse(mod.objective.is_maximize)
        self.assertEqual(mod.objective.offset, 1.0)
        self.assertDictEqual(_lin_terms(mod.objective), {x: 2.0})
        self.assertEmpty(_quad_terms(mod.objective))

    def test_set_objective_linear_obj_type_error_quadratic(self) -> None:
        mod = model.Model()
        x = mod.add_variable()

        with self.assertRaisesRegex(TypeError, "Quadratic"):
            mod.set_linear_objective(x * x, is_maximize=True)

    def test_set_objective_quadratic_objective(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        mod.objective.set_linear_coefficient(x, 10.0)
        mod.objective.set_linear_coefficient(y, 11.0)

        mod.set_quadratic_objective(3 * x * x + 2 * x + 1, is_maximize=True)

        self.assertTrue(mod.objective.is_maximize)
        self.assertEqual(mod.objective.offset, 1.0)
        self.assertDictEqual(_lin_terms(mod.objective), {x: 2.0})
        self.assertDictEqual(_quad_terms(mod.objective), {(x, x): 3.0})


class ModelAuxObjTest(absltest.TestCase):

    def test_add_aux_obj_with_expr(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        aux = mod.add_auxiliary_objective(priority=10, expr=3.0 * x + 4.0, name="aux")

        self.assertEqual(aux.offset, 4.0)
        self.assertDictEqual(_lin_terms(aux), {x: 3.0})
        self.assertFalse(aux.is_maximize)
        self.assertEqual(aux.priority, 10)
        self.assertEqual(aux.name, "aux")

    def test_add_aux_obj_with_maximize(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        aux = mod.add_maximization_objective(3.0 * x + 4.0, priority=10, name="aux")

        self.assertEqual(aux.offset, 4.0)
        self.assertDictEqual(_lin_terms(aux), {x: 3.0})
        self.assertTrue(aux.is_maximize)
        self.assertEqual(aux.priority, 10)
        self.assertEqual(aux.name, "aux")

    def test_add_aux_obj_with_minimize(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        aux = mod.add_minimization_objective(3.0 * x + 4.0, priority=10, name="aux")

        self.assertEqual(aux.offset, 4.0)
        self.assertDictEqual(_lin_terms(aux), {x: 3.0})
        self.assertFalse(aux.is_maximize)
        self.assertEqual(aux.priority, 10)
        self.assertEqual(aux.name, "aux")


_PROTO_VARS = model_pb2.VariablesProto(
    ids=[0],
    lower_bounds=[-2.0],
    upper_bounds=[2.0],
    integers=[False],
    names=[""],
)


class ModelObjectiveExportProtoIntegrationTest(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):
    """Test Model.export_model() and UpdateTracker.export_update() for objectives.

    These tests are not comprehensive, the proto generation code is completely
    tested in the tests for Elemental. We just want to make sure everything is
    connected.
    """

    def test_export_model_with_objective(self) -> None:
        mod = model.Model(primary_objective_name="obj-A")
        x = mod.add_variable(lb=-2.0, ub=2.0)
        mod.objective.priority = 3
        mod.maximize(3 * x * x + 4 * x + 5)
        proto_obj = model_pb2.ObjectiveProto(
            maximize=True,
            name="obj-A",
            priority=3,
            offset=5.0,
            linear_coefficients=sparse_containers_pb2.SparseDoubleVectorProto(
                ids=[0], values=[4.0]
            ),
            quadratic_coefficients=sparse_containers_pb2.SparseDoubleMatrixProto(
                row_ids=[0], column_ids=[0], coefficients=[3.0]
            ),
        )
        expected = model_pb2.ModelProto(variables=_PROTO_VARS, objective=proto_obj)

        self.assert_protos_equal(mod.export_model(), expected)

    def test_export_model_update_with_objective(self) -> None:
        mod = model.Model(primary_objective_name="obj-A")
        x = mod.add_variable(lb=-2.0, ub=2.0)
        tracker = mod.add_update_tracker()
        mod.objective.set_linear_coefficient(x, 2.5)

        expected = model_update_pb2.ModelUpdateProto(
            objective_updates=model_update_pb2.ObjectiveUpdatesProto(
                linear_coefficients=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0], values=[2.5]
                )
            )
        )

        self.assert_protos_equal(tracker.export_update(), expected)

    def test_export_model_with_auxiliary_objective(self) -> None:
        mod = model.Model()
        x = mod.add_variable(lb=-2.0, ub=2.0)
        aux = mod.add_auxiliary_objective(priority=3, name="obj-A")
        aux.set_to_expression(4 * x + 5)
        aux.is_maximize = True

        proto_obj = model_pb2.ObjectiveProto(
            maximize=True,
            name="obj-A",
            priority=3,
            offset=5.0,
            linear_coefficients=sparse_containers_pb2.SparseDoubleVectorProto(
                ids=[0], values=[4.0]
            ),
        )

        expected = model_pb2.ModelProto(
            variables=_PROTO_VARS, auxiliary_objectives={0: proto_obj}
        )

        self.assert_protos_equal(mod.export_model(), expected)

    def test_export_model_update_with_aux_obj_update(self) -> None:
        mod = model.Model(primary_objective_name="obj-A")
        x = mod.add_variable(lb=-2.0, ub=2.0)
        aux = mod.add_auxiliary_objective(priority=20)
        tracker = mod.add_update_tracker()
        aux.set_linear_coefficient(x, 2.5)

        expected = model_update_pb2.ModelUpdateProto(
            auxiliary_objectives_updates=model_update_pb2.AuxiliaryObjectivesUpdatesProto(
                objective_updates={
                    0: model_update_pb2.ObjectiveUpdatesProto(
                        linear_coefficients=sparse_containers_pb2.SparseDoubleVectorProto(
                            ids=[0], values=[2.5]
                        )
                    )
                }
            )
        )

        self.assert_protos_equal(tracker.export_update(), expected)


if __name__ == "__main__":
    absltest.main()
