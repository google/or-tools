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

"""Data structures for linear and quadratic constraints.

In contrast to BoundedLinearExpression and related structures, there is no
offset inside the inequality.

This is not part of the MathOpt public API, do not depend on it externally.
"""

import dataclasses
import math
from typing import Mapping, Optional, Union

from ortools.math_opt.python import bounded_expressions
from ortools.math_opt.python import variables

_BoundedLinearExpressions = (
    variables.LowerBoundedLinearExpression,
    variables.UpperBoundedLinearExpression,
    variables.BoundedLinearExpression,
)

_BoundedQuadraticExpressions = (
    variables.LowerBoundedLinearExpression,
    variables.UpperBoundedLinearExpression,
    variables.BoundedLinearExpression,
    variables.LowerBoundedQuadraticExpression,
    variables.UpperBoundedQuadraticExpression,
    variables.BoundedQuadraticExpression,
)

_BoundedExpressions = (
    bounded_expressions.LowerBoundedExpression,
    bounded_expressions.UpperBoundedExpression,
    bounded_expressions.BoundedExpression,
)


def _bool_error() -> TypeError:
    return TypeError(
        "Unsupported type for bounded_expr argument:"
        " bool. This error can occur when trying to add != constraints "
        "(which are not supported) or inequalities/equalities with constant "
        "left-hand-side and right-hand-side (which are redundant or make a "
        "model infeasible)."
    )


@dataclasses.dataclass
class NormalizedLinearInequality:
    """Represents an inequality lb <= expr <= ub where expr's offset is zero."""

    lb: float
    ub: float
    coefficients: Mapping[variables.Variable, float]

    def __init__(
        self,
        *,
        lb: Optional[float],
        ub: Optional[float],
        expr: Optional[variables.LinearTypes],
    ) -> None:
        """Raises a ValueError if expr's offset is infinite."""
        lb = -math.inf if lb is None else lb
        ub = math.inf if ub is None else ub
        expr = 0.0 if expr is None else expr
        if not isinstance(expr, (int, float, variables.LinearBase)):
            raise TypeError(
                f"Unsupported type for expr argument: {type(expr).__name__!r}."
            )

        flat_expr = variables.as_flat_linear_expression(expr)
        if math.isinf(flat_expr.offset):
            raise ValueError(
                "Trying to create a linear constraint whose expression has an"
                " infinite offset."
            )
        self.lb = lb - flat_expr.offset
        self.ub = ub - flat_expr.offset
        self.coefficients = flat_expr.terms


def _normalize_bounded_linear_expression(
    bounded_expr: variables.BoundedLinearTypes,
) -> NormalizedLinearInequality:
    """Converts a bounded linear expression into a NormalizedLinearInequality."""
    if isinstance(bounded_expr, variables.VarEqVar):
        return NormalizedLinearInequality(
            lb=0.0,
            ub=0.0,
            expr=bounded_expr.first_variable - bounded_expr.second_variable,
        )
    elif isinstance(bounded_expr, _BoundedExpressions):
        if isinstance(bounded_expr.expression, (int, float, variables.LinearBase)):
            return NormalizedLinearInequality(
                lb=bounded_expr.lower_bound,
                ub=bounded_expr.upper_bound,
                expr=bounded_expr.expression,
            )
        else:
            raise TypeError(
                "Bad type of expression in bounded_expr:"
                f" {type(bounded_expr.expression).__name__!r}."
            )
    else:
        raise TypeError(f"bounded_expr has bad type: {type(bounded_expr).__name__!r}.")


# TODO(b/227214976): Update the note below and link to pytype bug number.
# Note: bounded_expr's type includes bool only as a workaround to a pytype
# issue. Passing a bool for bounded_expr will raise an error in runtime.
def as_normalized_linear_inequality(
    bounded_expr: Optional[Union[bool, variables.BoundedLinearTypes]] = None,
    *,
    lb: Optional[float] = None,
    ub: Optional[float] = None,
    expr: Optional[variables.LinearTypes] = None,
) -> NormalizedLinearInequality:
    """Builds a NormalizedLinearInequality.

    If bounded_expr is not None, then all other arguments must be None.

    If expr has a nonzero offset, it will be subtracted from both lb and ub.

    When bounded_expr is unset and a named argument is unset, we use the defaults:
      * lb: -math.inf
      * ub: math.inf
      * expr: 0

    Args:
      bounded_expr: a linear inequality describing the constraint.
      lb: The lower bound when bounded_expr is None.
      ub: The upper bound if bounded_expr is None.
      expr: The expression when bounded_expr is None.

    Returns:
      A NormalizedLinearInequality representing the linear constraint.
    """
    if isinstance(bounded_expr, bool):
        raise _bool_error()
    if bounded_expr is not None:
        if lb is not None:
            raise AssertionError(
                "lb cannot be specified when bounded_expr is not None."
            )
        if ub is not None:
            raise AssertionError(
                "ub cannot be specified when bounded_expr is not None."
            )
        if expr is not None:
            raise AssertionError(
                "expr cannot be specified when bounded_expr is not None"
            )
        return _normalize_bounded_linear_expression(bounded_expr)
    # Note: NormalizedLinearInequality() will runtime check the type of expr.
    return NormalizedLinearInequality(lb=lb, ub=ub, expr=expr)


@dataclasses.dataclass
class NormalizedQuadraticInequality:
    """Represents an inequality lb <= expr <= ub where expr's offset is zero."""

    lb: float
    ub: float
    linear_coefficients: Mapping[variables.Variable, float]
    quadratic_coefficients: Mapping[variables.QuadraticTermKey, float]

    def __init__(
        self,
        *,
        lb: Optional[float] = None,
        ub: Optional[float] = None,
        expr: Optional[variables.QuadraticTypes] = None,
    ) -> None:
        """Raises a ValueError if expr's offset is infinite."""
        lb = -math.inf if lb is None else lb
        ub = math.inf if ub is None else ub
        expr = 0.0 if expr is None else expr
        if not isinstance(
            expr, (int, float, variables.LinearBase, variables.QuadraticBase)
        ):
            raise TypeError(
                f"Unsupported type for expr argument: {type(expr).__name__!r}."
            )
        flat_expr = variables.as_flat_quadratic_expression(expr)
        if math.isinf(flat_expr.offset):
            raise ValueError(
                "Trying to create a quadratic constraint whose expression has an"
                " infinite offset."
            )
        self.lb = lb - flat_expr.offset
        self.ub = ub - flat_expr.offset
        self.linear_coefficients = flat_expr.linear_terms
        self.quadratic_coefficients = flat_expr.quadratic_terms


def _normalize_bounded_quadratic_expression(
    bounded_expr: Union[variables.BoundedQuadraticTypes, variables.BoundedLinearTypes],
) -> NormalizedQuadraticInequality:
    """Converts a bounded quadratic expression into a NormalizedQuadraticInequality."""
    if isinstance(bounded_expr, variables.VarEqVar):
        return NormalizedQuadraticInequality(
            lb=0.0,
            ub=0.0,
            expr=bounded_expr.first_variable - bounded_expr.second_variable,
        )
    elif isinstance(bounded_expr, _BoundedExpressions):
        if isinstance(
            bounded_expr.expression,
            (int, float, variables.LinearBase, variables.QuadraticBase),
        ):
            return NormalizedQuadraticInequality(
                lb=bounded_expr.lower_bound,
                ub=bounded_expr.upper_bound,
                expr=bounded_expr.expression,
            )
        else:
            raise TypeError(
                "bounded_expr.expression has bad type:"
                f" {type(bounded_expr.expression).__name__!r}."
            )
    else:
        raise TypeError(f"bounded_expr has bad type: {type(bounded_expr).__name__!r}.")


# TODO(b/227214976): Update the note below and link to pytype bug number.
# Note: bounded_expr's type includes bool only as a workaround to a pytype
# issue. Passing a bool for bounded_expr will raise an error in runtime.
def as_normalized_quadratic_inequality(
    bounded_expr: Optional[
        Union[bool, variables.BoundedLinearTypes, variables.BoundedQuadraticTypes]
    ] = None,
    *,
    lb: Optional[float] = None,
    ub: Optional[float] = None,
    expr: Optional[variables.QuadraticTypes] = None,
) -> NormalizedQuadraticInequality:
    """Builds a NormalizedLinearInequality.

    If bounded_expr is not None, then all other arguments must be None.

    If expr has a nonzero offset, it will be subtracted from both lb and ub.

    When bounded_expr is unset and a named argument is unset, we use the defaults:
      * lb: -math.inf
      * ub: math.inf
      * expr: 0

    Args:
      bounded_expr: a quadratic inequality describing the constraint.
      lb: The lower bound when bounded_expr is None.
      ub: The upper bound if bounded_expr is None.
      expr: The expression when bounded_expr is None.

    Returns:
      A NormalizedLinearInequality representing the linear constraint.
    """
    if isinstance(bounded_expr, bool):
        raise _bool_error()
    if bounded_expr is not None:
        if lb is not None:
            raise AssertionError(
                "lb cannot be specified when bounded_expr is not None."
            )
        if ub is not None:
            raise AssertionError(
                "ub cannot be specified when bounded_expr is not None."
            )
        if expr is not None:
            raise AssertionError(
                "expr cannot be specified when bounded_expr is not None"
            )
        return _normalize_bounded_quadratic_expression(bounded_expr)
    return NormalizedQuadraticInequality(lb=lb, ub=ub, expr=expr)
