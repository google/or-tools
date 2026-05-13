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

"""Quadratic constraint in a model."""

from typing import Any, Iterator

from ortools.math_opt.elemental.python import enums
from ortools.math_opt.python import from_model
from ortools.math_opt.python import variables
from ortools.math_opt.python.elemental import elemental


class QuadraticConstraint(from_model.FromModel):
    """A quadratic constraint for an optimization model.

    A QuadraticConstraint adds the following restriction on feasible solutions to
    an optimization model:
      lb <= sum_i a_i x_i + sum_i sum_{j : i <= j} b_ij x_i x_j <= ub
    where x_i are the decision variables of the problem. lb == ub is allowed, and
    this models an equality constraint. lb > ub is also allowed, but the
    optimization problem will be infeasible.

    Quadratic constraints have limited mutability. You can delete a variable
    that the constraint uses, or you can delete the entire constraint. You
    currently cannot update bounds or coefficients. This may change in future
    versions.

    A QuadraticConstraint can be queried as follows:
      * lower_bound: a float property, lb above. Should not be NaN nor +inf.
      * upper_bound: a float property, ub above. Should not be NaN nor -inf.
      * get_linear_coefficient(): get the a_i * x_i terms. The variable must be
        from the same model as this constraint, and the a_i must be finite and not
        NaN. The coefficient for any variable not set is 0.0.
      * get_quadratic_coefficient(): like get_linear_coefficient() but for the
        b_ij terms. Note that get_quadratic_coefficient(x, y, 8) and
        get_quadratic_coefficient(y, x, 8) have the same result.

    The name is optional, read only, and used only for debugging. Non-empty names
    should be distinct.

    Do not create a QuadraticConstraint directly, use
    Model.add_quadratic_constraint() instead. Two QuadraticConstraint objects
    can represent the same constraint (for the same model). They will have the
    same underlying QuadraticConstraint.elemental for storing the data. The
    QuadraticConstraint class is simply a reference to an Elemental.
    """

    __slots__ = "_elemental", "_id"

    def __init__(self, elem: elemental.Elemental, cid: int) -> None:
        """Internal only, prefer Model functions (add_quadratic_constraint() and get_quadratic_constraint())."""
        if not isinstance(cid, int):
            raise TypeError(f"cid type should be int, was:{type(cid).__name__!r}")
        self._elemental: elemental.Elemental = elem
        self._id: int = cid

    @property
    def lower_bound(self) -> float:
        """The quadratic expression of the constraint must be at least this."""
        return self._elemental.get_attr(
            enums.DoubleAttr1.QUADRATIC_CONSTRAINT_LOWER_BOUND, (self._id,)
        )

    @property
    def upper_bound(self) -> float:
        """The quadratic expression of the constraint must be at most this."""
        return self._elemental.get_attr(
            enums.DoubleAttr1.QUADRATIC_CONSTRAINT_UPPER_BOUND, (self._id,)
        )

    @property
    def name(self) -> str:
        """The name of this constraint."""
        return self._elemental.get_element_name(
            enums.ElementType.QUADRATIC_CONSTRAINT, self._id
        )

    @property
    def id(self) -> int:
        """A unique (for the model) identifier for this constraint."""
        return self._id

    @property
    def elemental(self) -> elemental.Elemental:
        """Internal use only."""
        return self._elemental

    def get_linear_coefficient(self, var: variables.Variable) -> float:
        """Returns the linear coefficient for var in the constraint's quadratic expression."""
        from_model.model_is_same(var, self)
        return self._elemental.get_attr(
            enums.DoubleAttr2.QUADRATIC_CONSTRAINT_LINEAR_COEFFICIENT,
            (self._id, var.id),
        )

    def linear_terms(self) -> Iterator[variables.LinearTerm]:
        """Yields variable/coefficient pairs from the linear part of the constraint.

        Only the pairs with nonzero coefficient are returned.

        Yields:
          The variable, coefficient pairs.
        """
        keys = self._elemental.slice_attr(
            enums.DoubleAttr2.QUADRATIC_CONSTRAINT_LINEAR_COEFFICIENT, 0, self._id
        )
        coefs = self._elemental.get_attrs(
            enums.DoubleAttr2.QUADRATIC_CONSTRAINT_LINEAR_COEFFICIENT, keys
        )
        for i in range(len(keys)):
            yield variables.LinearTerm(
                variable=variables.Variable(self._elemental, int(keys[i, 1])),
                coefficient=float(coefs[i]),
            )

    def get_quadratic_coefficient(
        self, var1: variables.Variable, var2: variables.Variable
    ) -> float:
        """Returns the quadratic coefficient for the pair (var1, var2) in the constraint's quadratic expression."""
        from_model.model_is_same(var1, self)
        from_model.model_is_same(var2, self)
        return self._elemental.get_attr(
            enums.SymmetricDoubleAttr3.QUADRATIC_CONSTRAINT_QUADRATIC_COEFFICIENT,
            (self._id, var1.id, var2.id),
        )

    def quadratic_terms(self) -> Iterator[variables.QuadraticTerm]:
        """Yields variable/coefficient pairs from the quadratic part of the constraint.

        Only the pairs with nonzero coefficient are returned.

        Yields:
          The variable, coefficient pairs.
        """
        keys = self._elemental.slice_attr(
            enums.SymmetricDoubleAttr3.QUADRATIC_CONSTRAINT_QUADRATIC_COEFFICIENT,
            0,
            self._id,
        )
        coefs = self._elemental.get_attrs(
            enums.SymmetricDoubleAttr3.QUADRATIC_CONSTRAINT_QUADRATIC_COEFFICIENT,
            keys,
        )
        for i in range(len(keys)):
            yield variables.QuadraticTerm(
                variables.QuadraticTermKey(
                    variables.Variable(self._elemental, int(keys[i, 1])),
                    variables.Variable(self._elemental, int(keys[i, 2])),
                ),
                coefficient=float(coefs[i]),
            )

    def __str__(self):
        """Returns the name, or a string containing the id if the name is empty."""
        return self.name if self.name else f"quadratic_constraint_{self.id}"

    def __repr__(self):
        return f"<QuadraticConstraint id: {self.id}, name: {self.name!r}>"

    def __eq__(self, other: Any) -> bool:
        if isinstance(other, QuadraticConstraint):
            return self._id == other._id and self._elemental is other._elemental
        return False

    def __hash__(self) -> int:
        return hash((self._id, self._elemental))
