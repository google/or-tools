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
from absl.testing import parameterized
from ortools.math_opt import model_pb2
from ortools.math_opt import model_update_pb2
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import hash_model_storage
from ortools.math_opt.python import model_storage
from ortools.math_opt.python.testing import compare_proto

_StorageClass = model_storage.ModelStorageImplClass

_ModelUpdateProto = model_update_pb2.ModelUpdateProto
_VariableUpdatesProto = model_update_pb2.VariableUpdatesProto
_LinearConstraintUpdatesProto = model_update_pb2.LinearConstraintUpdatesProto
_SparseDoubleVectorProto = sparse_containers_pb2.SparseDoubleVectorProto
_SparseBoolVectorProto = sparse_containers_pb2.SparseBoolVectorProto
_SparseDoubleMatrixProto = sparse_containers_pb2.SparseDoubleMatrixProto
_VariablesProto = model_pb2.VariablesProto
_LinearConstraintsProto = model_pb2.LinearConstraintsProto
_ObjectiveUpdatesProto = model_update_pb2.ObjectiveUpdatesProto


@parameterized.parameters((hash_model_storage.HashModelStorage,))
class ModelStorageTest(
    compare_proto.MathOptProtoAssertions, parameterized.TestCase
):

  def test_simple_delete_var(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    tracker.advance_checkpoint()
    storage.delete_variable(x)
    self.assert_protos_equiv(
        _ModelUpdateProto(deleted_variable_ids=[0]), tracker.export_update()
    )

  def test_simple_delete_lin_con(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    tracker.advance_checkpoint()
    storage.delete_linear_constraint(c)
    self.assert_protos_equiv(
        _ModelUpdateProto(deleted_linear_constraint_ids=[0]),
        tracker.export_update(),
    )

  def test_update_var_lb(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    tracker.advance_checkpoint()
    storage.set_variable_lb(x, -7.0)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            variable_updates=_VariableUpdatesProto(
                lower_bounds=_SparseDoubleVectorProto(ids=[0], values=[-7.0])
            )
        ),
        tracker.export_update(),
    )

  def test_update_var_lb_same_value(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    tracker.advance_checkpoint()
    storage.set_variable_lb(x, -1.0)
    self.assertIsNone(tracker.export_update())

  def test_update_var_ub(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    tracker.advance_checkpoint()
    storage.set_variable_ub(x, 12.5)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            variable_updates=_VariableUpdatesProto(
                upper_bounds=_SparseDoubleVectorProto(ids=[0], values=[12.5])
            )
        ),
        tracker.export_update(),
    )

  def test_update_var_ub_same_value(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    tracker.advance_checkpoint()
    storage.set_variable_ub(x, 2.5)
    self.assertIsNone(tracker.export_update())

  def test_update_var_integer(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    tracker.advance_checkpoint()
    storage.set_variable_is_integer(x, False)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            variable_updates=_VariableUpdatesProto(
                integers=_SparseBoolVectorProto(ids=[0], values=[False])
            )
        ),
        tracker.export_update(),
    )

  def test_update_var_integer_same_value(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    tracker.advance_checkpoint()
    storage.set_variable_is_integer(x, True)
    self.assertIsNone(tracker.export_update())

  def test_update_var_then_delete(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    tracker.advance_checkpoint()
    storage.set_variable_lb(x, -3.0)
    storage.set_variable_ub(x, 5.0)
    storage.set_variable_is_integer(x, False)
    storage.delete_variable(x)
    self.assert_protos_equiv(
        _ModelUpdateProto(deleted_variable_ids=[0]), tracker.export_update()
    )

  def test_update_lin_con_lb(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    tracker.advance_checkpoint()
    storage.set_linear_constraint_lb(c, -7.0)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            linear_constraint_updates=_LinearConstraintUpdatesProto(
                lower_bounds=_SparseDoubleVectorProto(ids=[0], values=[-7.0])
            )
        ),
        tracker.export_update(),
    )

  def test_update_lin_con_lb_same_value(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    tracker.advance_checkpoint()
    storage.set_linear_constraint_lb(c, -1.0)
    self.assertIsNone(tracker.export_update())

  def test_update_lin_con_ub(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    tracker.advance_checkpoint()
    storage.set_linear_constraint_ub(c, 12.5)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            linear_constraint_updates=_LinearConstraintUpdatesProto(
                upper_bounds=_SparseDoubleVectorProto(ids=[0], values=[12.5])
            )
        ),
        tracker.export_update(),
    )

  def test_update_lin_con_ub_same_value(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    tracker.advance_checkpoint()
    storage.set_linear_constraint_ub(c, 2.5)
    self.assertIsNone(tracker.export_update())

  def test_update_lin_con_then_delete(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    tracker.advance_checkpoint()
    storage.set_linear_constraint_lb(c, -3.0)
    storage.set_linear_constraint_ub(c, 5.0)
    storage.delete_linear_constraint(c)
    self.assert_protos_equiv(
        _ModelUpdateProto(deleted_linear_constraint_ids=[0]),
        tracker.export_update(),
    )

  def test_new_var(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    storage.add_variable(-1.0, 2.5, True, "x")
    expected = _ModelUpdateProto(
        new_variables=_VariablesProto(
            ids=[0],
            lower_bounds=[-1.0],
            upper_bounds=[2.5],
            integers=[True],
            names=["x"],
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_modify_new_var(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    storage.set_variable_lb(x, -4.0)
    storage.set_variable_ub(x, 5.0)
    storage.set_variable_is_integer(x, False)
    expected = _ModelUpdateProto(
        new_variables=_VariablesProto(
            ids=[0],
            lower_bounds=[-4.0],
            upper_bounds=[5.0],
            integers=[False],
            names=["x"],
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_new_var_with_deletes(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(0.0, 1.0, False, "x")
    storage.add_variable(-1.0, 2.5, True, "y")
    storage.delete_variable(x)
    expected = _ModelUpdateProto(
        new_variables=_VariablesProto(
            ids=[1],
            lower_bounds=[-1.0],
            upper_bounds=[2.5],
            integers=[True],
            names=["y"],
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_delete_var_before_first_update(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    tracker.advance_checkpoint()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    storage.add_variable(-2.0, 3.5, True, "y")
    storage.delete_variable(x)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            new_variables=_VariablesProto(
                ids=[1],
                lower_bounds=[-2.0],
                upper_bounds=[3.5],
                integers=[True],
                names=["y"],
            )
        ),
        tracker.export_update(),
    )

  def test_new_lin_con(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    storage.add_linear_constraint(-1.0, 2.5, "c")
    expected = _ModelUpdateProto(
        new_linear_constraints=_LinearConstraintsProto(
            ids=[0], lower_bounds=[-1.0], upper_bounds=[2.5], names=["c"]
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_modify_new_lin_con(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    storage.set_linear_constraint_lb(c, -4.0)
    storage.set_linear_constraint_ub(c, 5.0)
    expected = _ModelUpdateProto(
        new_linear_constraints=_LinearConstraintsProto(
            ids=[0], lower_bounds=[-4.0], upper_bounds=[5.0], names=["c"]
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_new_lin_con_with_deletes(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    c = storage.add_linear_constraint(0.0, 1.0, "c")
    storage.add_linear_constraint(-1.0, 2.5, "d")
    storage.delete_linear_constraint(c)
    expected = _ModelUpdateProto(
        new_linear_constraints=_LinearConstraintsProto(
            ids=[1], lower_bounds=[-1.0], upper_bounds=[2.5], names=["d"]
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_delete_lin_con_before_first_update(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    tracker.advance_checkpoint()
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    storage.add_linear_constraint(-2.0, 3.5, "d")
    storage.delete_linear_constraint(c)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            new_linear_constraints=_LinearConstraintsProto(
                ids=[1], lower_bounds=[-2.0], upper_bounds=[3.5], names=["d"]
            )
        ),
        tracker.export_update(),
    )

  def test_update_objective_direction(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    storage.set_is_maximize(True)
    expected = _ModelUpdateProto(
        objective_updates=_ObjectiveUpdatesProto(direction_update=True)
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_update_objective_direction_same(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    storage.set_is_maximize(False)
    self.assertIsNone(tracker.export_update())

  def test_update_objective_offset(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    storage.set_objective_offset(5.0)
    expected = _ModelUpdateProto(
        objective_updates=_ObjectiveUpdatesProto(offset_update=5.0)
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_update_objective_offset_same(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    storage.set_objective_offset(0.0)
    self.assertIsNone(tracker.export_update())

  def test_objective_update_existing_zero(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    tracker.advance_checkpoint()
    storage.set_linear_objective_coefficient(x, 3.0)
    expected = _ModelUpdateProto(
        objective_updates=_ObjectiveUpdatesProto(
            linear_coefficients=_SparseDoubleVectorProto(ids=[0], values=[3.0])
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_objective_update_existing_zero_same(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    tracker.advance_checkpoint()
    storage.set_linear_objective_coefficient(x, 0.0)
    self.assertIsNone(tracker.export_update())

  def test_objective_update_existing_nonzero(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    storage.set_linear_objective_coefficient(x, 4.0)
    tracker.advance_checkpoint()
    storage.set_linear_objective_coefficient(x, 3.0)
    expected = _ModelUpdateProto(
        objective_updates=_ObjectiveUpdatesProto(
            linear_coefficients=_SparseDoubleVectorProto(ids=[0], values=[3.0])
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_objective_update_existing_nonzero_same(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    storage.set_linear_objective_coefficient(x, 4.0)
    tracker.advance_checkpoint()
    storage.set_linear_objective_coefficient(x, 4.0)
    self.assertIsNone(tracker.export_update())

  def test_objective_update_clear(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    y = storage.add_variable(0.0, 1.0, False, "y")
    z = storage.add_variable(0.0, 1.0, True, "z")

    storage.set_linear_objective_coefficient(x, 2.0)
    storage.set_linear_objective_coefficient(z, -5.5)
    storage.set_objective_offset(1.0)
    self.assertEqual(2.0, storage.get_linear_objective_coefficient(x))
    self.assertEqual(0.0, storage.get_linear_objective_coefficient(y))
    self.assertEqual(-5.5, storage.get_linear_objective_coefficient(z))
    self.assertEqual(1.0, storage.get_objective_offset())
    tracker.advance_checkpoint()
    w = storage.add_variable(0.0, 1.0, True, "w")
    storage.set_linear_objective_coefficient(w, 1.0)
    storage.clear_objective()
    expected = _ModelUpdateProto(
        new_variables=_VariablesProto(
            ids=[3],
            lower_bounds=[0.0],
            upper_bounds=[1.0],
            integers=[True],
            names=["w"],
        ),
        objective_updates=_ObjectiveUpdatesProto(
            offset_update=0.0,
            linear_coefficients=_SparseDoubleVectorProto(
                ids=[x, z], values=[0.0, 0.0]
            ),
        ),
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_objective_update_existing_to_zero(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    storage.set_linear_objective_coefficient(x, 4.0)
    tracker.advance_checkpoint()
    storage.set_linear_objective_coefficient(x, 0.0)
    expected = _ModelUpdateProto(
        objective_updates=_ObjectiveUpdatesProto(
            linear_coefficients=_SparseDoubleVectorProto(ids=[0], values=[0.0])
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_objective_update_existing_then_delete(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    storage.set_linear_objective_coefficient(x, 4.0)
    tracker.advance_checkpoint()
    storage.set_linear_objective_coefficient(x, 2.0)
    storage.delete_variable(x)
    self.assert_protos_equiv(
        _ModelUpdateProto(deleted_variable_ids=[0]), tracker.export_update()
    )

  def test_objective_update_new(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    storage.set_linear_objective_coefficient(x, 4.0)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            new_variables=_VariablesProto(
                ids=[0],
                lower_bounds=[-1.0],
                upper_bounds=[2.5],
                integers=[True],
                names=["x"],
            ),
            objective_updates=_ObjectiveUpdatesProto(
                linear_coefficients=_SparseDoubleVectorProto(
                    ids=[0], values=[4.0]
                )
            ),
        ),
        tracker.export_update(),
    )

  def test_objective_update_new_zero(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    storage.set_linear_objective_coefficient(x, 4.0)
    storage.set_linear_objective_coefficient(x, 0.0)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            new_variables=_VariablesProto(
                ids=[0],
                lower_bounds=[-1.0],
                upper_bounds=[2.5],
                integers=[True],
                names=["x"],
            )
        ),
        tracker.export_update(),
    )

  def test_objective_update_new_then_delete(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    storage.set_linear_objective_coefficient(x, 4.0)
    storage.delete_variable(x)
    self.assert_protos_equiv(_ModelUpdateProto(), tracker.export_update())

  def test_objective_update_old_new_ordering(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    old_handles = []
    for i in range(4):
      x = storage.add_variable(-1.0, 2.5, True, f"x_{i}")
      storage.set_linear_objective_coefficient(x, i + 1.0)
      old_handles.append(x)
    tracker.advance_checkpoint()
    for i in range(4):
      x = storage.add_variable(-1.0, 2.5, True, f"x_{i+4}")
      storage.set_linear_objective_coefficient(x, i + 10.0)
    for i, h in enumerate(old_handles):
      storage.set_linear_objective_coefficient(h, -2.0 * i)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            new_variables=_VariablesProto(
                ids=[4, 5, 6, 7],
                lower_bounds=[-1.0, -1.0, -1.0, -1.0],
                upper_bounds=[2.5, 2.5, 2.5, 2.5],
                integers=[True, True, True, True],
                names=["x_4", "x_5", "x_6", "x_7"],
            ),
            objective_updates=_ObjectiveUpdatesProto(
                linear_coefficients=_SparseDoubleVectorProto(
                    ids=[0, 1, 2, 3, 4, 5, 6, 7],
                    values=[0.0, -2.0, -4.0, -6.0, 10.0, 11.0, 12.0, 13.0],
                )
            ),
        ),
        tracker.export_update(),
    )

  def test_quadratic_objective_update_existing_zero(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    y = storage.add_variable(-1.0, 2.5, True, "y")
    tracker.advance_checkpoint()
    storage.set_quadratic_objective_coefficient(x, y, 3.0)
    expected = _ModelUpdateProto(
        objective_updates=_ObjectiveUpdatesProto(
            quadratic_coefficients=sparse_containers_pb2.SparseDoubleMatrixProto(
                row_ids=[0], column_ids=[1], coefficients=[3.0]
            )
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_quadratic_objective_update_existing_zero_same(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    y = storage.add_variable(-1.0, 2.5, True, "y")
    tracker.advance_checkpoint()
    storage.set_quadratic_objective_coefficient(x, y, 0.0)
    self.assertIsNone(tracker.export_update())

  def test_quadratic_objective_update_existing_nonzero(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    y = storage.add_variable(-1.0, 2.5, True, "y")
    storage.set_quadratic_objective_coefficient(x, y, 4.0)
    tracker.advance_checkpoint()
    storage.set_quadratic_objective_coefficient(x, y, 3.0)
    expected = _ModelUpdateProto(
        objective_updates=_ObjectiveUpdatesProto(
            quadratic_coefficients=sparse_containers_pb2.SparseDoubleMatrixProto(
                row_ids=[0], column_ids=[1], coefficients=[3.0]
            )
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_quadratic_objective_update_existing_nonzero_same(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    y = storage.add_variable(-1.0, 2.5, True, "y")
    storage.set_quadratic_objective_coefficient(x, y, 4.0)
    tracker.advance_checkpoint()
    storage.set_quadratic_objective_coefficient(x, y, 4.0)
    self.assertIsNone(tracker.export_update())

  def test_quadratic_objective_update_clear(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    y = storage.add_variable(0.0, 1.0, False, "y")
    z = storage.add_variable(0.0, 1.0, True, "z")

    storage.set_linear_objective_coefficient(x, 2.0)
    storage.set_linear_objective_coefficient(z, -5.5)
    storage.set_quadratic_objective_coefficient(x, y, 4.0)
    storage.set_objective_offset(1.0)
    self.assertEqual(2.0, storage.get_linear_objective_coefficient(x))
    self.assertEqual(0.0, storage.get_linear_objective_coefficient(y))
    self.assertEqual(-5.5, storage.get_linear_objective_coefficient(z))
    self.assertEqual(0.0, storage.get_quadratic_objective_coefficient(x, x))
    self.assertEqual(4.0, storage.get_quadratic_objective_coefficient(x, y))
    self.assertEqual(1.0, storage.get_objective_offset())
    tracker.advance_checkpoint()
    w = storage.add_variable(0.0, 1.0, True, "w")
    storage.set_linear_objective_coefficient(w, 1.0)
    storage.set_quadratic_objective_coefficient(w, w, 2.0)
    storage.clear_objective()
    expected = _ModelUpdateProto(
        new_variables=_VariablesProto(
            ids=[3],
            lower_bounds=[0.0],
            upper_bounds=[1.0],
            integers=[True],
            names=["w"],
        ),
        objective_updates=_ObjectiveUpdatesProto(
            offset_update=0.0,
            linear_coefficients=_SparseDoubleVectorProto(
                ids=[x, z], values=[0.0, 0.0]
            ),
            quadratic_coefficients=sparse_containers_pb2.SparseDoubleMatrixProto(
                row_ids=[x], column_ids=[y], coefficients=[0.0]
            ),
        ),
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_quadratic_objective_update_existing_to_zero(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    y = storage.add_variable(-1.0, 2.5, True, "y")
    storage.set_quadratic_objective_coefficient(x, y, 4.0)
    tracker.advance_checkpoint()
    storage.set_quadratic_objective_coefficient(x, y, 0.0)
    expected = _ModelUpdateProto(
        objective_updates=_ObjectiveUpdatesProto(
            quadratic_coefficients=sparse_containers_pb2.SparseDoubleMatrixProto(
                row_ids=[x], column_ids=[y], coefficients=[0.0]
            )
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_quadratic_objective_update_existing_then_delete(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    y = storage.add_variable(-1.0, 2.5, True, "y")
    storage.set_quadratic_objective_coefficient(x, y, 4.0)
    tracker.advance_checkpoint()
    storage.set_quadratic_objective_coefficient(x, y, 2.0)
    storage.delete_variable(x)
    self.assert_protos_equiv(
        _ModelUpdateProto(deleted_variable_ids=[0]), tracker.export_update()
    )

  def test_quadratic_objective_update_new(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    storage.set_quadratic_objective_coefficient(x, x, 4.0)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            new_variables=_VariablesProto(
                ids=[0],
                lower_bounds=[-1.0],
                upper_bounds=[2.5],
                integers=[True],
                names=["x"],
            ),
            objective_updates=_ObjectiveUpdatesProto(
                quadratic_coefficients=sparse_containers_pb2.SparseDoubleMatrixProto(
                    row_ids=[x], column_ids=[x], coefficients=[4.0]
                )
            ),
        ),
        tracker.export_update(),
    )

  def test_quadratic_objective_update_new_old_deleted(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    old_var1 = storage.add_variable(-1.0, 2.5, True, "old1")
    old_var2 = storage.add_variable(-1.0, 2.5, True, "old2")
    deleted_var1 = storage.add_variable(-1.0, 2.5, True, "deleted1")
    deleted_var2 = storage.add_variable(-1.0, 2.5, True, "deleted2")
    tracker.advance_checkpoint()
    new_var1 = storage.add_variable(0.0, 1.0, True, "new1")
    new_var2 = storage.add_variable(0.0, 1.0, True, "new2")
    storage.set_quadratic_objective_coefficient(old_var1, old_var1, 1.0)
    storage.set_quadratic_objective_coefficient(old_var1, old_var2, 2.0)
    storage.set_quadratic_objective_coefficient(old_var1, new_var1, 3.0)
    storage.set_quadratic_objective_coefficient(new_var1, new_var1, 4.0)
    storage.set_quadratic_objective_coefficient(new_var1, new_var2, 5.0)
    storage.set_quadratic_objective_coefficient(deleted_var1, deleted_var1, 6.0)
    storage.set_quadratic_objective_coefficient(deleted_var1, deleted_var2, 7.0)
    storage.set_quadratic_objective_coefficient(deleted_var1, old_var1, 8.0)
    storage.set_quadratic_objective_coefficient(deleted_var1, new_var1, 9.0)
    storage.delete_variable(deleted_var1)
    storage.delete_variable(deleted_var2)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            deleted_variable_ids=[deleted_var1, deleted_var2],
            new_variables=_VariablesProto(
                ids=[new_var1, new_var2],
                lower_bounds=[0.0, 0.0],
                upper_bounds=[1.0, 1.0],
                integers=[True, True],
                names=["new1", "new2"],
            ),
            objective_updates=_ObjectiveUpdatesProto(
                quadratic_coefficients=sparse_containers_pb2.SparseDoubleMatrixProto(
                    row_ids=[old_var1, old_var1, old_var1, new_var1, new_var1],
                    column_ids=[
                        old_var1,
                        old_var2,
                        new_var1,
                        new_var1,
                        new_var2,
                    ],
                    coefficients=[1.0, 2.0, 3.0, 4.0, 5.0],
                )
            ),
        ),
        tracker.export_update(),
    )

  def test_quadratic_objective_update_new_zero(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    y = storage.add_variable(-1.0, 2.5, True, "y")
    storage.set_quadratic_objective_coefficient(x, y, 4.0)
    storage.set_quadratic_objective_coefficient(x, y, 0.0)
    storage.set_linear_objective_coefficient(x, 0.0)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            new_variables=_VariablesProto(
                ids=[0, 1],
                lower_bounds=[-1.0, -1.0],
                upper_bounds=[2.5, 2.5],
                integers=[True, True],
                names=["x", "y"],
            )
        ),
        tracker.export_update(),
    )

  def test_quadratic_objective_update_new_then_delete(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    y = storage.add_variable(-1.0, 2.5, True, "y")
    storage.set_quadratic_objective_coefficient(x, y, 4.0)
    storage.delete_variable(x)
    storage.delete_variable(y)
    self.assert_protos_equiv(_ModelUpdateProto(), tracker.export_update())

  def test_quadratic_objective_update_old_new_ordering(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    old_handles = []
    for i in range(4):
      x = storage.add_variable(-1.0, 2.5, True, f"x_{i}")
      old_handles.append(x)
    for i in range(3):
      storage.set_quadratic_objective_coefficient(
          old_handles[i], old_handles[i + 1], i + 1
      )
    tracker.advance_checkpoint()
    new_handles = []
    for i in range(4):
      x = storage.add_variable(-1.0, 2.5, True, f"x_{i+4}")
      new_handles.append(x)
    for i in range(3):
      storage.set_quadratic_objective_coefficient(
          new_handles[i], new_handles[i + 1], i + 10
      )
    for i in range(3):
      storage.set_quadratic_objective_coefficient(
          old_handles[i], old_handles[i + 1], -2.0 * i
      )
    self.assert_protos_equiv(
        _ModelUpdateProto(
            new_variables=_VariablesProto(
                ids=[4, 5, 6, 7],
                lower_bounds=[-1.0, -1.0, -1.0, -1.0],
                upper_bounds=[2.5, 2.5, 2.5, 2.5],
                integers=[True, True, True, True],
                names=["x_4", "x_5", "x_6", "x_7"],
            ),
            objective_updates=_ObjectiveUpdatesProto(
                quadratic_coefficients=sparse_containers_pb2.SparseDoubleMatrixProto(
                    row_ids=[0, 1, 2, 4, 5, 6],
                    column_ids=[1, 2, 3, 5, 6, 7],
                    coefficients=[0, -2.0, -4.0, 10, 11, 12],
                )
            ),
        ),
        tracker.export_update(),
    )

  def test_update_lin_con_mat_existing_zero(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    tracker.advance_checkpoint()
    storage.set_linear_constraint_coefficient(c, x, 3.0)
    expected = _ModelUpdateProto(
        linear_constraint_matrix_updates=_SparseDoubleMatrixProto(
            row_ids=[0], column_ids=[0], coefficients=[3.0]
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_update_lin_con_mat_existing_zero_same(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    tracker.advance_checkpoint()
    storage.set_linear_constraint_coefficient(c, x, 0.0)
    self.assertIsNone(tracker.export_update())

  def test_lin_con_mat_update_existing_nonzero(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    storage.set_linear_constraint_coefficient(c, x, 1.0)
    tracker.advance_checkpoint()
    storage.set_linear_constraint_coefficient(c, x, 3.0)
    expected = _ModelUpdateProto(
        linear_constraint_matrix_updates=_SparseDoubleMatrixProto(
            row_ids=[0], column_ids=[0], coefficients=[3.0]
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_lin_con_mat_update_existing_nonzero_same(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    storage.set_linear_constraint_coefficient(c, x, 1.0)
    tracker.advance_checkpoint()
    storage.set_linear_constraint_coefficient(c, x, 1.0)
    self.assertIsNone(tracker.export_update())

  def test_lin_con_mat_update_existing_to_zero(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    storage.set_linear_constraint_coefficient(c, x, 1.0)
    tracker.advance_checkpoint()
    storage.set_linear_constraint_coefficient(c, x, 0.0)
    expected = _ModelUpdateProto(
        linear_constraint_matrix_updates=_SparseDoubleMatrixProto(
            row_ids=[0], column_ids=[0], coefficients=[0.0]
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())

  def test_lin_con_mat_update_existing_then_delete_var(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    storage.set_linear_constraint_coefficient(c, x, 1.0)
    tracker.advance_checkpoint()
    storage.set_linear_constraint_coefficient(c, x, 6.0)
    storage.delete_variable(x)
    self.assert_protos_equiv(
        _ModelUpdateProto(deleted_variable_ids=[0]), tracker.export_update()
    )

  def test_lin_con_mat_update_existing_then_delete_lin_con(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    storage.set_linear_constraint_coefficient(c, x, 1.0)
    tracker.advance_checkpoint()
    storage.set_linear_constraint_coefficient(c, x, 6.0)
    storage.delete_linear_constraint(c)
    self.assert_protos_equiv(
        _ModelUpdateProto(deleted_linear_constraint_ids=[0]),
        tracker.export_update(),
    )

  def test_lin_con_mat_update_existing_then_delete_both(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    storage.set_linear_constraint_coefficient(c, x, 1.0)
    tracker.advance_checkpoint()
    storage.set_linear_constraint_coefficient(c, x, 6.0)
    storage.delete_linear_constraint(c)
    storage.delete_variable(x)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            deleted_variable_ids=[0], deleted_linear_constraint_ids=[0]
        ),
        tracker.export_update(),
    )

  def test_lin_con_mat_update_new_var(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    tracker.advance_checkpoint()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    storage.set_linear_constraint_coefficient(c, x, 4.0)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            new_variables=_VariablesProto(
                ids=[0],
                lower_bounds=[-1.0],
                upper_bounds=[2.5],
                integers=[True],
                names=["x"],
            ),
            linear_constraint_matrix_updates=_SparseDoubleMatrixProto(
                row_ids=[0], column_ids=[0], coefficients=[4.0]
            ),
        ),
        tracker.export_update(),
    )

  def test_lin_con_mat_update_new_lin_con(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    tracker.advance_checkpoint()
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    storage.set_linear_constraint_coefficient(c, x, 4.0)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            new_linear_constraints=_LinearConstraintsProto(
                ids=[0], lower_bounds=[-1.0], upper_bounds=[2.5], names=["c"]
            ),
            linear_constraint_matrix_updates=_SparseDoubleMatrixProto(
                row_ids=[0], column_ids=[0], coefficients=[4.0]
            ),
        ),
        tracker.export_update(),
    )

  def test_lin_con_mat_update_new_both(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    storage.set_linear_constraint_coefficient(c, x, 4.0)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            new_variables=_VariablesProto(
                ids=[0],
                lower_bounds=[-1.0],
                upper_bounds=[2.5],
                integers=[True],
                names=["x"],
            ),
            new_linear_constraints=_LinearConstraintsProto(
                ids=[0], lower_bounds=[-1.0], upper_bounds=[2.5], names=["c"]
            ),
            linear_constraint_matrix_updates=_SparseDoubleMatrixProto(
                row_ids=[0], column_ids=[0], coefficients=[4.0]
            ),
        ),
        tracker.export_update(),
    )

  def test_lin_con_mat_update_new_zero(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    tracker.advance_checkpoint()
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    storage.set_linear_constraint_coefficient(c, x, 4.0)
    storage.set_linear_constraint_coefficient(c, x, 0.0)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            new_linear_constraints=_LinearConstraintsProto(
                ids=[0], lower_bounds=[-1.0], upper_bounds=[2.5], names=["c"]
            )
        ),
        tracker.export_update(),
    )

  def test_lin_con_mat_update_new_then_delete(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    x = storage.add_variable(-1.0, 2.5, True, "x")
    tracker.advance_checkpoint()
    c = storage.add_linear_constraint(-1.0, 2.5, "c")
    storage.set_linear_constraint_coefficient(c, x, 4.0)
    storage.delete_variable(x)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            deleted_variable_ids=[0],
            new_linear_constraints=_LinearConstraintsProto(
                ids=[0], lower_bounds=[-1.0], upper_bounds=[2.5], names=["c"]
            ),
        ),
        tracker.export_update(),
    )

  def test_lin_con_mat_update_old_new_ordering(
      self, storage_class: _StorageClass
  ) -> None:
    storage = storage_class("test_model")
    tracker = storage.add_update_tracker()
    var_handles = [storage.add_variable(0.0, 1.0, True, "") for _ in range(2)]
    lin_con_handles = [
        storage.add_linear_constraint(0.0, 1.0, "") for _ in range(2)
    ]
    for v in var_handles:
      for l in lin_con_handles:
        storage.set_linear_constraint_coefficient(l, v, 1.0)
    tracker.advance_checkpoint()
    x = storage.add_variable(0.0, 1.0, True, "x")
    c = storage.add_linear_constraint(0.0, 1.0, "c")
    storage.set_linear_constraint_coefficient(
        lin_con_handles[0], var_handles[0], 5.0
    )
    storage.set_linear_constraint_coefficient(lin_con_handles[0], x, 4.0)
    storage.set_linear_constraint_coefficient(c, var_handles[1], 3.0)
    storage.set_linear_constraint_coefficient(c, x, 2.0)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            new_variables=_VariablesProto(
                ids=[2],
                lower_bounds=[0.0],
                upper_bounds=[1.0],
                integers=[True],
                names=["x"],
            ),
            new_linear_constraints=_LinearConstraintsProto(
                ids=[2], lower_bounds=[0.0], upper_bounds=[1.0], names=["c"]
            ),
            linear_constraint_matrix_updates=_SparseDoubleMatrixProto(
                row_ids=[0, 0, 2, 2],
                column_ids=[0, 2, 1, 2],
                coefficients=[5.0, 4.0, 3.0, 2.0],
            ),
        ),
        tracker.export_update(),
    )

  def test_remove_update_tracker(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    x = storage.add_variable(0.0, 1.0, True, "x")
    tracker = storage.add_update_tracker()
    storage.set_variable_ub(x, 7.0)
    expected = _ModelUpdateProto(
        variable_updates=_VariableUpdatesProto(
            upper_bounds=_SparseDoubleVectorProto(ids=[0], values=[7.0])
        )
    )
    self.assert_protos_equiv(expected, tracker.export_update())
    storage.remove_update_tracker(tracker)
    with self.assertRaises(model_storage.UsedUpdateTrackerAfterRemovalError):
      tracker.export_update()
    with self.assertRaises(model_storage.UsedUpdateTrackerAfterRemovalError):
      tracker.advance_checkpoint()
    with self.assertRaises(KeyError):
      storage.remove_update_tracker(tracker)

  def test_remove_update_tracker_wrong_model(
      self, storage_class: _StorageClass
  ) -> None:
    storage1 = storage_class("test_model1")
    storage2 = storage_class("test_model2")
    tracker1 = storage1.add_update_tracker()
    with self.assertRaises(KeyError):
      storage2.remove_update_tracker(tracker1)

  def test_multiple_update_tracker(self, storage_class: _StorageClass) -> None:
    storage = storage_class("test_model")
    x = storage.add_variable(0.0, 1.0, True, "x")
    y = storage.add_variable(0.0, 1.0, True, "y")
    tracker1 = storage.add_update_tracker()
    storage.set_variable_ub(x, 7.0)
    tracker2 = storage.add_update_tracker()
    storage.set_variable_ub(y, 3.0)
    self.assert_protos_equiv(
        _ModelUpdateProto(
            variable_updates=_VariableUpdatesProto(
                upper_bounds=_SparseDoubleVectorProto(
                    ids=[0, 1], values=[7.0, 3.0]
                )
            )
        ),
        tracker1.export_update(),
    )
    self.assert_protos_equiv(
        _ModelUpdateProto(
            variable_updates=_VariableUpdatesProto(
                upper_bounds=_SparseDoubleVectorProto(ids=[1], values=[3.0])
            )
        ),
        tracker2.export_update(),
    )


if __name__ == "__main__":
  absltest.main()
