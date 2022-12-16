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

"""Linear constraint in a model."""

from typing import Any, Iterator, NamedTuple

from ortools.math_opt.elemental.python import enums
from ortools.math_opt.python import from_model
from ortools.math_opt.python import variables
from ortools.math_opt.python.elemental import elemental


class LinearConstraint(from_model.FromModel):
    """A linear constraint for an optimization model.

    A LinearConstraint adds the following restriction on feasible solutions to an
    optimization model:
      lb <= sum_{i in I} a_i * x_i <= ub
    where x_i are the decision variables of the problem. lb == ub is allowed, this
    models the equality constraint:
      sum_{i in I} a_i * x_i == b
    Setting lb > ub will result in an InvalidArgument error at solve time (the
    values are allowed to cross temporarily between solves).

    A LinearConstraint can be configured as follows:
      * lower_bound: a float property, lb above. Should not be NaN nor +inf.
      * upper_bound: a float property, ub above. Should not be NaN nor -inf.
      * set_coefficient() and get_coefficient(): get and set the a_i * x_i
        terms. The variable must be from the same model as this constraint, and
        the a_i must be finite and not NaN. The coefficient for any variable not
        set is 0.0, and setting a coefficient to 0.0 removes it from I above.

    The name is optional, read only, and used only for debugging. Non-empty names
    should be distinct.

    Do not create a LinearConstraint directly, use Model.add_linear_constraint()
    instead. Two LinearConstraint objects can represent the same constraint (for
    the same model). They will have the same underlying LinearConstraint.elemental
    for storing the data. The LinearConstraint class is simply a reference to an
    Elemental.
    """

    __slots__ = "_elemental", "_id"

    def __init__(self, elem: elemental.Elemental, cid: int) -> None:
        """Internal only, prefer Model functions (add_linear_constraint() and get_linear_constraint())."""
        if not isinstance(cid, int):
            raise TypeError(f"cid type should be int, was:{type(cid).__name__!r}")
        self._elemental: elemental.Elemental = elem
        self._id: int = cid

    @property
    def lower_bound(self) -> float:
        return self._elemental.get_attr(
            enums.DoubleAttr1.LINEAR_CONSTRAINT_LOWER_BOUND, (self._id,)
        )

    @lower_bound.setter
    def lower_bound(self, value: float) -> None:
        self._elemental.set_attr(
            enums.DoubleAttr1.LINEAR_CONSTRAINT_LOWER_BOUND, (self._id,), value
        )

    @property
    def upper_bound(self) -> float:
        return self._elemental.get_attr(
            enums.DoubleAttr1.LINEAR_CONSTRAINT_UPPER_BOUND, (self._id,)
        )

    @upper_bound.setter
    def upper_bound(self, value: float) -> None:
        self._elemental.set_attr(
            enums.DoubleAttr1.LINEAR_CONSTRAINT_UPPER_BOUND, (self._id,), value
        )

    @property
    def name(self) -> str:
        return self._elemental.get_element_name(
            enums.ElementType.LINEAR_CONSTRAINT, self._id
        )

    @property
    def id(self) -> int:
        return self._id

    @property
    def elemental(self) -> elemental.Elemental:
        """Internal use only."""
        return self._elemental

    def set_coefficient(self, var: variables.Variable, coefficient: float) -> None:
        from_model.model_is_same(var, self)
        self._elemental.set_attr(
            enums.DoubleAttr2.LINEAR_CONSTRAINT_COEFFICIENT,
            (self._id, var.id),
            coefficient,
        )

    def get_coefficient(self, var: variables.Variable) -> float:
        from_model.model_is_same(var, self)
        return self._elemental.get_attr(
            enums.DoubleAttr2.LINEAR_CONSTRAINT_COEFFICIENT, (self._id, var.id)
        )

    def terms(self) -> Iterator[variables.LinearTerm]:
        """Yields the variable/coefficient pairs with nonzero coefficient for this linear constraint."""
        keys = self._elemental.slice_attr(
            enums.DoubleAttr2.LINEAR_CONSTRAINT_COEFFICIENT, 0, self._id
        )
        coefs = self._elemental.get_attrs(
            enums.DoubleAttr2.LINEAR_CONSTRAINT_COEFFICIENT, keys
        )
        for i in range(len(keys)):
            yield variables.LinearTerm(
                variable=variables.Variable(self._elemental, int(keys[i, 1])),
                coefficient=float(coefs[i]),
            )

    def as_bounded_linear_expression(self) -> variables.BoundedLinearExpression:
        """Returns the bounded expression from lower_bound, upper_bound and terms."""
        return variables.BoundedLinearExpression(
            self.lower_bound, variables.LinearSum(self.terms()), self.upper_bound
        )

    def __str__(self):
        """Returns the name, or a string containing the id if the name is empty."""
        return self.name if self.name else f"linear_constraint_{self.id}"

    def __repr__(self):
        return f"<LinearConstraint id: {self.id}, name: {self.name!r}>"

    def __eq__(self, other: Any) -> bool:
        if isinstance(other, LinearConstraint):
            return self._id == other._id and self._elemental is other._elemental
        return False

    def __hash__(self) -> int:
        return hash(self._id)


class LinearConstraintMatrixEntry(NamedTuple):
    linear_constraint: LinearConstraint
    variable: variables.Variable
    coefficient: float
