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

from typing import Any, Iterator, Optional

from ortools.math_opt.elemental.python import enums
from ortools.math_opt.python import from_model
from ortools.math_opt.python import variables
from ortools.math_opt.python.elemental import elemental


class IndicatorConstraint(from_model.FromModel):
    """An indicator constraint for an optimization model.

    An IndicatorConstraint adds the following restriction on feasible solutions to
    an optimization model:
      if z == 1 then lb <= sum_{i in I} a_i * x_i <= ub
    where z is a binary decision variable (or its negation) and x_i are the
    decision variables of the problem. Equality constraints lb == ub is allowed,
    which models the constraint:
      if z == 1 then sum_{i in I} a_i * x_i == b
    Setting lb > ub will result in an InvalidArgument error at solve time.

    Indicator constraints have limited mutability. You can delete a variable
    that the constraint uses, or you can delete the entire constraint. You
    currently cannot update bounds or coefficients. This may change in future
    versions.

    If the indicator variable is deleted or was None at creation time, the
    constraint will lead to an invalid model at solve time, unless the constraint
    is deleted before solving.

    The name is optional, read only, and used only for debugging. Non-empty names
    should be distinct.

    Do not create an IndicatorConstraint directly, use
    Model.add_indicator_constraint() instead. Two IndicatorConstraint objects can
    represent the same constraint (for the same model). They will have the same
    underlying IndicatorConstraint.elemental for storing the data. The
    IndicatorConstraint class is simply a reference to an Elemental.
    """

    __slots__ = "_elemental", "_id"

    def __init__(self, elem: elemental.Elemental, cid: int) -> None:
        """Internal only, prefer Model functions (add_indicator_constraint() and get_indicator_constraint())."""
        if not isinstance(cid, int):
            raise TypeError(f"cid type should be int, was:{type(cid).__name__!r}")
        self._elemental: elemental.Elemental = elem
        self._id: int = cid

    @property
    def lower_bound(self) -> float:
        return self._elemental.get_attr(
            enums.DoubleAttr1.INDICATOR_CONSTRAINT_LOWER_BOUND, (self._id,)
        )

    @property
    def upper_bound(self) -> float:
        return self._elemental.get_attr(
            enums.DoubleAttr1.INDICATOR_CONSTRAINT_UPPER_BOUND, (self._id,)
        )

    @property
    def activate_on_zero(self) -> bool:
        return self._elemental.get_attr(
            enums.BoolAttr1.INDICATOR_CONSTRAINT_ACTIVATE_ON_ZERO, (self._id,)
        )

    @property
    def indicator_variable(self) -> Optional[variables.Variable]:
        var_id = self._elemental.get_attr(
            enums.VariableAttr1.INDICATOR_CONSTRAINT_INDICATOR, (self._id,)
        )
        if var_id < 0:
            return None
        return variables.Variable(self._elemental, var_id)

    @property
    def name(self) -> str:
        return self._elemental.get_element_name(
            enums.ElementType.INDICATOR_CONSTRAINT, self._id
        )

    @property
    def id(self) -> int:
        return self._id

    @property
    def elemental(self) -> elemental.Elemental:
        """Internal use only."""
        return self._elemental

    def get_coefficient(self, var: variables.Variable) -> float:
        from_model.model_is_same(var, self)
        return self._elemental.get_attr(
            enums.DoubleAttr2.INDICATOR_CONSTRAINT_LINEAR_COEFFICIENT,
            (self._id, var.id),
        )

    def terms(self) -> Iterator[variables.LinearTerm]:
        """Yields the variable/coefficient pairs with nonzero coefficient for this linear constraint."""
        keys = self._elemental.slice_attr(
            enums.DoubleAttr2.INDICATOR_CONSTRAINT_LINEAR_COEFFICIENT, 0, self._id
        )
        coefs = self._elemental.get_attrs(
            enums.DoubleAttr2.INDICATOR_CONSTRAINT_LINEAR_COEFFICIENT, keys
        )
        for i in range(len(keys)):
            yield variables.LinearTerm(
                variable=variables.Variable(self._elemental, int(keys[i, 1])),
                coefficient=float(coefs[i]),
            )

    def get_implied_constraint(self) -> variables.BoundedLinearExpression:
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
        if isinstance(other, IndicatorConstraint):
            return self._id == other._id and self._elemental is other._elemental
        return False

    def __hash__(self) -> int:
        return hash(self._id)
