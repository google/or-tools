# Copyright 2010-2024 Google LLC
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

"""Methods for building and solving model_builder models.

The following two sections describe the main
methods for building and solving those models.

* [`Model`](#model_builder.Model): Methods for creating
models, including variables and constraints.
* [`Solver`](#model_builder.Solver): Methods for solving
a model and evaluating solutions.

Additional methods for solving Model models:

* [`Constraint`](#model_builder.Constraint): A few utility methods for modifying
  constraints created by `Model`.
* [`LinearExpr`](#model_builder.LinearExpr): Methods for creating constraints
  and the objective from large arrays of coefficients.

Other methods and functions listed are primarily used for developing OR-Tools,
rather than for solving specific optimization problems.
"""

import abc
import dataclasses
import math
import numbers
import typing
from typing import Callable, List, Optional, Sequence, Tuple, Union, cast

import numpy as np
from numpy import typing as npt
import pandas as pd

from ortools.linear_solver import linear_solver_pb2
from ortools.linear_solver.python import model_builder_helper as mbh
from ortools.linear_solver.python import model_builder_numbers as mbn


# Custom types.
NumberT = Union[int, float, numbers.Real, np.number]
IntegerT = Union[int, numbers.Integral, np.integer]
LinearExprT = Union["LinearExpr", NumberT]
ConstraintT = Union["_BoundedLinearExpr", bool]
_IndexOrSeries = Union[pd.Index, pd.Series]
_VariableOrConstraint = Union["LinearConstraint", "Variable"]

# Forward solve statuses.
SolveStatus = mbh.SolveStatus

# pylint: disable=protected-access


class LinearExpr(metaclass=abc.ABCMeta):
    """Holds an linear expression.

    A linear expression is built from constants and variables.
    For example, `x + 2.0 * (y - z + 1.0)`.

    Linear expressions are used in Model models in constraints and in the
    objective:

    * You can define linear constraints as in:

    ```
    model.add(x + 2 * y <= 5.0)
    model.add(sum(array_of_vars) == 5.0)
    ```

    * In Model, the objective is a linear expression:

    ```
    model.minimize(x + 2.0 * y + z)
    ```

    * For large arrays, using the LinearExpr class is faster that using the python
    `sum()` function. You can create constraints and the objective from lists of
    linear expressions or coefficients as follows:

    ```
    model.minimize(model_builder.LinearExpr.sum(expressions))
    model.add(model_builder.LinearExpr.weighted_sum(expressions, coeffs) >= 0)
    ```
    """

    @classmethod
    def sum(  # pytype: disable=annotation-type-mismatch  # numpy-scalars
        cls, expressions: Sequence[LinearExprT], *, constant: NumberT = 0.0
    ) -> LinearExprT:
        """Creates `sum(expressions) + constant`.

        It can perform simple simplifications and returns different objects,
        including the input.

        Args:
          expressions: a sequence of linear expressions or constants.
          constant: a numerical constant.

        Returns:
          a LinearExpr instance or a numerical constant.
        """
        checked_constant: np.double = mbn.assert_is_a_number(constant)
        if not expressions:
            return checked_constant
        if len(expressions) == 1 and mbn.is_zero(checked_constant):
            return expressions[0]

        return LinearExpr.weighted_sum(
            expressions, np.ones(len(expressions)), constant=checked_constant
        )

    @classmethod
    def weighted_sum(  # pytype: disable=annotation-type-mismatch  # numpy-scalars
        cls,
        expressions: Sequence[LinearExprT],
        coefficients: Sequence[NumberT],
        *,
        constant: NumberT = 0.0,
    ) -> Union[NumberT, "_LinearExpression"]:
        """Creates `sum(expressions[i] * coefficients[i]) + constant`.

        It can perform simple simplifications and returns different object,
        including the input.

        Args:
          expressions: a sequence of linear expressions or constants.
          coefficients: a sequence of numerical constants.
          constant: a numerical constant.

        Returns:
          a _LinearExpression instance or a numerical constant.
        """
        if len(expressions) != len(coefficients):
            raise ValueError(
                "LinearExpr.weighted_sum: expressions and coefficients have"
                " different lengths"
            )
        checked_constant: np.double = mbn.assert_is_a_number(constant)
        if not expressions:
            return checked_constant
        return _sum_as_flat_linear_expression(
            to_process=list(zip(expressions, coefficients)), offset=checked_constant
        )

    @classmethod
    def term(  # pytype: disable=annotation-type-mismatch  # numpy-scalars
        cls,
        expression: LinearExprT,
        coefficient: NumberT,
        *,
        constant: NumberT = 0.0,
    ) -> LinearExprT:
        """Creates `expression * coefficient + constant`.

        It can perform simple simplifications and returns different object,
        including the input.
        Args:
          expression: a linear expression or a constant.
          coefficient: a numerical constant.
          constant: a numerical constant.

        Returns:
          a LinearExpr instance or a numerical constant.
        """
        checked_coefficient: np.double = mbn.assert_is_a_number(coefficient)
        checked_constant: np.double = mbn.assert_is_a_number(constant)

        if mbn.is_zero(checked_coefficient):
            return checked_constant
        if mbn.is_one(checked_coefficient) and mbn.is_zero(checked_constant):
            return expression
        if mbn.is_a_number(expression):
            return np.double(expression) * checked_coefficient + checked_constant
        if isinstance(expression, LinearExpr):
            return _as_flat_linear_expression(
                expression * checked_coefficient + checked_constant
            )
        raise TypeError(f"Unknown expression {expression!r} of type {type(expression)}")

    def __hash__(self):
        return object.__hash__(self)

    def __add__(self, arg: LinearExprT) -> "_Sum":
        return _Sum(self, arg)

    def __radd__(self, arg: LinearExprT) -> "_Sum":
        return self.__add__(arg)

    def __sub__(self, arg: LinearExprT) -> "_Sum":
        return _Sum(self, -arg)

    def __rsub__(self, arg: LinearExprT) -> "_Sum":
        return _Sum(-self, arg)

    def __mul__(self, arg: NumberT) -> "_Product":
        return _Product(self, arg)

    def __rmul__(self, arg: NumberT) -> "_Product":
        return self.__mul__(arg)

    def __truediv__(self, coeff: NumberT) -> "_Product":
        return self.__mul__(1.0 / coeff)

    def __neg__(self) -> "_Product":
        return _Product(self, -1)

    def __bool__(self):
        raise NotImplementedError(f"Cannot use a LinearExpr {self} as a Boolean value")

    def __eq__(self, arg: LinearExprT) -> "BoundedLinearExpression":
        return BoundedLinearExpression(self - arg, 0, 0)

    def __ge__(self, arg: LinearExprT) -> "BoundedLinearExpression":
        return BoundedLinearExpression(
            self - arg, 0, math.inf
        )  # pytype: disable=wrong-arg-types  # numpy-scalars

    def __le__(self, arg: LinearExprT) -> "BoundedLinearExpression":
        return BoundedLinearExpression(
            self - arg, -math.inf, 0
        )  # pytype: disable=wrong-arg-types  # numpy-scalars


class Variable(LinearExpr):
    """A variable (continuous or integral).

    A Variable is an object that can take on any integer value within defined
    ranges. Variables appear in constraint like:

        x + y >= 5

    Solving a model is equivalent to finding, for each variable, a single value
    from the set of initial values (called the initial domain), such that the
    model is feasible, or optimal if you provided an objective function.
    """

    def __init__(
        self,
        helper: mbh.ModelBuilderHelper,
        lb: NumberT,
        ub: Optional[NumberT],
        is_integral: Optional[bool],
        name: Optional[str],
    ) -> None:
        """See Model.new_var below."""
        LinearExpr.__init__(self)
        self.__helper: mbh.ModelBuilderHelper = helper
        # Python do not support multiple __init__ methods.
        # This method is only called from the Model class.
        # We hack the parameter to support the two cases:
        # case 1:
        #     helper is a ModelBuilderHelper, lb is a double value, ub is a double
        #     value, is_integral is a Boolean value, and name is a string.
        # case 2:
        #     helper is a ModelBuilderHelper, lb is an index (int), ub is None,
        #     is_integral is None, and name is None.
        if mbn.is_integral(lb) and ub is None and is_integral is None:
            self.__index: np.int32 = np.int32(lb)
            self.__helper: mbh.ModelBuilderHelper = helper
        else:
            index: np.int32 = helper.add_var()
            self.__index: np.int32 = np.int32(index)
            self.__helper: mbh.ModelBuilderHelper = helper
            helper.set_var_lower_bound(index, lb)
            helper.set_var_upper_bound(index, ub)
            helper.set_var_integrality(index, is_integral)
            if name:
                helper.set_var_name(index, name)

    @property
    def index(self) -> np.int32:
        """Returns the index of the variable in the helper."""
        return self.__index

    @property
    def helper(self) -> mbh.ModelBuilderHelper:
        """Returns the underlying ModelBuilderHelper."""
        return self.__helper

    def is_equal_to(self, other: LinearExprT) -> bool:
        """Returns true if self == other in the python sense."""
        if not isinstance(other, Variable):
            return False
        return self.index == other.index and self.helper == other.helper

    def __str__(self) -> str:
        return self.name

    def __repr__(self) -> str:
        return self.__str__()

    @property
    def name(self) -> str:
        """Returns the name of the variable."""
        var_name = self.__helper.var_name(self.__index)
        if var_name:
            return var_name
        return f"variable#{self.index}"

    @name.setter
    def name(self, name: str) -> None:
        """Sets the name of the variable."""
        self.__helper.set_var_name(self.__index, name)

    @property
    def lower_bound(self) -> np.double:
        """Returns the lower bound of the variable."""
        return self.__helper.var_lower_bound(self.__index)

    @lower_bound.setter
    def lower_bound(self, bound: NumberT) -> None:
        """Sets the lower bound of the variable."""
        self.__helper.set_var_lower_bound(self.__index, bound)

    @property
    def upper_bound(self) -> np.double:
        """Returns the upper bound of the variable."""
        return self.__helper.var_upper_bound(self.__index)

    @upper_bound.setter
    def upper_bound(self, bound: NumberT) -> None:
        """Sets the upper bound of the variable."""
        self.__helper.set_var_upper_bound(self.__index, bound)

    @property
    def is_integral(self) -> bool:
        """Returns whether the variable is integral."""
        return self.__helper.var_is_integral(self.__index)

    @is_integral.setter
    def integrality(self, is_integral: bool) -> None:
        """Sets the integrality of the variable."""
        self.__helper.set_var_integrality(self.__index, is_integral)

    @property
    def objective_coefficient(self) -> NumberT:
        return self.__helper.var_objective_coefficient(self.__index)

    @objective_coefficient.setter
    def objective_coefficient(self, coeff: NumberT) -> None:
        self.__helper.set_var_objective_coefficient(self.__index, coeff)

    def __eq__(self, arg: Optional[LinearExprT]) -> ConstraintT:
        if arg is None:
            return False
        if isinstance(arg, Variable):
            return VarEqVar(self, arg)
        return BoundedLinearExpression(
            self - arg, 0.0, 0.0
        )  # pytype: disable=wrong-arg-types  # numpy-scalars

    def __hash__(self):
        return hash((self.__helper, self.__index))


class _BoundedLinearExpr(metaclass=abc.ABCMeta):
    """Interface for types that can build bounded linear (boolean) expressions.

    Classes derived from _BoundedLinearExpr are used to build linear constraints
    to be satisfied.

      * BoundedLinearExpression: a linear expression with upper and lower bounds.
      * VarEqVar: an equality comparison between two variables.
    """

    @abc.abstractmethod
    def _add_linear_constraint(
        self, helper: mbh.ModelBuilderHelper, name: str
    ) -> "LinearConstraint":
        """Creates a new linear constraint in the helper.

        Args:
          helper (mbh.ModelBuilderHelper): The helper to create the constraint.
          name (str): The name of the linear constraint.

        Returns:
          LinearConstraint: A reference to the linear constraint in the helper.
        """

    @abc.abstractmethod
    def _add_enforced_linear_constraint(
        self,
        helper: mbh.ModelBuilderHelper,
        var: Variable,
        value: bool,
        name: str,
    ) -> "EnforcedLinearConstraint":
        """Creates a new enforced linear constraint in the helper.

        Args:
          helper (mbh.ModelBuilderHelper): The helper to create the constraint.
          var (Variable): The indicator variable of the constraint.
          value (bool): The indicator value of the constraint.
          name (str): The name of the linear constraint.

        Returns:
          Enforced LinearConstraint: A reference to the linear constraint in the
          helper.
        """


def _add_linear_constraint_to_helper(
    bounded_expr: Union[bool, _BoundedLinearExpr],
    helper: mbh.ModelBuilderHelper,
    name: Optional[str],
):
    """Creates a new linear constraint in the helper.

    It handles boolean values (which might arise in the construction of
    BoundedLinearExpressions).

    If bounded_expr is a Boolean value, the created constraint is different.
    In that case, the constraint will be immutable and marked as under-specified.
    It will be always feasible or infeasible whether the value is True or False.

    Args:
      bounded_expr: The bounded expression used to create the constraint.
      helper: The helper to create the constraint.
      name: The name of the constraint to be created.

    Returns:
      LinearConstraint: a constraint in the helper corresponding to the input.

    Raises:
      TypeError: If constraint is an invalid type.
    """
    if isinstance(bounded_expr, bool):
        c = LinearConstraint(helper, is_under_specified=True)
        if name is not None:
            helper.set_constraint_name(c.index, name)
        if bounded_expr:
            # constraint that is always feasible: 0.0 <= nothing <= 0.0
            helper.set_constraint_lower_bound(c.index, 0.0)
            helper.set_constraint_upper_bound(c.index, 0.0)
        else:
            # constraint that is always infeasible: +oo <= nothing <= -oo
            helper.set_constraint_lower_bound(c.index, 1)
            helper.set_constraint_upper_bound(c.index, -1)
        return c
    if isinstance(bounded_expr, _BoundedLinearExpr):
        # pylint: disable=protected-access
        return bounded_expr._add_linear_constraint(helper, name)
    raise TypeError("invalid type={}".format(type(bounded_expr)))


def _add_enforced_linear_constraint_to_helper(
    bounded_expr: Union[bool, _BoundedLinearExpr],
    helper: mbh.ModelBuilderHelper,
    var: Variable,
    value: bool,
    name: Optional[str],
):
    """Creates a new enforced linear constraint in the helper.

    It handles boolean values (which might arise in the construction of
    BoundedLinearExpressions).

    If bounded_expr is a Boolean value, the linear part of the constraint is
    different.
    In that case, the constraint will be immutable and marked as under-specified.
    Its linear part will be always feasible or infeasible whether the value is
    True or False.

    Args:
      bounded_expr: The bounded expression used to create the constraint.
      helper: The helper to create the constraint.
      var: the variable used in the indicator
      value: the value used in the indicator
      name: The name of the constraint to be created.

    Returns:
      EnforcedLinearConstraint: a constraint in the helper corresponding to the
      input.

    Raises:
      TypeError: If constraint is an invalid type.
    """
    if isinstance(bounded_expr, bool):
        # TODO(user): create indicator variable assignment instead ?
        c = EnforcedLinearConstraint(helper, is_under_specified=True)
        c.indicator_variable = var
        c.indicator_value = value
        if name is not None:
            helper.set_enforced_constraint_name(c.index, name)
        if bounded_expr:
            # constraint that is always feasible: 0.0 <= nothing <= 0.0
            helper.set_enforced_constraint_lower_bound(c.index, 0.0)
            helper.set_enforced_constraint_upper_bound(c.index, 0.0)
        else:
            # constraint that is always infeasible: +oo <= nothing <= -oo
            helper.set_enforced_constraint_lower_bound(c.index, 1)
            helper.set_enforced_constraint_upper_bound(c.index, -1)
        return c
    if isinstance(bounded_expr, _BoundedLinearExpr):
        # pylint: disable=protected-access
        return bounded_expr._add_enforced_linear_constraint(helper, var, value, name)
    raise TypeError("invalid type={}".format(type(bounded_expr)))


@dataclasses.dataclass(repr=False, eq=False, frozen=True)
class VarEqVar(_BoundedLinearExpr):
    """Represents var == var."""

    __slots__ = ("left", "right")

    left: Variable
    right: Variable

    def __str__(self):
        return f"{self.left} == {self.right}"

    def __repr__(self):
        return self.__str__()

    def __bool__(self) -> bool:
        return hash(self.left) == hash(self.right)

    def _add_linear_constraint(
        self, helper: mbh.ModelBuilderHelper, name: str
    ) -> "LinearConstraint":
        c = LinearConstraint(helper)
        helper.set_constraint_lower_bound(c.index, 0.0)
        helper.set_constraint_upper_bound(c.index, 0.0)
        # pylint: disable=protected-access
        helper.add_term_to_constraint(c.index, self.left.index, 1.0)
        helper.add_term_to_constraint(c.index, self.right.index, -1.0)
        # pylint: enable=protected-access
        helper.set_constraint_name(c.index, name)
        return c

    def _add_enforced_linear_constraint(
        self,
        helper: mbh.ModelBuilderHelper,
        var: Variable,
        value: bool,
        name: str,
    ) -> "EnforcedLinearConstraint":
        """Adds an enforced linear constraint to the model."""
        c = EnforcedLinearConstraint(helper)
        c.indicator_variable = var
        c.indicator_value = value
        helper.set_enforced_constraint_lower_bound(c.index, 0.0)
        helper.set_enforced_constraint_upper_bound(c.index, 0.0)
        # pylint: disable=protected-access
        helper.add_term_to_enforced_constraint(c.index, self.left.index, 1.0)
        helper.add_term_to_enforced_constraint(c.index, self.right.index, -1.0)
        # pylint: enable=protected-access
        helper.set_enforced_constraint_name(c.index, name)
        return c


class BoundedLinearExpression(_BoundedLinearExpr):
    """Represents a linear constraint: `lb <= linear expression <= ub`.

    The only use of this class is to be added to the Model through
    `Model.add(bounded expression)`, as in:

        model.Add(x + 2 * y -1 >= z)
    """

    def __init__(self, expr: LinearExprT, lb: NumberT, ub: NumberT) -> None:
        self.__expr: LinearExprT = expr
        self.__lb: np.double = mbn.assert_is_a_number(lb)
        self.__ub: np.double = mbn.assert_is_a_number(ub)

    def __str__(self) -> str:
        if self.__lb > -math.inf and self.__ub < math.inf:
            if self.__lb == self.__ub:
                return f"{self.__expr} == {self.__lb}"
            else:
                return f"{self.__lb} <= {self.__expr} <= {self.__ub}"
        elif self.__lb > -math.inf:
            return f"{self.__expr} >= {self.__lb}"
        elif self.__ub < math.inf:
            return f"{self.__expr} <= {self.__ub}"
        else:
            return f"{self.__expr} free"

    def __repr__(self):
        return self.__str__()

    @property
    def expression(self) -> LinearExprT:
        return self.__expr

    @property
    def lower_bound(self) -> np.double:
        return self.__lb

    @property
    def upper_bound(self) -> np.double:
        return self.__ub

    def __bool__(self) -> bool:
        raise NotImplementedError(
            f"Cannot use a BoundedLinearExpression {self} as a Boolean value"
        )

    def _add_linear_constraint(
        self, helper: mbh.ModelBuilderHelper, name: Optional[str]
    ) -> "LinearConstraint":
        c = LinearConstraint(helper)
        flat_expr = _as_flat_linear_expression(self.__expr)
        # pylint: disable=protected-access
        helper.add_terms_to_constraint(
            c.index, flat_expr._variable_indices, flat_expr._coefficients
        )
        helper.set_constraint_lower_bound(c.index, self.__lb - flat_expr._offset)
        helper.set_constraint_upper_bound(c.index, self.__ub - flat_expr._offset)
        # pylint: enable=protected-access
        if name is not None:
            helper.set_constraint_name(c.index, name)
        return c

    def _add_enforced_linear_constraint(
        self,
        helper: mbh.ModelBuilderHelper,
        var: Variable,
        value: bool,
        name: Optional[str],
    ) -> "EnforcedLinearConstraint":
        """Adds an enforced linear constraint to the model."""
        c = EnforcedLinearConstraint(helper)
        c.indicator_variable = var
        c.indicator_value = value
        flat_expr = _as_flat_linear_expression(self.__expr)
        # pylint: disable=protected-access
        helper.add_terms_to_enforced_constraint(
            c.index, flat_expr._variable_indices, flat_expr._coefficients
        )
        helper.set_enforced_constraint_lower_bound(
            c.index, self.__lb - flat_expr._offset
        )
        helper.set_enforced_constraint_upper_bound(
            c.index, self.__ub - flat_expr._offset
        )
        # pylint: enable=protected-access
        if name is not None:
            helper.set_enforced_constraint_name(c.index, name)
        return c


class LinearConstraint:
    """Stores a linear equation.

    Example:
        x = model.new_num_var(0, 10, 'x')
        y = model.new_num_var(0, 10, 'y')

        linear_constraint = model.add(x + 2 * y == 5)
    """

    def __init__(
        self,
        helper: mbh.ModelBuilderHelper,
        *,
        index: Optional[IntegerT] = None,
        is_under_specified: bool = False,
    ) -> None:
        """LinearConstraint constructor.

        Args:
          helper: The pybind11 ModelBuilderHelper.
          index: If specified, recreates a wrapper to an existing linear constraint.
          is_under_specified: indicates if the constraint was created by
            model.add(bool).
        """
        if index is None:
            self.__index = helper.add_linear_constraint()
        else:
            self.__index = index
        self.__helper: mbh.ModelBuilderHelper = helper
        self.__is_under_specified = is_under_specified

    @property
    def index(self) -> IntegerT:
        """Returns the index of the constraint in the helper."""
        return self.__index

    @property
    def helper(self) -> mbh.ModelBuilderHelper:
        """Returns the ModelBuilderHelper instance."""
        return self.__helper

    @property
    def lower_bound(self) -> np.double:
        return self.__helper.constraint_lower_bound(self.__index)

    @lower_bound.setter
    def lower_bound(self, bound: NumberT) -> None:
        self.assert_constraint_is_well_defined()
        self.__helper.set_constraint_lower_bound(self.__index, bound)

    @property
    def upper_bound(self) -> np.double:
        return self.__helper.constraint_upper_bound(self.__index)

    @upper_bound.setter
    def upper_bound(self, bound: NumberT) -> None:
        self.assert_constraint_is_well_defined()
        self.__helper.set_constraint_upper_bound(self.__index, bound)

    @property
    def name(self) -> str:
        constraint_name = self.__helper.constraint_name(self.__index)
        if constraint_name:
            return constraint_name
        return f"linear_constraint#{self.__index}"

    @name.setter
    def name(self, name: str) -> None:
        return self.__helper.set_constraint_name(self.__index, name)

    @property
    def is_under_specified(self) -> bool:
        """Returns True if the constraint is under specified.

        Usually, it means that it was created by model.add(False) or model.add(True)
        The effect is that modifying the constraint will raise an exception.
        """
        return self.__is_under_specified

    def assert_constraint_is_well_defined(self) -> None:
        """Raises an exception if the constraint is under specified."""
        if self.__is_under_specified:
            raise ValueError(
                f"Constraint {self.index} is under specified and cannot be modified"
            )

    def __str__(self):
        return self.name

    def __repr__(self):
        return (
            f"LinearConstraint({self.name}, lb={self.lower_bound},"
            f" ub={self.upper_bound},"
            f" var_indices={self.helper.constraint_var_indices(self.index)},"
            f" coefficients={self.helper.constraint_coefficients(self.index)})"
        )

    def set_coefficient(self, var: Variable, coeff: NumberT) -> None:
        """Sets the coefficient of the variable in the constraint."""
        self.assert_constraint_is_well_defined()
        self.__helper.set_constraint_coefficient(self.__index, var.index, coeff)

    def add_term(self, var: Variable, coeff: NumberT) -> None:
        """Adds var * coeff to the constraint."""
        self.assert_constraint_is_well_defined()
        self.__helper.safe_add_term_to_constraint(self.__index, var.index, coeff)

    def clear_terms(self) -> None:
        """Clear all terms of the constraint."""
        self.assert_constraint_is_well_defined()
        self.__helper.clear_constraint_terms(self.__index)


class EnforcedLinearConstraint:
    """Stores an enforced linear equation, also name indicator constraint.

    Example:
        x = model.new_num_var(0, 10, 'x')
        y = model.new_num_var(0, 10, 'y')
        z = model.new_bool_var('z')

        enforced_linear_constraint = model.add_enforced(x + 2 * y == 5, z, False)
    """

    def __init__(
        self,
        helper: mbh.ModelBuilderHelper,
        *,
        index: Optional[IntegerT] = None,
        is_under_specified: bool = False,
    ) -> None:
        """EnforcedLinearConstraint constructor.

        Args:
          helper: The pybind11 ModelBuilderHelper.
          index: If specified, recreates a wrapper to an existing linear constraint.
          is_under_specified: indicates if the constraint was created by
            model.add(bool).
        """
        if index is None:
            self.__index = helper.add_enforced_linear_constraint()
        else:
            if not helper.is_enforced_linear_constraint(index):
                raise ValueError(
                    f"the given index {index} does not refer to an enforced linear"
                    " constraint"
                )

            self.__index = index
        self.__helper: mbh.ModelBuilderHelper = helper
        self.__is_under_specified = is_under_specified

    @property
    def index(self) -> IntegerT:
        """Returns the index of the constraint in the helper."""
        return self.__index

    @property
    def helper(self) -> mbh.ModelBuilderHelper:
        """Returns the ModelBuilderHelper instance."""
        return self.__helper

    @property
    def lower_bound(self) -> np.double:
        return self.__helper.enforced_constraint_lower_bound(self.__index)

    @lower_bound.setter
    def lower_bound(self, bound: NumberT) -> None:
        self.assert_constraint_is_well_defined()
        self.__helper.set_enforced_constraint_lower_bound(self.__index, bound)

    @property
    def upper_bound(self) -> np.double:
        return self.__helper.enforced_constraint_upper_bound(self.__index)

    @upper_bound.setter
    def upper_bound(self, bound: NumberT) -> None:
        self.assert_constraint_is_well_defined()
        self.__helper.set_enforced_constraint_upper_bound(self.__index, bound)

    @property
    def indicator_variable(self) -> "Variable":
        enforcement_var_index = (
            self.__helper.enforced_constraint_indicator_variable_index(self.__index)
        )
        return Variable(self.__helper, enforcement_var_index, None, None, None)

    @indicator_variable.setter
    def indicator_variable(self, var: "Variable") -> None:
        self.__helper.set_enforced_constraint_indicator_variable_index(
            self.__index, var.index
        )

    @property
    def indicator_value(self) -> bool:
        return self.__helper.enforced_constraint_indicator_value(self.__index)

    @indicator_value.setter
    def indicator_value(self, value: bool) -> None:
        self.__helper.set_enforced_constraint_indicator_value(self.__index, value)

    @property
    def name(self) -> str:
        constraint_name = self.__helper.enforced_constraint_name(self.__index)
        if constraint_name:
            return constraint_name
        return f"enforced_linear_constraint#{self.__index}"

    @name.setter
    def name(self, name: str) -> None:
        return self.__helper.set_enforced_constraint_name(self.__index, name)

    @property
    def is_under_specified(self) -> bool:
        """Returns True if the constraint is under specified.

        Usually, it means that it was created by model.add(False) or model.add(True)
        The effect is that modifying the constraint will raise an exception.
        """
        return self.__is_under_specified

    def assert_constraint_is_well_defined(self) -> None:
        """Raises an exception if the constraint is under specified."""
        if self.__is_under_specified:
            raise ValueError(
                f"Constraint {self.index} is under specified and cannot be modified"
            )

    def __str__(self):
        return self.name

    def __repr__(self):
        return (
            f"EnforcedLinearConstraint({self.name}, lb={self.lower_bound},"
            f" ub={self.upper_bound},"
            f" var_indices={self.helper.enforced_constraint_var_indices(self.index)},"
            f" coefficients={self.helper.enforced_constraint_coefficients(self.index)},"
            f" indicator_variable={self.indicator_variable}"
            f" indicator_value={self.indicator_value})"
        )

    def set_coefficient(self, var: Variable, coeff: NumberT) -> None:
        """Sets the coefficient of the variable in the constraint."""
        self.assert_constraint_is_well_defined()
        self.__helper.set_enforced_constraint_coefficient(
            self.__index, var.index, coeff
        )

    def add_term(self, var: Variable, coeff: NumberT) -> None:
        """Adds var * coeff to the constraint."""
        self.assert_constraint_is_well_defined()
        self.__helper.safe_add_term_to_enforced_constraint(
            self.__index, var.index, coeff
        )

    def clear_terms(self) -> None:
        """Clear all terms of the constraint."""
        self.assert_constraint_is_well_defined()
        self.__helper.clear_enforced_constraint_terms(self.__index)


class Model:
    """Methods for building a linear model.

    Methods beginning with:

    * ```new_``` create integer, boolean, or interval variables.
    * ```add_``` create new constraints and add them to the model.
    """

    def __init__(self):
        self.__helper: mbh.ModelBuilderHelper = mbh.ModelBuilderHelper()

    def clone(self) -> "Model":
        """Returns a clone of the current model."""
        clone = Model()
        clone.helper.overwrite_model(self.helper)
        return clone

    @typing.overload
    def _get_linear_constraints(self, constraints: Optional[pd.Index]) -> pd.Index: ...

    @typing.overload
    def _get_linear_constraints(self, constraints: pd.Series) -> pd.Series: ...

    def _get_linear_constraints(
        self, constraints: Optional[_IndexOrSeries] = None
    ) -> _IndexOrSeries:
        if constraints is None:
            return self.get_linear_constraints()
        return constraints

    @typing.overload
    def _get_variables(self, variables: Optional[pd.Index]) -> pd.Index: ...

    @typing.overload
    def _get_variables(self, variables: pd.Series) -> pd.Series: ...

    def _get_variables(
        self, variables: Optional[_IndexOrSeries] = None
    ) -> _IndexOrSeries:
        if variables is None:
            return self.get_variables()
        return variables

    def get_linear_constraints(self) -> pd.Index:
        """Gets all linear constraints in the model."""
        return pd.Index(
            [self.linear_constraint_from_index(i) for i in range(self.num_constraints)],
            name="linear_constraint",
        )

    def get_linear_constraint_expressions(
        self, constraints: Optional[_IndexOrSeries] = None
    ) -> pd.Series:
        """Gets the expressions of all linear constraints in the set.

        If `constraints` is a `pd.Index`, then the output will be indexed by the
        constraints. If `constraints` is a `pd.Series` indexed by the underlying
        dimensions, then the output will be indexed by the same underlying
        dimensions.

        Args:
          constraints (Union[pd.Index, pd.Series]): Optional. The set of linear
            constraints from which to get the expressions. If unspecified, all
            linear constraints will be in scope.

        Returns:
          pd.Series: The expressions of all linear constraints in the set.
        """
        return _attribute_series(
            # pylint: disable=g-long-lambda
            func=lambda c: _as_flat_linear_expression(
                # pylint: disable=g-complex-comprehension
                sum(
                    coeff * Variable(self.__helper, var_id, None, None, None)
                    for var_id, coeff in zip(
                        c.helper.constraint_var_indices(c.index),
                        c.helper.constraint_coefficients(c.index),
                    )
                )
            ),
            values=self._get_linear_constraints(constraints),
        )

    def get_linear_constraint_lower_bounds(
        self, constraints: Optional[_IndexOrSeries] = None
    ) -> pd.Series:
        """Gets the lower bounds of all linear constraints in the set.

        If `constraints` is a `pd.Index`, then the output will be indexed by the
        constraints. If `constraints` is a `pd.Series` indexed by the underlying
        dimensions, then the output will be indexed by the same underlying
        dimensions.

        Args:
          constraints (Union[pd.Index, pd.Series]): Optional. The set of linear
            constraints from which to get the lower bounds. If unspecified, all
            linear constraints will be in scope.

        Returns:
          pd.Series: The lower bounds of all linear constraints in the set.
        """
        return _attribute_series(
            func=lambda c: c.lower_bound,  # pylint: disable=protected-access
            values=self._get_linear_constraints(constraints),
        )

    def get_linear_constraint_upper_bounds(
        self, constraints: Optional[_IndexOrSeries] = None
    ) -> pd.Series:
        """Gets the upper bounds of all linear constraints in the set.

        If `constraints` is a `pd.Index`, then the output will be indexed by the
        constraints. If `constraints` is a `pd.Series` indexed by the underlying
        dimensions, then the output will be indexed by the same underlying
        dimensions.

        Args:
          constraints (Union[pd.Index, pd.Series]): Optional. The set of linear
            constraints. If unspecified, all linear constraints will be in scope.

        Returns:
          pd.Series: The upper bounds of all linear constraints in the set.
        """
        return _attribute_series(
            func=lambda c: c.upper_bound,  # pylint: disable=protected-access
            values=self._get_linear_constraints(constraints),
        )

    def get_variables(self) -> pd.Index:
        """Gets all variables in the model."""
        return pd.Index(
            [self.var_from_index(i) for i in range(self.num_variables)],
            name="variable",
        )

    def get_variable_lower_bounds(
        self, variables: Optional[_IndexOrSeries] = None
    ) -> pd.Series:
        """Gets the lower bounds of all variables in the set.

        If `variables` is a `pd.Index`, then the output will be indexed by the
        variables. If `variables` is a `pd.Series` indexed by the underlying
        dimensions, then the output will be indexed by the same underlying
        dimensions.

        Args:
          variables (Union[pd.Index, pd.Series]): Optional. The set of variables
            from which to get the lower bounds. If unspecified, all variables will
            be in scope.

        Returns:
          pd.Series: The lower bounds of all variables in the set.
        """
        return _attribute_series(
            func=lambda v: v.lower_bound,  # pylint: disable=protected-access
            values=self._get_variables(variables),
        )

    def get_variable_upper_bounds(
        self, variables: Optional[_IndexOrSeries] = None
    ) -> pd.Series:
        """Gets the upper bounds of all variables in the set.

        Args:
          variables (Union[pd.Index, pd.Series]): Optional. The set of variables
            from which to get the upper bounds. If unspecified, all variables will
            be in scope.

        Returns:
          pd.Series: The upper bounds of all variables in the set.
        """
        return _attribute_series(
            func=lambda v: v.upper_bound,  # pylint: disable=protected-access
            values=self._get_variables(variables),
        )

    # Integer variable.

    def new_var(
        self, lb: NumberT, ub: NumberT, is_integer: bool, name: Optional[str]
    ) -> Variable:
        """Create an integer variable with domain [lb, ub].

        Args:
          lb: Lower bound of the variable.
          ub: Upper bound of the variable.
          is_integer: Indicates if the variable must take integral values.
          name: The name of the variable.

        Returns:
          a variable whose domain is [lb, ub].
        """

        return Variable(self.__helper, lb, ub, is_integer, name)

    def new_int_var(
        self, lb: NumberT, ub: NumberT, name: Optional[str] = None
    ) -> Variable:
        """Create an integer variable with domain [lb, ub].

        Args:
          lb: Lower bound of the variable.
          ub: Upper bound of the variable.
          name: The name of the variable.

        Returns:
          a variable whose domain is [lb, ub].
        """

        return self.new_var(lb, ub, True, name)

    def new_num_var(
        self, lb: NumberT, ub: NumberT, name: Optional[str] = None
    ) -> Variable:
        """Create an integer variable with domain [lb, ub].

        Args:
          lb: Lower bound of the variable.
          ub: Upper bound of the variable.
          name: The name of the variable.

        Returns:
          a variable whose domain is [lb, ub].
        """

        return self.new_var(lb, ub, False, name)

    def new_bool_var(self, name: Optional[str] = None) -> Variable:
        """Creates a 0-1 variable with the given name."""
        return self.new_var(
            0, 1, True, name
        )  # pytype: disable=wrong-arg-types  # numpy-scalars

    def new_constant(self, value: NumberT) -> Variable:
        """Declares a constant variable."""
        return self.new_var(value, value, False, None)

    def new_var_series(
        self,
        name: str,
        index: pd.Index,
        lower_bounds: Union[NumberT, pd.Series] = -math.inf,
        upper_bounds: Union[NumberT, pd.Series] = math.inf,
        is_integral: Union[bool, pd.Series] = False,
    ) -> pd.Series:
        """Creates a series of (scalar-valued) variables with the given name.

        Args:
          name (str): Required. The name of the variable set.
          index (pd.Index): Required. The index to use for the variable set.
          lower_bounds (Union[int, float, pd.Series]): Optional. A lower bound for
            variables in the set. If a `pd.Series` is passed in, it will be based on
            the corresponding values of the pd.Series. Defaults to -inf.
          upper_bounds (Union[int, float, pd.Series]): Optional. An upper bound for
            variables in the set. If a `pd.Series` is passed in, it will be based on
            the corresponding values of the pd.Series. Defaults to +inf.
          is_integral (bool, pd.Series): Optional. Indicates if the variable can
            only take integer values. If a `pd.Series` is passed in, it will be
            based on the corresponding values of the pd.Series. Defaults to False.

        Returns:
          pd.Series: The variable set indexed by its corresponding dimensions.

        Raises:
          TypeError: if the `index` is invalid (e.g. a `DataFrame`).
          ValueError: if the `name` is not a valid identifier or already exists.
          ValueError: if the `lowerbound` is greater than the `upperbound`.
          ValueError: if the index of `lower_bound`, `upper_bound`, or `is_integer`
          does not match the input index.
        """
        if not isinstance(index, pd.Index):
            raise TypeError("Non-index object is used as index")
        if not name.isidentifier():
            raise ValueError("name={} is not a valid identifier".format(name))
        if (
            mbn.is_a_number(lower_bounds)
            and mbn.is_a_number(upper_bounds)
            and lower_bounds > upper_bounds
        ):
            raise ValueError(
                "lower_bound={} is greater than upper_bound={} for variable set={}".format(
                    lower_bounds, upper_bounds, name
                )
            )
        if (
            isinstance(is_integral, bool)
            and is_integral
            and mbn.is_a_number(lower_bounds)
            and mbn.is_a_number(upper_bounds)
            and math.isfinite(lower_bounds)
            and math.isfinite(upper_bounds)
            and math.ceil(lower_bounds) > math.floor(upper_bounds)
        ):
            raise ValueError(
                "ceil(lower_bound={})={}".format(lower_bounds, math.ceil(lower_bounds))
                + " is greater than floor("
                + "upper_bound={})={}".format(upper_bounds, math.floor(upper_bounds))
                + " for variable set={}".format(name)
            )
        lower_bounds = _convert_to_series_and_validate_index(lower_bounds, index)
        upper_bounds = _convert_to_series_and_validate_index(upper_bounds, index)
        is_integrals = _convert_to_series_and_validate_index(is_integral, index)
        return pd.Series(
            index=index,
            data=[
                # pylint: disable=g-complex-comprehension
                Variable(
                    helper=self.__helper,
                    name=f"{name}[{i}]",
                    lb=lower_bounds[i],
                    ub=upper_bounds[i],
                    is_integral=is_integrals[i],
                )
                for i in index
            ],
        )

    def new_num_var_series(
        self,
        name: str,
        index: pd.Index,
        lower_bounds: Union[NumberT, pd.Series] = -math.inf,
        upper_bounds: Union[NumberT, pd.Series] = math.inf,
    ) -> pd.Series:
        """Creates a series of continuous variables with the given name.

        Args:
          name (str): Required. The name of the variable set.
          index (pd.Index): Required. The index to use for the variable set.
          lower_bounds (Union[int, float, pd.Series]): Optional. A lower bound for
            variables in the set. If a `pd.Series` is passed in, it will be based on
            the corresponding values of the pd.Series. Defaults to -inf.
          upper_bounds (Union[int, float, pd.Series]): Optional. An upper bound for
            variables in the set. If a `pd.Series` is passed in, it will be based on
            the corresponding values of the pd.Series. Defaults to +inf.

        Returns:
          pd.Series: The variable set indexed by its corresponding dimensions.

        Raises:
          TypeError: if the `index` is invalid (e.g. a `DataFrame`).
          ValueError: if the `name` is not a valid identifier or already exists.
          ValueError: if the `lowerbound` is greater than the `upperbound`.
          ValueError: if the index of `lower_bound`, `upper_bound`, or `is_integer`
          does not match the input index.
        """
        return self.new_var_series(name, index, lower_bounds, upper_bounds, False)

    def new_int_var_series(
        self,
        name: str,
        index: pd.Index,
        lower_bounds: Union[NumberT, pd.Series] = -math.inf,
        upper_bounds: Union[NumberT, pd.Series] = math.inf,
    ) -> pd.Series:
        """Creates a series of integer variables with the given name.

        Args:
          name (str): Required. The name of the variable set.
          index (pd.Index): Required. The index to use for the variable set.
          lower_bounds (Union[int, float, pd.Series]): Optional. A lower bound for
            variables in the set. If a `pd.Series` is passed in, it will be based on
            the corresponding values of the pd.Series. Defaults to -inf.
          upper_bounds (Union[int, float, pd.Series]): Optional. An upper bound for
            variables in the set. If a `pd.Series` is passed in, it will be based on
            the corresponding values of the pd.Series. Defaults to +inf.

        Returns:
          pd.Series: The variable set indexed by its corresponding dimensions.

        Raises:
          TypeError: if the `index` is invalid (e.g. a `DataFrame`).
          ValueError: if the `name` is not a valid identifier or already exists.
          ValueError: if the `lowerbound` is greater than the `upperbound`.
          ValueError: if the index of `lower_bound`, `upper_bound`, or `is_integer`
          does not match the input index.
        """
        return self.new_var_series(name, index, lower_bounds, upper_bounds, True)

    def new_bool_var_series(
        self,
        name: str,
        index: pd.Index,
    ) -> pd.Series:
        """Creates a series of Boolean variables with the given name.

        Args:
          name (str): Required. The name of the variable set.
          index (pd.Index): Required. The index to use for the variable set.

        Returns:
          pd.Series: The variable set indexed by its corresponding dimensions.

        Raises:
          TypeError: if the `index` is invalid (e.g. a `DataFrame`).
          ValueError: if the `name` is not a valid identifier or already exists.
          ValueError: if the `lowerbound` is greater than the `upperbound`.
          ValueError: if the index of `lower_bound`, `upper_bound`, or `is_integer`
          does not match the input index.
        """
        return self.new_var_series(name, index, 0, 1, True)

    def var_from_index(self, index: IntegerT) -> Variable:
        """Rebuilds a variable object from the model and its index."""
        return Variable(self.__helper, index, None, None, None)

    # Linear constraints.

    def add_linear_constraint(  # pytype: disable=annotation-type-mismatch  # numpy-scalars
        self,
        linear_expr: LinearExprT,
        lb: NumberT = -math.inf,
        ub: NumberT = math.inf,
        name: Optional[str] = None,
    ) -> LinearConstraint:
        """Adds the constraint: `lb <= linear_expr <= ub` with the given name."""
        ct = LinearConstraint(self.__helper)
        if name:
            self.__helper.set_constraint_name(ct.index, name)
        if mbn.is_a_number(linear_expr):
            self.__helper.set_constraint_lower_bound(ct.index, lb - linear_expr)
            self.__helper.set_constraint_upper_bound(ct.index, ub - linear_expr)
        elif isinstance(linear_expr, Variable):
            self.__helper.set_constraint_lower_bound(ct.index, lb)
            self.__helper.set_constraint_upper_bound(ct.index, ub)
            self.__helper.add_term_to_constraint(ct.index, linear_expr.index, 1.0)
        elif isinstance(linear_expr, LinearExpr):
            flat_expr = _as_flat_linear_expression(linear_expr)
            # pylint: disable=protected-access
            self.__helper.set_constraint_lower_bound(ct.index, lb - flat_expr._offset)
            self.__helper.set_constraint_upper_bound(ct.index, ub - flat_expr._offset)
            self.__helper.add_terms_to_constraint(
                ct.index, flat_expr._variable_indices, flat_expr._coefficients
            )
        else:
            raise TypeError(
                f"Not supported: Model.add_linear_constraint({linear_expr})"
                f" with type {type(linear_expr)}"
            )
        return ct

    def add(
        self, ct: Union[ConstraintT, pd.Series], name: Optional[str] = None
    ) -> Union[LinearConstraint, pd.Series]:
        """Adds a `BoundedLinearExpression` to the model.

        Args:
          ct: A [`BoundedLinearExpression`](#boundedlinearexpression).
          name: An optional name.

        Returns:
          An instance of the `Constraint` class.

        Note that a special treatment is done when the argument does not contain any
        variable, and thus evaluates to True or False.

        `model.add(True)` will create a constraint 0 <= empty sum <= 0.
        The constraint will be marked as under specified, and cannot be modified
        thereafter.

        `model.add(False)` will create a constraint inf <= empty sum <= -inf. The
        constraint will be marked as under specified, and cannot be modified
        thereafter.

        you can check the if a constraint is under specified by reading the
        `LinearConstraint.is_under_specified` property.
        """
        if isinstance(ct, _BoundedLinearExpr):
            return ct._add_linear_constraint(self.__helper, name)
        elif isinstance(ct, bool):
            return _add_linear_constraint_to_helper(ct, self.__helper, name)
        elif isinstance(ct, pd.Series):
            return pd.Series(
                index=ct.index,
                data=[
                    _add_linear_constraint_to_helper(
                        expr, self.__helper, f"{name}[{i}]"
                    )
                    for (i, expr) in zip(ct.index, ct)
                ],
            )
        else:
            raise TypeError("Not supported: Model.add(" + str(ct) + ")")

    def linear_constraint_from_index(self, index: IntegerT) -> LinearConstraint:
        """Rebuilds a linear constraint object from the model and its index."""
        return LinearConstraint(self.__helper, index=index)

    # EnforcedLinear constraints.

    def add_enforced_linear_constraint(  # pytype: disable=annotation-type-mismatch  # numpy-scalars
        self,
        linear_expr: LinearExprT,
        ivar: "Variable",
        ivalue: bool,
        lb: NumberT = -math.inf,
        ub: NumberT = math.inf,
        name: Optional[str] = None,
    ) -> EnforcedLinearConstraint:
        """Adds the constraint: `ivar == ivalue => lb <= linear_expr <= ub` with the given name."""
        ct = EnforcedLinearConstraint(self.__helper)
        ct.indicator_variable = ivar
        ct.indicator_value = ivalue
        if name:
            self.__helper.set_constraint_name(ct.index, name)
        if mbn.is_a_number(linear_expr):
            self.__helper.set_constraint_lower_bound(ct.index, lb - linear_expr)
            self.__helper.set_constraint_upper_bound(ct.index, ub - linear_expr)
        elif isinstance(linear_expr, Variable):
            self.__helper.set_constraint_lower_bound(ct.index, lb)
            self.__helper.set_constraint_upper_bound(ct.index, ub)
            self.__helper.add_term_to_constraint(ct.index, linear_expr.index, 1.0)
        elif isinstance(linear_expr, LinearExpr):
            flat_expr = _as_flat_linear_expression(linear_expr)
            # pylint: disable=protected-access
            self.__helper.set_constraint_lower_bound(ct.index, lb - flat_expr._offset)
            self.__helper.set_constraint_upper_bound(ct.index, ub - flat_expr._offset)
            self.__helper.add_terms_to_constraint(
                ct.index, flat_expr._variable_indices, flat_expr._coefficients
            )
        else:
            raise TypeError(
                "Not supported:"
                f" Model.add_enforced_linear_constraint({linear_expr}) with"
                f" type {type(linear_expr)}"
            )
        return ct

    def add_enforced(
        self,
        ct: Union[ConstraintT, pd.Series],
        var: Union[Variable, pd.Series],
        value: Union[bool, pd.Series],
        name: Optional[str] = None,
    ) -> Union[EnforcedLinearConstraint, pd.Series]:
        """Adds a `ivar == ivalue => BoundedLinearExpression` to the model.

        Args:
          ct: A [`BoundedLinearExpression`](#boundedlinearexpression).
          var: The indicator variable
          value: the indicator value
          name: An optional name.

        Returns:
          An instance of the `Constraint` class.

        Note that a special treatment is done when the argument does not contain any
        variable, and thus evaluates to True or False.

        model.add_enforced(True, ivar, ivalue) will create a constraint 0 <= empty
        sum <= 0

        model.add_enforced(False, var, value) will create a constraint inf <=
        empty sum <= -inf

        you can check the if a constraint is always false (lb=inf, ub=-inf) by
        calling EnforcedLinearConstraint.is_always_false()
        """
        if isinstance(ct, _BoundedLinearExpr):
            return ct._add_enforced_linear_constraint(self.__helper, var, value, name)
        elif (
            isinstance(ct, bool)
            and isinstance(var, Variable)
            and isinstance(value, bool)
        ):
            return _add_enforced_linear_constraint_to_helper(
                ct, self.__helper, var, value, name
            )
        elif isinstance(ct, pd.Series):
            ivar_series = _convert_to_var_series_and_validate_index(var, ct.index)
            ivalue_series = _convert_to_series_and_validate_index(value, ct.index)
            return pd.Series(
                index=ct.index,
                data=[
                    _add_enforced_linear_constraint_to_helper(
                        expr,
                        self.__helper,
                        ivar_series[i],
                        ivalue_series[i],
                        f"{name}[{i}]",
                    )
                    for (i, expr) in zip(ct.index, ct)
                ],
            )
        else:
            raise TypeError("Not supported: Model.add_enforced(" + str(ct) + ")")

    def enforced_linear_constraint_from_index(
        self, index: IntegerT
    ) -> EnforcedLinearConstraint:
        """Rebuilds an enforced linear constraint object from the model and its index."""
        return EnforcedLinearConstraint(self.__helper, index=index)

    # Objective.
    def minimize(self, linear_expr: LinearExprT) -> None:
        """Minimizes the given objective."""
        self.__optimize(linear_expr, False)

    def maximize(self, linear_expr: LinearExprT) -> None:
        """Maximizes the given objective."""
        self.__optimize(linear_expr, True)

    def __optimize(self, linear_expr: LinearExprT, maximize: bool) -> None:
        """Defines the objective."""
        self.helper.clear_objective()
        self.__helper.set_maximize(maximize)
        if mbn.is_a_number(linear_expr):
            self.helper.set_objective_offset(linear_expr)
        elif isinstance(linear_expr, Variable):
            self.helper.set_var_objective_coefficient(linear_expr.index, 1.0)
        elif isinstance(linear_expr, LinearExpr):
            flat_expr = _as_flat_linear_expression(linear_expr)
            # pylint: disable=protected-access
            self.helper.set_objective_offset(flat_expr._offset)
            self.helper.set_objective_coefficients(
                flat_expr._variable_indices, flat_expr._coefficients
            )
        else:
            raise TypeError(f"Not supported: Model.minimize/maximize({linear_expr})")

    @property
    def objective_offset(self) -> np.double:
        """Returns the fixed offset of the objective."""
        return self.__helper.objective_offset()

    @objective_offset.setter
    def objective_offset(self, value: NumberT) -> None:
        self.__helper.set_objective_offset(value)

    def objective_expression(self) -> "_LinearExpression":
        """Returns the expression to optimize."""
        return _as_flat_linear_expression(
            sum(
                variable * self.__helper.var_objective_coefficient(variable.index)
                for variable in self.get_variables()
                if self.__helper.var_objective_coefficient(variable.index) != 0.0
            )
            + self.__helper.objective_offset()
        )

    # Hints.
    def clear_hints(self):
        """Clears all solution hints."""
        self.__helper.clear_hints()

    def add_hint(self, var: Variable, value: NumberT) -> None:
        """Adds var == value as a hint to the model.

        Args:
          var: The variable of the hint
          value: The value of the hint

        Note that variables must not appear more than once in the list of hints.
        """
        self.__helper.add_hint(var.index, value)

    # Input/Output
    def export_to_lp_string(self, obfuscate: bool = False) -> str:
        options: mbh.MPModelExportOptions = mbh.MPModelExportOptions()
        options.obfuscate = obfuscate
        return self.__helper.export_to_lp_string(options)

    def export_to_mps_string(self, obfuscate: bool = False) -> str:
        options: mbh.MPModelExportOptions = mbh.MPModelExportOptions()
        options.obfuscate = obfuscate
        return self.__helper.export_to_mps_string(options)

    def export_to_proto(self) -> linear_solver_pb2.MPModelProto:
        """Exports the optimization model to a ProtoBuf format."""
        return mbh.to_mpmodel_proto(self.__helper)

    def import_from_mps_string(self, mps_string: str) -> bool:
        """Reads a model from a MPS string."""
        return self.__helper.import_from_mps_string(mps_string)

    def import_from_mps_file(self, mps_file: str) -> bool:
        """Reads a model from a .mps file."""
        return self.__helper.import_from_mps_file(mps_file)

    def import_from_lp_string(self, lp_string: str) -> bool:
        """Reads a model from a LP string."""
        return self.__helper.import_from_lp_string(lp_string)

    def import_from_lp_file(self, lp_file: str) -> bool:
        """Reads a model from a .lp file."""
        return self.__helper.import_from_lp_file(lp_file)

    def import_from_proto_file(self, proto_file: str) -> bool:
        """Reads a model from a proto file."""
        return self.__helper.read_model_from_proto_file(proto_file)

    def export_to_proto_file(self, proto_file: str) -> bool:
        """Writes a model to a proto file."""
        return self.__helper.write_model_to_proto_file(proto_file)

    # Model getters and Setters

    @property
    def num_variables(self) -> int:
        """Returns the number of variables in the model."""
        return self.__helper.num_variables()

    @property
    def num_constraints(self) -> int:
        """The number of constraints in the model."""
        return self.__helper.num_constraints()

    @property
    def name(self) -> str:
        """The name of the model."""
        return self.__helper.name()

    @name.setter
    def name(self, name: str):
        self.__helper.set_name(name)

    @property
    def helper(self) -> mbh.ModelBuilderHelper:
        """Returns the model builder helper."""
        return self.__helper


class Solver:
    """Main solver class.

    The purpose of this class is to search for a solution to the model provided
    to the solve() method.

    Once solve() is called, this class allows inspecting the solution found
    with the value() method, as well as general statistics about the solve
    procedure.
    """

    def __init__(self, solver_name: str):
        self.__solve_helper: mbh.ModelSolverHelper = mbh.ModelSolverHelper(solver_name)
        self.log_callback: Optional[Callable[[str], None]] = None

    def solver_is_supported(self) -> bool:
        """Checks whether the requested solver backend was found."""
        return self.__solve_helper.solver_is_supported()

    # Solver backend and parameters.
    def set_time_limit_in_seconds(self, limit: NumberT) -> None:
        """Sets a time limit for the solve() call."""
        self.__solve_helper.set_time_limit_in_seconds(limit)

    def set_solver_specific_parameters(self, parameters: str) -> None:
        """Sets parameters specific to the solver backend."""
        self.__solve_helper.set_solver_specific_parameters(parameters)

    def enable_output(self, enabled: bool) -> None:
        """Controls the solver backend logs."""
        self.__solve_helper.enable_output(enabled)

    def solve(self, model: Model) -> SolveStatus:
        """Solves a problem and passes each solution to the callback if not null."""
        if self.log_callback is not None:
            self.__solve_helper.set_log_callback(self.log_callback)
        else:
            self.__solve_helper.clear_log_callback()
        self.__solve_helper.solve(model.helper)
        return SolveStatus(self.__solve_helper.status())

    def stop_search(self):
        """Stops the current search asynchronously."""
        self.__solve_helper.interrupt_solve()

    def value(self, expr: LinearExprT) -> np.double:
        """Returns the value of a linear expression after solve."""
        if not self.__solve_helper.has_solution():
            return pd.NA
        if mbn.is_a_number(expr):
            return expr
        elif isinstance(expr, Variable):
            return self.__solve_helper.var_value(expr.index)
        elif isinstance(expr, LinearExpr):
            flat_expr = _as_flat_linear_expression(expr)
            return self.__solve_helper.expression_value(
                flat_expr._variable_indices,
                flat_expr._coefficients,
                flat_expr._offset,
            )
        else:
            raise TypeError(f"Unknown expression {expr!r} of type {type(expr)}")

    def values(self, variables: _IndexOrSeries) -> pd.Series:
        """Returns the values of the input variables.

        If `variables` is a `pd.Index`, then the output will be indexed by the
        variables. If `variables` is a `pd.Series` indexed by the underlying
        dimensions, then the output will be indexed by the same underlying
        dimensions.

        Args:
          variables (Union[pd.Index, pd.Series]): The set of variables from which to
            get the values.

        Returns:
          pd.Series: The values of all variables in the set.
        """
        if not self.__solve_helper.has_solution():
            return _attribute_series(func=lambda v: pd.NA, values=variables)
        return _attribute_series(
            func=lambda v: self.__solve_helper.var_value(v.index),
            values=variables,
        )

    def reduced_costs(self, variables: _IndexOrSeries) -> pd.Series:
        """Returns the reduced cost of the input variables.

        If `variables` is a `pd.Index`, then the output will be indexed by the
        variables. If `variables` is a `pd.Series` indexed by the underlying
        dimensions, then the output will be indexed by the same underlying
        dimensions.

        Args:
          variables (Union[pd.Index, pd.Series]): The set of variables from which to
            get the values.

        Returns:
          pd.Series: The reduced cost of all variables in the set.
        """
        if not self.__solve_helper.has_solution():
            return _attribute_series(func=lambda v: pd.NA, values=variables)
        return _attribute_series(
            func=lambda v: self.__solve_helper.reduced_cost(v.index),
            values=variables,
        )

    def reduced_cost(self, var: Variable) -> np.double:
        """Returns the reduced cost of a linear expression after solve."""
        if not self.__solve_helper.has_solution():
            return pd.NA
        return self.__solve_helper.reduced_cost(var.index)

    def dual_values(self, constraints: _IndexOrSeries) -> pd.Series:
        """Returns the dual values of the input constraints.

        If `constraints` is a `pd.Index`, then the output will be indexed by the
        constraints. If `constraints` is a `pd.Series` indexed by the underlying
        dimensions, then the output will be indexed by the same underlying
        dimensions.

        Args:
          constraints (Union[pd.Index, pd.Series]): The set of constraints from
            which to get the dual values.

        Returns:
          pd.Series: The dual_values of all constraints in the set.
        """
        if not self.__solve_helper.has_solution():
            return _attribute_series(func=lambda v: pd.NA, values=constraints)
        return _attribute_series(
            func=lambda v: self.__solve_helper.dual_value(v.index),
            values=constraints,
        )

    def dual_value(self, ct: LinearConstraint) -> np.double:
        """Returns the dual value of a linear constraint after solve."""
        if not self.__solve_helper.has_solution():
            return pd.NA
        return self.__solve_helper.dual_value(ct.index)

    def activity(self, ct: LinearConstraint) -> np.double:
        """Returns the activity of a linear constraint after solve."""
        if not self.__solve_helper.has_solution():
            return pd.NA
        return self.__solve_helper.activity(ct.index)

    @property
    def objective_value(self) -> np.double:
        """Returns the value of the objective after solve."""
        if not self.__solve_helper.has_solution():
            return pd.NA
        return self.__solve_helper.objective_value()

    @property
    def best_objective_bound(self) -> np.double:
        """Returns the best lower (upper) bound found when min(max)imizing."""
        if not self.__solve_helper.has_solution():
            return pd.NA
        return self.__solve_helper.best_objective_bound()

    @property
    def status_string(self) -> str:
        """Returns additional information of the last solve.

        It can describe why the model is invalid.
        """
        return self.__solve_helper.status_string()

    @property
    def wall_time(self) -> np.double:
        return self.__solve_helper.wall_time()

    @property
    def user_time(self) -> np.double:
        return self.__solve_helper.user_time()


# The maximum number of terms to display in a linear expression's repr.
_MAX_LINEAR_EXPRESSION_REPR_TERMS = 5


@dataclasses.dataclass(repr=False, eq=False, frozen=True)
class _LinearExpression(LinearExpr):
    """For variables x, an expression: offset + sum_{i in I} coeff_i * x_i."""

    __slots__ = ("_variable_indices", "_coefficients", "_offset", "_helper")

    _variable_indices: npt.NDArray[np.int32]
    _coefficients: npt.NDArray[np.double]
    _offset: float
    _helper: Optional[mbh.ModelBuilderHelper]

    @property
    def variable_indices(self) -> npt.NDArray[np.int32]:
        return self._variable_indices

    @property
    def coefficients(self) -> npt.NDArray[np.double]:
        return self._coefficients

    @property
    def constant(self) -> float:
        return self._offset

    @property
    def helper(self) -> Optional[mbh.ModelBuilderHelper]:
        return self._helper

    def __repr__(self):
        return self.__str__()

    def __str__(self):
        if self._helper is None:
            return str(self._offset)

        result = []
        for index, coeff in zip(self.variable_indices, self.coefficients):
            if len(result) >= _MAX_LINEAR_EXPRESSION_REPR_TERMS:
                result.append(" + ...")
                break
            var_name = Variable(self._helper, index, None, None, None).name
            if not result and mbn.is_one(coeff):
                result.append(var_name)
            elif not result and mbn.is_minus_one(coeff):
                result.append(f"-{var_name}")
            elif not result:
                result.append(f"{coeff} * {var_name}")
            elif mbn.is_one(coeff):
                result.append(f" + {var_name}")
            elif mbn.is_minus_one(coeff):
                result.append(f" - {var_name}")
            elif coeff > 0.0:
                result.append(f" + {coeff} * {var_name}")
            elif coeff < 0.0:
                result.append(f" - {-coeff} * {var_name}")

        if not result:
            return f"{self.constant}"
        if self.constant > 0:
            result.append(f" + {self.constant}")
        elif self.constant < 0:
            result.append(f" - {-self.constant}")
        return "".join(result)


def _sum_as_flat_linear_expression(
    to_process: List[Tuple[LinearExprT, float]], offset: float = 0.0
) -> _LinearExpression:
    """Creates a _LinearExpression as the sum of terms."""
    indices = []
    coeffs = []
    helper = None
    while to_process:  # Flatten AST of LinearTypes.
        expr, coeff = to_process.pop()
        if isinstance(expr, _Sum):
            to_process.append((expr._left, coeff))
            to_process.append((expr._right, coeff))
        elif isinstance(expr, Variable):
            indices.append([expr.index])
            coeffs.append([coeff])
            if helper is None:
                helper = expr.helper
        elif mbn.is_a_number(expr):
            offset += coeff * cast(NumberT, expr)
        elif isinstance(expr, _Product):
            to_process.append((expr._expression, coeff * expr._coefficient))
        elif isinstance(expr, _LinearExpression):
            offset += coeff * expr._offset
            if expr._helper is not None:
                indices.append(expr.variable_indices)
                coeffs.append(np.multiply(expr.coefficients, coeff))
                if helper is None:
                    helper = expr._helper
        else:
            raise TypeError(
                "Unrecognized linear expression: " + str(expr) + f" {type(expr)}"
            )

    if helper is not None:
        all_indices: npt.NDArray[np.int32] = np.concatenate(indices, axis=0)
        all_coeffs: npt.NDArray[np.double] = np.concatenate(coeffs, axis=0)
        sorted_indices, sorted_coefficients = helper.sort_and_regroup_terms(
            all_indices, all_coeffs
        )
        return _LinearExpression(sorted_indices, sorted_coefficients, offset, helper)
    else:
        assert not indices
        assert not coeffs
        return _LinearExpression(
            _variable_indices=np.zeros(dtype=np.int32, shape=[0]),
            _coefficients=np.zeros(dtype=np.double, shape=[0]),
            _offset=offset,
            _helper=None,
        )


def _as_flat_linear_expression(base_expr: LinearExprT) -> _LinearExpression:
    """Converts floats, ints and Linear objects to a LinearExpression."""
    if isinstance(base_expr, _LinearExpression):
        return base_expr
    return _sum_as_flat_linear_expression(to_process=[(base_expr, 1.0)], offset=0.0)


@dataclasses.dataclass(repr=False, eq=False, frozen=True)
class _Sum(LinearExpr):
    """Represents the (deferred) sum of two expressions."""

    __slots__ = ("_left", "_right")

    _left: LinearExprT
    _right: LinearExprT

    def __repr__(self):
        return self.__str__()

    def __str__(self):
        return str(_as_flat_linear_expression(self))


@dataclasses.dataclass(repr=False, eq=False, frozen=True)
class _Product(LinearExpr):
    """Represents the (deferred) product of an expression by a constant."""

    __slots__ = ("_expression", "_coefficient")

    _expression: LinearExpr
    _coefficient: NumberT

    def __repr__(self):
        return self.__str__()

    def __str__(self):
        return str(_as_flat_linear_expression(self))


def _get_index(obj: _IndexOrSeries) -> pd.Index:
    """Returns the indices of `obj` as a `pd.Index`."""
    if isinstance(obj, pd.Series):
        return obj.index
    return obj


def _attribute_series(
    *,
    func: Callable[[_VariableOrConstraint], NumberT],
    values: _IndexOrSeries,
) -> pd.Series:
    """Returns the attributes of `values`.

    Args:
      func: The function to call for getting the attribute data.
      values: The values that the function will be applied (element-wise) to.

    Returns:
      pd.Series: The attribute values.
    """
    return pd.Series(
        data=[func(v) for v in values],
        index=_get_index(values),
    )


def _convert_to_series_and_validate_index(
    value_or_series: Union[bool, NumberT, pd.Series], index: pd.Index
) -> pd.Series:
    """Returns a pd.Series of the given index with the corresponding values.

    Args:
      value_or_series: the values to be converted (if applicable).
      index: the index of the resulting pd.Series.

    Returns:
      pd.Series: The set of values with the given index.

    Raises:
      TypeError: If the type of `value_or_series` is not recognized.
      ValueError: If the index does not match.
    """
    if mbn.is_a_number(value_or_series) or isinstance(value_or_series, bool):
        result = pd.Series(data=value_or_series, index=index)
    elif isinstance(value_or_series, pd.Series):
        if value_or_series.index.equals(index):
            result = value_or_series
        else:
            raise ValueError("index does not match")
    else:
        raise TypeError("invalid type={}".format(type(value_or_series)))
    return result


def _convert_to_var_series_and_validate_index(
    var_or_series: Union["Variable", pd.Series], index: pd.Index
) -> pd.Series:
    """Returns a pd.Series of the given index with the corresponding values.

    Args:
      var_or_series: the variables to be converted (if applicable).
      index: the index of the resulting pd.Series.

    Returns:
      pd.Series: The set of values with the given index.

    Raises:
      TypeError: If the type of `value_or_series` is not recognized.
      ValueError: If the index does not match.
    """
    if isinstance(var_or_series, Variable):
        result = pd.Series(data=var_or_series, index=index)
    elif isinstance(var_or_series, pd.Series):
        if var_or_series.index.equals(index):
            result = var_or_series
        else:
            raise ValueError("index does not match")
    else:
        raise TypeError("invalid type={}".format(type(var_or_series)))
    return result


# Compatibility.
ModelBuilder = Model
ModelSolver = Solver
