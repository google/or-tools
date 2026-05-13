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

"""Bounded (above and below), upper bounded, and lower bounded expressions."""

import math
from typing import Any, Generic, NoReturn, Optional, Type, TypeVar

_CHAINED_COMPARISON_MESSAGE = (
    "If you were trying to create a two-sided or "
    "ranged linear inequality of the form `lb <= "
    "expr <= ub`, try `(lb <= expr) <= ub` instead"
)


def _raise_binary_operator_type_error(
    operator: str,
    lhs: Type[Any],
    rhs: Type[Any],
    extra_message: Optional[str] = None,
) -> NoReturn:
    """Raises TypeError on unsupported operators."""
    message = (
        f"unsupported operand type(s) for {operator}: {lhs.__name__!r} and"
        f" {rhs.__name__!r}"
    )
    if extra_message is not None:
        message += "\n" + extra_message
    raise TypeError(message)


T = TypeVar("T")


class BoundedExpression(Generic[T]):
    """An inequality of the form lower_bound <= expression <= upper_bound.

    Where:
     * expression is a T, typically LinearBase or QuadraticBase.
     * lower_bound is a float.
     * upper_bound is a float.

    Note: Because of limitations related to Python's handling of chained
    comparisons, bounded expressions cannot be directly created usign
    overloaded comparisons as in `lower_bound <= expression <= upper_bound`.
    One solution is to wrap one of the inequalities in parenthesis as in
    `(lower_bound <= expression) <= upper_bound`.
    """

    __slots__ = "_expression", "_lower_bound", "_upper_bound"

    def __init__(self, lower_bound: float, expression: T, upper_bound: float) -> None:
        self._expression: T = expression
        self._lower_bound: float = lower_bound
        self._upper_bound: float = upper_bound

    @property
    def expression(self) -> T:
        return self._expression

    @property
    def lower_bound(self) -> float:
        return self._lower_bound

    @property
    def upper_bound(self) -> float:
        return self._upper_bound

    def __bool__(self) -> bool:
        raise TypeError(
            "__bool__ is unsupported for BoundedExpression"
            + "\n"
            + _CHAINED_COMPARISON_MESSAGE
        )

    def __str__(self):
        return f"{self._lower_bound} <= {self._expression!s} <= {self._upper_bound}"

    def __repr__(self):
        return f"{self._lower_bound} <= {self._expression!r} <= {self._upper_bound}"


class UpperBoundedExpression(Generic[T]):
    """An inequality of the form expression <= upper_bound.

    Where:
     * expression is a T, and
     * upper_bound is a float
    """

    __slots__ = "_expression", "_upper_bound"

    def __init__(self, expression: T, upper_bound: float) -> None:
        """Operator overloading can be used instead: e.g. `x + y <= 2.0`."""
        self._expression: T = expression
        self._upper_bound: float = upper_bound

    @property
    def expression(self) -> T:
        return self._expression

    @property
    def lower_bound(self) -> float:
        return -math.inf

    @property
    def upper_bound(self) -> float:
        return self._upper_bound

    def __ge__(self, lhs: float) -> BoundedExpression[T]:
        if isinstance(lhs, (int, float)):
            return BoundedExpression[T](lhs, self.expression, self.upper_bound)
        _raise_binary_operator_type_error(">=", type(self), type(lhs))

    def __bool__(self) -> bool:
        raise TypeError(
            "__bool__ is unsupported for UpperBoundedExpression"
            + "\n"
            + _CHAINED_COMPARISON_MESSAGE
        )

    def __str__(self):
        return f"{self._expression!s} <= {self._upper_bound}"

    def __repr__(self):
        return f"{self._expression!r} <= {self._upper_bound}"


class LowerBoundedExpression(Generic[T]):
    """An inequality of the form expression >= lower_bound.

    Where:
     * expression is a linear expression, and
     * lower_bound is a float
    """

    __slots__ = "_expression", "_lower_bound"

    def __init__(self, expression: T, lower_bound: float) -> None:
        """Operator overloading can be used instead: e.g. `x + y >= 2.0`."""
        self._expression: T = expression
        self._lower_bound: float = lower_bound

    @property
    def expression(self) -> T:
        return self._expression

    @property
    def lower_bound(self) -> float:
        return self._lower_bound

    @property
    def upper_bound(self) -> float:
        return math.inf

    def __le__(self, rhs: float) -> BoundedExpression[T]:
        if isinstance(rhs, (int, float)):
            return BoundedExpression[T](self.lower_bound, self.expression, rhs)
        _raise_binary_operator_type_error("<=", type(self), type(rhs))

    def __bool__(self) -> bool:
        raise TypeError(
            "__bool__ is unsupported for LowerBoundedExpression"
            + "\n"
            + _CHAINED_COMPARISON_MESSAGE
        )

    def __str__(self):
        return f"{self._expression!s} >= {self._lower_bound}"

    def __repr__(self):
        return f"{self._expression!r} >= {self._lower_bound}"
