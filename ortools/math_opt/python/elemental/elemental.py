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

"""An API for storing a MathOpt model and tracking model modifications."""

from typing import Optional, Protocol, Sequence

import numpy as np

# typing.Self is only in python 3.11+, for OR-tools supports down to 3.8.
from typing_extensions import Self

from ortools.math_opt import model_pb2
from ortools.math_opt import model_update_pb2
from ortools.math_opt.elemental.python import enums


class Elemental(Protocol):
    """An API for building, modifying, and tracking changes to a MathOpt model.

    On functions that return protocol buffers: These functions can fail for two
    reasons:
     (1) The data is too large for proto's in memory representation. Specifically,
         any repeated field can have at most 2^31 entries (~2 billion). So if your
         model has this many nonzeros in the constraint matrix, we cannot build a
         proto for it (we can potentially export to a text format still).
     (2) The particular combination of Elemental and Proto you are using must
         serialize your message (typically to cross a Python/C++ language
         boundary). Proto has a limit of 2GB for serialized messages, which is
         generally hit much earlier than the repeated field limit.

    Note that for users solving locally, they can avoid needing to serialize
    their proto by:
     - using the C++ implementation of Elemental
     - using the upb or cpp implementations of proto for python and compile
       correctly, see go/fastpythonproto and
       https://github.com/protocolbuffers/protobuf/blob/main/python/README.md.
    """

    def __init__(
        self, *, model_name: str = "", primary_objective_name: str = ""
    ) -> None:
        """Creates an empty optimization model.

        Args:
          model_name: The name of the model, used for logging and export only.
          primary_objective_name: The name of the main objective of the problem.
            Typically used only for multi-objective problems.
        """

    @classmethod
    def from_model_proto(cls, proto: model_pb2.ModelProto) -> Self:
        """Returns an Elemental equivalent to the input proto."""

    def clone(self, *, new_model_name: Optional[str] = None) -> Self:
        """Returns a copy of this model with no associated diffs."""

    @property
    def model_name(self) -> str:
        """The name of the model."""

    @property
    def primary_objective_name(self) -> str:
        """The name of the primary objective of the model (rarely used)."""

    def add_element(self, element_type: enums.ElementType, name: str) -> int:
        """Adds an element of `element_type` to the model and returns its id."""

    def add_elements(
        self, element_type: enums.ElementType, num: int
    ) -> np.typing.NDArray[np.int64]:
        """Adds `num` `element_type`s to the model and returns their ids.

        All elements added will have the name ''.

        Args:
          element_type: The ElementType of elements to add to the model.
          num: How many elements are added.

        Returns:
          A numpy array with shape (num,) with the ids of the newly added elements.
        """

    def add_named_elements(
        self,
        element_type: enums.ElementType,
        names: np.typing.NDArray,
    ) -> np.typing.NDArray[np.int64]:
        """Adds an element of `element_type` for each name in names and returns ids.

        Args:
          element_type: The ElementType of elements to add to the model.
          names: The names the elements, must have shape (n,) and string values.

        Returns:
          A numpy array with shape (n,) with the ids of the newly added elements.
        """

    def delete_element(self, element_type: enums.ElementType, element_id: int) -> bool:
        """Deletes element `id` of `element_type` from model, returns success."""

    def delete_elements(
        self,
        element_type: enums.ElementType,
        elements: np.typing.NDArray[np.int64],
    ) -> np.typing.NDArray[np.bool_]:
        """Removes `elements` from the model, returning true elementwise on success.

        A value of false is returned when an element was deleted in a previous call
        to delete elements or the element was never in the model. Note that
        repeating an id in `elements` for a single call to this function results in
        an exception.

        Args:
          element_type: The ElementType of elements to delete from to the model.
          elements: The ids of the elements to delete, must have shape (n,).

        Returns:
          A numpy array with shape (n,) indicating if each element was successfully
          deleted. (Entries are false when the element id was previously deleted or
          was never in the model.)

        Raises:
          ValueError: if elements contains any duplicates. No modifications to the
            model will be applied when this exception is raised.
        """

    def get_element_name(self, element_type: enums.ElementType, element_id: int) -> str:
        """Returns the name of the element `id` of ElementType `element_type`."""

    def get_element_names(
        self,
        element_type: enums.ElementType,
        elements: np.typing.NDArray[np.int64],
    ) -> np.typing.NDArray:
        """Returns the name of each element in `elements`.

        Note that elements have a default name of '' if no name is provided.

        Args:
          element_type: The ElementType of elements to get the names for.
          elements: The ids of the elements, must have shape (n,).

        Returns:
          A numpy array with shape (n,) containing the names.

        Raises:
           ValueError: if any id from `elements` is not in the model.
        """

    def element_exists(self, element_type: enums.ElementType, element_id: int) -> bool:
        """Returns if element `id` of ElementType `element_type` is in the model."""

    def elements_exist(
        self,
        element_type: enums.ElementType,
        elements: np.typing.NDArray[np.int64],
    ) -> np.typing.NDArray[np.bool_]:
        """Returns if each id in `elements` is an element in the model.

        Args:
          element_type: The ElementType to check.
          elements: The ids to look for, must have shape (n,).

        Returns:
          A numpy array with shape (n,) containing true if each element is in the
          model (the id has been created and not deleted).
        """

    def get_next_element_id(self, element_type: enums.ElementType) -> int:
        """Returns the next available element id of type `element_type`."""

    def get_num_elements(self, element_type: enums.ElementType) -> int:
        """Returns the number of elements of type `element_type` in the model."""

    def get_elements(
        self, element_type: enums.ElementType
    ) -> np.typing.NDArray[np.int64]:
        """Returns all element ids for type `element_type` in unspecified order."""

    def ensure_next_element_id_at_least(
        self, element_type: enums.ElementType, element_id: int
    ) -> None:
        """Increases next_element_id() to `element_id` if it is currently less."""

    def set_attr(
        self,
        attr: enums.PyAttr[enums.AttrPyValueType],
        key: Sequence[int],
        values: enums.AttrPyValueType,
    ) -> None:
        """Sets an attribute to a value for a key."""

    def set_attrs(
        self,
        attr: enums.Attr[enums.AttrValueType],
        keys: np.typing.NDArray[np.int64],
        values: np.typing.NDArray[enums.AttrValueType],
    ) -> None:
        """Sets the value of an attribute for a list of keys.

        Args:
          attr: The attribute to modify, with k elements in each key.
          keys: An (n, k) array of n keys to set this attribute for.
          values: An array with shape (n,), the values to set for each key.

        Raises:
          ValueError: if (1) any key is repeated (2) any key references an element
            not in the model, (3) the shape of keys are values is invalid, or (4)
            the shape of keys and values is inconsistent. No changes are applied for
            any key if the operation fails.
        """

    def get_attr(
        self, attr: enums.PyAttr[enums.AttrPyValueType], key: Sequence[int]
    ) -> enums.AttrPyValueType:
        """Returns the attribute value for a key.

        The type of the attribute determines the number of elements in the key and
        return type. E.g. when attr=DoubleAttr1.VARIABLE_LOWER_BOUND, key should
        have size one (the element id of the variable) and the return type is float.

        Args:
          attr: The attribute to query, which implies the key size and return type.
          key: A sequence of k ints, the element ids of the key.

        Returns:
          The value for the key, or the default value for the attribute if the key
          is not set.

        Raises:
          ValueError: if key is of the wrong size or key refers to an element id
            is not in the model.
        """

    def get_attrs(
        self,
        attr: enums.Attr[enums.AttrValueType],
        keys: np.typing.NDArray[np.int64],
    ) -> np.typing.NDArray[enums.AttrValueType]:
        """Returns the values of an attribute for a list of keys.

        Repeated keys are okay.

        Args:
          attr: The attribute to query, with k elements in each key.
          keys: An (n, k) array of n keys to read.

        Returns:
          An array with shape (n,) with the values for each key. The default value
          of the attribute is returned if it was never set for the key.

        Raises:
          ValueError: if (1) any key references an element not in the model or (2)
            the shape of keys is invalid.
        """

    def clear_attr(self, attr: enums.AnyAttr) -> None:
        """Restores an attribute to its default value for every key."""

    def is_attr_non_default(self, attr: enums.AnyAttr, key: Sequence[int]) -> bool:
        """Returns true if the attribute has a non-default value for key."""

    def bulk_is_attr_non_default(
        self, attr: enums.AnyAttr, keys: np.typing.NDArray[np.int64]
    ) -> np.typing.NDArray[np.bool_]:
        """Returns which keys take a value different from the attribute's default.

        Repeated keys are okay.

        Args:
          attr: The attribute to query, with k elements in each key.
          keys: An (n, k) array to of n keys to query.

        Returns:
          An array with shape (n,), for each key, if it had a non-default value.

        Raises:
          ValueError: if (1) any key references an element not in the model or (2)
            the shape of keys is invalid.
        """

    def get_attr_num_non_defaults(self, attr: enums.AnyAttr) -> int:
        """Returns the number of keys with a non-default value for an attribute."""

    def get_attr_non_defaults(self, attr: enums.AnyAttr) -> np.typing.NDArray[np.int64]:
        """Returns the keys with a non-default value for an attribute.

        Args:
          attr: The attribute to query, with k elements in each key.

        Returns:
          An array with shape (n, k) if there are n keys with a non-default value
          for this attribute.
        """

    def slice_attr(
        self, attr: enums.AnyAttr, key_index: int, element_id: int
    ) -> np.typing.NDArray[np.int64]:
        """Returns the keys with a non-default value for an attribute along a slice.

        Args:
          attr: The attribute to query, with k elements in each key.
          key_index: The index of the key to slice on, in [0..k).
          element_id: The value of the key to slice on, must be the id of an element
            with type given by the `key_index` key for `attr`.

        Returns:
          An array with shape (n, k) if there are n keys along the slice with a
          non-default value for this attribute.

        Raises:
          ValueError: if (1) `key_index` is not in [0..k) or (2) if no element with
            `element_id` exists.
        """

    def get_attr_slice_size(
        self, attr: enums.AnyAttr, key_index: int, element_id: int
    ) -> int:
        """Returns the number of keys in slice_attr(attr, key_index, element_id)."""

    def export_model(self, *, remove_names: bool = False) -> model_pb2.ModelProto:
        """Returns a ModelProto equivalent to this model.

        Args:
          remove_names: If True, exclude names (e.g. variable names, the model name)
            from the returned proto.

        Returns:
          The equivalent ModelProto.

        Raises:
          ValueError: if the model is too big to fit into the proto, see class
            description for details.
        """

    def add_diff(self) -> int:
        """Creates a new Diff to track changes to the model and returns its id."""

    def delete_diff(self, diff_id: int) -> None:
        """Stop tracking changes to the model for the Diff with id `diff_id`."""

    def advance_diff(self, diff_id: int) -> None:
        """Discards any previously tracked changes for this Diff.

        The diff will now track changes from the point onward.

        Args:
          diff_id: The id of to the Diff to advance.

        Raises:
          ValueError: if diff_id does not reference a Diff for this model (e.g.,
            the Diff was already deleted).
        """

    def export_model_update(
        self, diff_id: int, *, remove_names: bool = False
    ) -> Optional[model_update_pb2.ModelUpdateProto]:
        """Returns a ModelUpdateProto with the changes for the Diff `diff_id`.

        Args:
          diff_id: The id of the Diff to get changes for.
          remove_names: If True, exclude names (e.g. variable names, the model name)
            from the returned proto.

        Returns:
          All changes to the model since the most recent call to
          `advance(diff_id)`, or since the Diff was created if it was never
          advanced. Returns `None` instead of an empty proto when there are no
          changes.

        Raises:
          ValueError: if the update is too big to fit into the proto (see class
            description for details) or if diff_id does not reference a Diff for
            this model (e.g., the id was already deleted).
        """

    def apply_update(self, update_proto: model_update_pb2.ModelUpdateProto) -> None:
        """Modifies the model to apply the changes from `update_proto`.

        Args:
          update_proto: the changes to apply to the model.

        Raises:
          ValueError: if the update proto is invalid for the current model, or if
           the implementation must serialize the proto and it is too large (see
           class description).
        """
