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

"""Tests for mathopt elemental python bindings."""

import math

import numpy as np

from absl.testing import absltest
from ortools.math_opt import model_pb2
from ortools.math_opt import model_update_pb2
from ortools.math_opt.elemental.python import cpp_elemental
from ortools.math_opt.elemental.python import enums
from ortools.math_opt.python.testing import compare_proto

_VARIABLE = enums.ElementType.VARIABLE
_LINEAR_CONSTRAINT = enums.ElementType.LINEAR_CONSTRAINT
_INDICATOR_CONSTRAINT = enums.ElementType.INDICATOR_CONSTRAINT

_MAXIMIZE = enums.BoolAttr0.MAXIMIZE
_VARIABLE_LOWER_BOUND = enums.DoubleAttr1.VARIABLE_LOWER_BOUND
_LINEAR_CONSTRAINT_COEFFICIENT = enums.DoubleAttr2.LINEAR_CONSTRAINT_COEFFICIENT
_OBJECTIVE_QUADRATIC_COEFFICIENT = (
    enums.SymmetricDoubleAttr2.OBJECTIVE_QUADRATIC_COEFFICIENT
)
_INDICATOR_CONSTRAINT_INDICATOR = (
    enums.VariableAttr1.INDICATOR_CONSTRAINT_INDICATOR
)


def _sort_attr_keys(
    attr_keys: np.typing.NDArray[np.int64],
) -> np.typing.NDArray[np.int64]:
  """Sorts attr_keys lexicographically."""
  return attr_keys[np.lexsort(np.rot90(attr_keys))]


class BindingsTest(compare_proto.MathOptProtoAssertions, absltest.TestCase):

  def test_init_names_not_set(self):
    e = cpp_elemental.CppElemental()
    self.assertEqual(e.model_name, "")
    self.assertEqual(e.primary_objective_name, "")

  def test_init_names_set(self):
    e = cpp_elemental.CppElemental(
        model_name="abc", primary_objective_name="123"
    )
    self.assertEqual(e.model_name, "abc")
    self.assertEqual(e.primary_objective_name, "123")

  def test_element_operations(self):
    e = cpp_elemental.CppElemental()

    # Add two variables.
    xs = e.add_elements(_VARIABLE, 2)

    self.assertEqual(e.get_num_elements(_VARIABLE), 2)
    self.assertEqual(e.get_next_element_id(_VARIABLE), xs[-1] + 1)
    np.testing.assert_array_equal(
        e.elements_exist(_VARIABLE, xs), [True, True], strict=True
    )

    # Delete first variable.
    e.delete_elements(_VARIABLE, xs[0:1])

    np.testing.assert_array_equal(
        e.elements_exist(_VARIABLE, xs), [False, True], strict=True
    )

    # Add constraint c.
    c = e.add_element(_LINEAR_CONSTRAINT, "c")

    self.assertEqual(e.get_num_elements(_LINEAR_CONSTRAINT), 1)
    self.assertEqual(e.element_exists(_LINEAR_CONSTRAINT, c), True)
    self.assertEqual(e.get_element_name(_LINEAR_CONSTRAINT, c), "c")
    np.testing.assert_array_equal(
        e.get_elements(_VARIABLE), [xs[1]], strict=True
    )
    np.testing.assert_array_equal(
        e.get_elements(_LINEAR_CONSTRAINT),
        np.array([c], dtype=np.int64),
        strict=True,
    )

  def test_ensure_next_element_id_at_least(self):
    e = cpp_elemental.CppElemental()
    e.ensure_next_element_id_at_least(_VARIABLE, 4)
    self.assertEqual(e.add_element(_VARIABLE, "x"), 4)

  def test_name_handling(self):
    e = cpp_elemental.CppElemental()
    ids = e.add_named_elements(
        _LINEAR_CONSTRAINT,
        np.array(["c", "name", "a somewhat long name", "a 💩 name"]),
    )
    np.testing.assert_array_equal(
        e.get_element_names(_LINEAR_CONSTRAINT, ids),
        np.array(["c", "name", "a somewhat long name", "a 💩 name"]),
        strict=True,
    )
    with self.assertRaisesRegex(ValueError, "got 1d array of dtype l"):
      e.add_named_elements(_LINEAR_CONSTRAINT, np.array([1, 2, 3]))
    with self.assertRaisesRegex(ValueError, "got 2d array of dtype U"):
      e.add_named_elements(
          _LINEAR_CONSTRAINT, np.array([["a", "b"], ["c", "d"]])
      )

  def test_delete_with_duplicates_raises(self):
    e = cpp_elemental.CppElemental()
    xs = e.add_elements(_VARIABLE, 1)
    with self.assertRaisesRegex(ValueError, "duplicates"):
      e.delete_elements(_VARIABLE, np.array([xs[0], xs[0]]))

  def test_element_operations_bad_shape(self):
    e = cpp_elemental.CppElemental()
    ids = e.add_elements(_VARIABLE, 2)
    with self.assertRaisesRegex(
        ValueError, "array has incorrect number of dimensions: 2; expected 1"
    ):
      e.delete_elements(_VARIABLE, np.full((1, 1), ids[0]))

  def test_bad_element_type_raises(self):
    e = cpp_elemental.CppElemental()
    with self.assertRaisesRegex(TypeError, "incompatible function arguments"):
      e.add_elements(-42, 1)  # pytype: disable=wrong-arg-types

  def test_attr0(self):
    e = cpp_elemental.CppElemental()
    keys = np.empty((1, 0), np.int64)
    default_value = e.get_attrs(_MAXIMIZE, keys)
    self.assertFalse(e.is_attr_non_default(_MAXIMIZE, keys[0]))
    self.assertEqual(e.get_attr_num_non_defaults(_MAXIMIZE), 0)
    np.testing.assert_array_equal(
        e.get_attr_non_defaults(_MAXIMIZE),
        np.empty((0, 0), np.int64),
        strict=True,
    )

    new_value = np.invert(default_value)
    e.set_attrs(_MAXIMIZE, keys, new_value)

    self.assertEqual(e.get_attrs(_MAXIMIZE, keys), new_value)
    np.testing.assert_array_equal(
        e.bulk_is_attr_non_default(_MAXIMIZE, keys),
        np.array([True]),
        strict=True,
    )
    self.assertEqual(e.get_attr_num_non_defaults(_MAXIMIZE), 1)
    np.testing.assert_array_equal(
        e.get_attr_non_defaults(_MAXIMIZE), keys, strict=True
    )

  def test_attr1(self):
    e = cpp_elemental.CppElemental()
    x = e.add_elements(_VARIABLE, 3)
    keys = np.column_stack([x])
    np.testing.assert_array_equal(
        e.get_attrs(_VARIABLE_LOWER_BOUND, keys),
        np.array([-np.inf, -np.inf, -np.inf]),
    )
    self.assertEqual(e.get_attr(_VARIABLE_LOWER_BOUND, keys[0]), -math.inf)
    np.testing.assert_array_equal(
        e.bulk_is_attr_non_default(_VARIABLE_LOWER_BOUND, keys),
        np.array([False, False, False]),
        strict=True,
    )
    self.assertEqual(
        e.is_attr_non_default(_VARIABLE_LOWER_BOUND, keys[0]), False
    )
    np.testing.assert_array_equal(
        e.get_attr_non_defaults(_VARIABLE_LOWER_BOUND),
        np.empty((0, 1), np.int64),
        strict=True,
    )

    e.set_attrs(
        _VARIABLE_LOWER_BOUND,
        keys[[0, 2]],
        np.array([42.0, 44.0]),
    )
    np.testing.assert_array_equal(
        e.get_attrs(_VARIABLE_LOWER_BOUND, keys),
        np.array([42.0, -np.inf, 44.0]),
        strict=True,
    )
    e.set_attr(_VARIABLE_LOWER_BOUND, keys[0], 45.0)
    np.testing.assert_array_equal(
        e.get_attrs(_VARIABLE_LOWER_BOUND, keys),
        np.array([45.0, -np.inf, 44.0]),
        strict=True,
    )
    np.testing.assert_array_equal(
        e.bulk_is_attr_non_default(_VARIABLE_LOWER_BOUND, keys),
        np.array([True, False, True]),
        strict=True,
    )
    self.assertEqual(e.get_attr_num_non_defaults(_VARIABLE_LOWER_BOUND), 2)
    # Note: sorting the result because ordering is not guaranteed.
    np.testing.assert_array_equal(
        np.sort(e.get_attr_non_defaults(_VARIABLE_LOWER_BOUND), axis=0),
        np.array([[x[0]], [x[2]]]),
        strict=True,
    )

  def test_attr2(self):
    e = cpp_elemental.CppElemental()
    x = e.add_elements(_VARIABLE, 1)
    c = e.add_elements(_LINEAR_CONSTRAINT, 1)
    keys = np.column_stack([x, c])
    np.testing.assert_array_equal(
        e.get_attrs(_LINEAR_CONSTRAINT_COEFFICIENT, keys),
        np.array([0.0]),
        strict=True,
    )
    np.testing.assert_array_equal(
        e.bulk_is_attr_non_default(_LINEAR_CONSTRAINT_COEFFICIENT, keys),
        np.array([False]),
        strict=True,
    )
    self.assertEqual(
        e.get_attr_num_non_defaults(_LINEAR_CONSTRAINT_COEFFICIENT), 0
    )
    np.testing.assert_array_equal(
        e.get_attr_non_defaults(_LINEAR_CONSTRAINT_COEFFICIENT),
        np.empty((0, 2), np.int64),
        strict=True,
    )

    e.set_attrs(_LINEAR_CONSTRAINT_COEFFICIENT, keys, np.array([42.0]))
    np.testing.assert_array_equal(
        e.get_attrs(_LINEAR_CONSTRAINT_COEFFICIENT, keys),
        np.array([42.0]),
        strict=True,
    )
    np.testing.assert_array_equal(
        e.bulk_is_attr_non_default(_LINEAR_CONSTRAINT_COEFFICIENT, keys),
        np.array([True]),
        strict=True,
    )
    self.assertEqual(
        e.get_attr_num_non_defaults(_LINEAR_CONSTRAINT_COEFFICIENT), 1
    )
    np.testing.assert_array_equal(
        e.get_attr_non_defaults(_LINEAR_CONSTRAINT_COEFFICIENT),
        keys,
        strict=True,
    )

  def test_attr2_symmetric(self):
    e = cpp_elemental.CppElemental()
    xs = e.add_elements(_VARIABLE, 3)
    q01 = [xs[0], xs[1]]
    q21 = [xs[2], xs[1]]
    q12 = [xs[1], xs[2]]

    e.set_attr(_OBJECTIVE_QUADRATIC_COEFFICIENT, q01, 42.0)
    e.set_attr(_OBJECTIVE_QUADRATIC_COEFFICIENT, q21, 43.0)
    e.set_attr(_OBJECTIVE_QUADRATIC_COEFFICIENT, q12, 44.0)
    self.assertEqual(
        e.get_attr_num_non_defaults(_OBJECTIVE_QUADRATIC_COEFFICIENT), 2
    )

    # Note: sorting the result because ordering is not guaranteed.
    np.testing.assert_array_equal(
        np.sort(
            e.get_attr_non_defaults(_OBJECTIVE_QUADRATIC_COEFFICIENT), axis=0
        ),
        np.array([q01, q12]),
        strict=True,
    )

  def test_attr1_element_valued(self):
    e = cpp_elemental.CppElemental()
    x = e.add_element(_VARIABLE, "x")
    ic = e.add_element(_INDICATOR_CONSTRAINT, "ic")

    e.set_attr(_INDICATOR_CONSTRAINT_INDICATOR, [ic], x)
    self.assertEqual(
        e.get_attr_num_non_defaults(_INDICATOR_CONSTRAINT_INDICATOR), 1
    )

  def test_clear_attr0(self):
    e = cpp_elemental.CppElemental()
    e.set_attr(_MAXIMIZE, (), True)
    self.assertTrue(e.get_attr(_MAXIMIZE, ()))
    e.clear_attr(_MAXIMIZE)
    self.assertFalse(e.get_attr(_MAXIMIZE, ()))

  def test_clear_attr1(self):
    e = cpp_elemental.CppElemental()
    x = e.add_element(_VARIABLE, "x")
    e.set_attr(_VARIABLE_LOWER_BOUND, (x,), 4.0)
    self.assertEqual(e.get_attr(_VARIABLE_LOWER_BOUND, (x,)), 4.0)
    e.clear_attr(_VARIABLE_LOWER_BOUND)
    self.assertEqual(e.get_attr(_VARIABLE_LOWER_BOUND, (x,)), -math.inf)

  def test_attr0_bad_attr_id_raises(self):
    e = cpp_elemental.CppElemental()
    with self.assertRaisesRegex(TypeError, "incompatible function arguments"):
      e.get_attrs(-42, np.array([1]))  # pytype: disable=wrong-arg-types
    # Note: `assertRaisesRegex` does not seem to work with multiline regexps.
    with self.assertRaisesRegex(TypeError, "incompatible function arguments"):
      e.get_attrs(_VARIABLE, ())  # pytype: disable=wrong-arg-types
    with self.assertRaisesRegex(TypeError, "attr: BoolAttr0"):
      e.get_attrs(_VARIABLE, ())  # pytype: disable=wrong-arg-types
    with self.assertRaisesRegex(TypeError, "attr: DoubleAttr1"):
      e.get_attrs(_VARIABLE, ())  # pytype: disable=wrong-arg-types

  def test_attr1_bad_element_id_raises(self):
    e = cpp_elemental.CppElemental()
    with self.assertRaisesRegex(ValueError, "-1.*variable"):
      e.get_attrs(_VARIABLE_LOWER_BOUND, np.array([[-1]]))

  def test_set_attr_with_duplicates_raises(self):
    e = cpp_elemental.CppElemental()
    x = e.add_elements(_VARIABLE, 2)
    with self.assertRaisesRegex(ValueError, "array has duplicates"):
      e.set_attrs(
          _VARIABLE_LOWER_BOUND,
          np.array([[x[0]], [x[1]], [x[1]]]),
          np.array([42.0, 44.0, 46.0]),
      )
    # We should not have modified any attribute.
    self.assertEqual(e.get_attr_num_non_defaults(_VARIABLE_LOWER_BOUND), 0)

  def test_set_attr_with_nonexistent_raises(self):
    e = cpp_elemental.CppElemental()
    x = e.add_elements(_VARIABLE, 1)
    with self.assertRaisesRegex(
        ValueError, "linear_constraint id 0 does not exist"
    ):
      e.set_attrs(
          _LINEAR_CONSTRAINT_COEFFICIENT,
          np.array([[x[0], -1]]),
          np.array([42.0]),
      )

  def test_slice_attr1_success(self):
    e = cpp_elemental.CppElemental()
    x = e.add_element(_VARIABLE, "x")
    y = e.add_element(_VARIABLE, "y")
    e.set_attr(_VARIABLE_LOWER_BOUND, (x,), 2.0)
    np.testing.assert_array_equal(
        e.slice_attr(_VARIABLE_LOWER_BOUND, 0, x),
        np.array([[x]], dtype=np.int64),
        strict=True,
    )
    self.assertEqual(e.get_attr_slice_size(_VARIABLE_LOWER_BOUND, 0, x), 1)
    np.testing.assert_array_equal(
        e.slice_attr(_VARIABLE_LOWER_BOUND, 0, y),
        np.zeros((0, 1), dtype=np.int64),
        strict=True,
    )
    self.assertEqual(e.get_attr_slice_size(_VARIABLE_LOWER_BOUND, 0, y), 0)

  def test_slice_attr1_invalid_key_index(self):
    e = cpp_elemental.CppElemental()
    x = e.add_element(_VARIABLE, "x")
    with self.assertRaisesRegex(ValueError, "key_index was: -1"):
      e.slice_attr(_VARIABLE_LOWER_BOUND, -1, x)
    with self.assertRaisesRegex(ValueError, "key_index was: 1"):
      e.slice_attr(_VARIABLE_LOWER_BOUND, 1, x)

  def test_slice_attr1_invalid_element_index(self):
    e = cpp_elemental.CppElemental()
    e.add_element(_VARIABLE, "x")
    with self.assertRaisesRegex(ValueError, "no element with id -1"):
      e.slice_attr(_VARIABLE_LOWER_BOUND, 0, -1)
    with self.assertRaisesRegex(ValueError, "no element with id 4"):
      e.slice_attr(_VARIABLE_LOWER_BOUND, 0, 4)

  def test_slice_attr2_success(self):
    e = cpp_elemental.CppElemental()
    # The first two variables are unused so that all variable and constraint
    # indices are all different.
    e.add_element(_VARIABLE, "")
    e.add_element(_VARIABLE, "")
    x = e.add_element(_VARIABLE, "x")
    y = e.add_element(_VARIABLE, "y")
    z = e.add_element(_VARIABLE, "z")

    c = e.add_element(_LINEAR_CONSTRAINT, "c")
    d = e.add_element(_LINEAR_CONSTRAINT, "d")
    e.set_attr(_LINEAR_CONSTRAINT_COEFFICIENT, (c, x), 2.0)
    e.set_attr(_LINEAR_CONSTRAINT_COEFFICIENT, (d, x), 3.0)
    e.set_attr(_LINEAR_CONSTRAINT_COEFFICIENT, (d, y), 4.0)
    np.testing.assert_array_equal(
        e.slice_attr(_LINEAR_CONSTRAINT_COEFFICIENT, 0, c),
        np.array([[c, x]], dtype=np.int64),
        strict=True,
    )
    self.assertEqual(
        e.get_attr_slice_size(_LINEAR_CONSTRAINT_COEFFICIENT, 0, c), 1
    )

    np.testing.assert_array_equal(
        _sort_attr_keys(e.slice_attr(_LINEAR_CONSTRAINT_COEFFICIENT, 0, d)),
        np.array([[d, x], [d, y]], dtype=np.int64),
        strict=True,
    )
    self.assertEqual(
        e.get_attr_slice_size(_LINEAR_CONSTRAINT_COEFFICIENT, 0, d), 2
    )

    np.testing.assert_array_equal(
        _sort_attr_keys(e.slice_attr(_LINEAR_CONSTRAINT_COEFFICIENT, 1, x)),
        np.array([[c, x], [d, x]], dtype=np.int64),
        strict=True,
    )
    self.assertEqual(
        e.get_attr_slice_size(_LINEAR_CONSTRAINT_COEFFICIENT, 1, x), 2
    )

    np.testing.assert_array_equal(
        e.slice_attr(_LINEAR_CONSTRAINT_COEFFICIENT, 1, y),
        np.array([[d, y]], dtype=np.int64),
        strict=True,
    )
    self.assertEqual(
        e.get_attr_slice_size(_LINEAR_CONSTRAINT_COEFFICIENT, 1, y), 1
    )

    np.testing.assert_array_equal(
        e.slice_attr(_LINEAR_CONSTRAINT_COEFFICIENT, 1, z),
        np.zeros((0, 2), dtype=np.int64),
        strict=True,
    )
    self.assertEqual(
        e.get_attr_slice_size(_LINEAR_CONSTRAINT_COEFFICIENT, 1, z), 0
    )

  def test_clone(self):
    e = cpp_elemental.CppElemental(model_name="mmm")
    x = e.add_element(_VARIABLE, "x")
    e.set_attr(_VARIABLE_LOWER_BOUND, (x,), 4.0)

    e2 = e.clone()
    self.assertEqual(e2.model_name, "mmm")
    np.testing.assert_array_equal(
        e2.get_elements(_VARIABLE), np.array([x], dtype=np.int64), strict=True
    )
    np.testing.assert_array_equal(
        e2.get_attr_non_defaults(_VARIABLE_LOWER_BOUND),
        np.array([[x]], dtype=np.int64),
        strict=True,
    )
    self.assertEqual(e2.get_attr(_VARIABLE_LOWER_BOUND, (x,)), 4.0)

  def test_clone_with_rename(self):
    e = cpp_elemental.CppElemental(model_name="mmm")
    x = e.add_element(_VARIABLE, "x")
    e2 = e.clone(new_model_name="yyy")
    self.assertEqual(e2.model_name, "yyy")
    np.testing.assert_array_equal(
        e2.get_elements(_VARIABLE), np.array([x], dtype=np.int64), strict=True
    )

  def test_export_model(self):
    e = cpp_elemental.CppElemental()
    x = e.add_element(_VARIABLE, "x")
    e.set_attr(_VARIABLE_LOWER_BOUND, (x,), 4.0)

    expected = model_pb2.ModelProto(
        variables=model_pb2.VariablesProto(
            ids=[0],
            lower_bounds=[4.0],
            upper_bounds=[math.inf],
            integers=[False],
            names=["x"],
        )
    )
    self.assert_protos_equal(e.export_model(), expected)

    expected.variables.names[:] = []
    self.assert_protos_equal(e.export_model(remove_names=True), expected)

  def test_repr(self):
    e = cpp_elemental.CppElemental()
    e.add_element(_VARIABLE, "xyz")
    self.assertEqual(
        repr(e),
        """Model:
ElementType: variable num_elements: 1 next_id: 1
  id: 0 name: "xyz\"""",
    )

  def test_add_and_delete_diffs(self):
    e = cpp_elemental.CppElemental()
    self.assertEqual(e.add_diff(), 0)
    self.assertEqual(e.add_diff(), 1)
    e.delete_diff(1)

  def test_export_model_update_has_update(self):
    e = cpp_elemental.CppElemental()
    d = e.add_diff()
    e.add_element(_VARIABLE, "xyz")

    update = e.export_model_update(d)

    self.assertIsNotNone(update)
    expected = model_update_pb2.ModelUpdateProto(
        new_variables=model_pb2.VariablesProto(
            ids=[0],
            lower_bounds=[-math.inf],
            upper_bounds=[math.inf],
            integers=[False],
            names=["xyz"],
        )
    )
    self.assert_protos_equal(update, expected)

    # Now export again without names
    update_no_names = e.export_model_update(d, remove_names=True)
    self.assertIsNotNone(update_no_names)
    expected.new_variables.names[:] = []
    self.assert_protos_equal(update_no_names, expected)

  def test_export_model_update_empty(self):
    e = cpp_elemental.CppElemental()
    d = e.add_diff()
    update = e.export_model_update(d)
    self.assertIsNone(update)

  def test_advance_diff(self):
    e = cpp_elemental.CppElemental()
    d = e.add_diff()
    e.add_element(_VARIABLE, "xyz")
    e.advance_diff(d)
    update = e.export_model_update(d)
    self.assertIsNone(update)

  def test_delete_diff_twice_error(self):
    e = cpp_elemental.CppElemental()
    self.assertEqual(e.add_diff(), 0)
    e.delete_diff(0)
    with self.assertRaisesRegex(ValueError, "no diff with id: 0"):
      e.delete_diff(0)

  def test_delete_diff_never_created_error(self):
    e = cpp_elemental.CppElemental()
    with self.assertRaisesRegex(ValueError, "no diff with id: 0"):
      e.delete_diff(0)

  def test_export_model_update_diff_never_created(self):
    e = cpp_elemental.CppElemental()
    with self.assertRaisesRegex(ValueError, "no diff with id: 0"):
      e.export_model_update(0)

  def test_advance_diff_never_created(self):
    e = cpp_elemental.CppElemental()
    with self.assertRaisesRegex(ValueError, "no diff with id: 0"):
      e.advance_diff(0)


if __name__ == "__main__":
  absltest.main()
