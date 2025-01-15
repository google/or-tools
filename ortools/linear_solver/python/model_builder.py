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

import math
import numbers
import typing
from typing import Callable, Optional, Union

import numpy as np
import pandas as pd

from ortools.linear_solver import linear_solver_pb2
from ortools.linear_solver.python import model_builder_helper as mbh
from ortools.linear_solver.python import model_builder_numbers as mbn

# Custom types.
NumberT = Union[int, float, numbers.Real, np.number]
IntegerT = Union[int, numbers.Integral, np.integer]
LinearExprT = Union[mbh.LinearExpr, NumberT]
ConstraintT = Union[mbh.BoundedLinearExpression, bool]
_IndexOrSeries = Union[pd.Index, pd.Series]
_VariableOrConstraint = Union["LinearConstraint", mbh.Variable]

# Forward solve statuses.
AffineExpr = mbh.AffineExpr
BoundedLinearExpression = mbh.BoundedLinearExpression
FlatExpr = mbh.FlatExpr
LinearExpr = mbh.LinearExpr
SolveStatus = mbh.SolveStatus
Variable = mbh.Variable


def _add_linear_constraint_to_helper(
    bounded_expr: Union[bool, mbh.BoundedLinearExpression],
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
    if isinstance(bounded_expr, mbh.BoundedLinearExpression):
        c = LinearConstraint(helper)
        # pylint: disable=protected-access
        helper.add_terms_to_constraint(c.index, bounded_expr.vars, bounded_expr.coeffs)
        helper.set_constraint_lower_bound(c.index, bounded_expr.lower_bound)
        helper.set_constraint_upper_bound(c.index, bounded_expr.upper_bound)
        # pylint: enable=protected-access
        if name is not None:
            helper.set_constraint_name(c.index, name)
        return c
    raise TypeError(f"invalid type={type(bounded_expr).__name__!r}")


def _add_enforced_linear_constraint_to_helper(
    bounded_expr: Union[bool, mbh.BoundedLinearExpression],
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
    if isinstance(bounded_expr, mbh.BoundedLinearExpression):
        c = EnforcedLinearConstraint(helper)
        c.indicator_variable = var
        c.indicator_value = value
        helper.add_terms_to_enforced_constraint(
            c.index, bounded_expr.vars, bounded_expr.coeffs
        )
        helper.set_enforced_constraint_lower_bound(c.index, bounded_expr.lower_bound)
        helper.set_enforced_constraint_upper_bound(c.index, bounded_expr.upper_bound)
        if name is not None:
            helper.set_constraint_name(c.index, name)
        return c

    raise TypeError(f"invalid type={type(bounded_expr).__name__!r}")


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

    def __hash__(self):
        return hash((self.__helper, self.__index))

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
        return Variable(self.__helper, enforcement_var_index)

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
            func=lambda c: mbh.FlatExpr(
                # pylint: disable=g-complex-comprehension
                [
                    Variable(self.__helper, var_id)
                    for var_id in c.helper.constraint_var_indices(c.index)
                ],
                c.helper.constraint_coefficients(c.index),
                0.0,
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
        if name:
            return Variable(self.__helper, lb, ub, is_integer, name)
        else:
            return Variable(self.__helper, lb, ub, is_integer)

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
            raise ValueError(f"name={name!r} is not a valid identifier")
        if (
            mbn.is_a_number(lower_bounds)
            and mbn.is_a_number(upper_bounds)
            and lower_bounds > upper_bounds
        ):
            raise ValueError(
                f"lower_bound={lower_bounds} is greater than"
                f" upper_bound={upper_bounds} for variable set={name!r}"
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
                f"ceil(lower_bound={lower_bounds})={math.ceil(lower_bounds)}"
                f" is greater than floor({upper_bounds}) = {math.floor(upper_bounds)}"
                f" for variable set={name!r}"
            )
        lower_bounds = _convert_to_series_and_validate_index(lower_bounds, index)
        upper_bounds = _convert_to_series_and_validate_index(upper_bounds, index)
        is_integrals = _convert_to_series_and_validate_index(is_integral, index)
        return pd.Series(
            index=index,
            data=[
                # pylint: disable=g-complex-comprehension
                Variable(
                    self.__helper,
                    lower_bounds[i],
                    upper_bounds[i],
                    is_integrals[i],
                    f"{name}[{i}]",
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
        return Variable(self.__helper, index)

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
        elif isinstance(linear_expr, LinearExpr):
            flat_expr = mbh.FlatExpr(linear_expr)
            # pylint: disable=protected-access
            self.__helper.set_constraint_lower_bound(ct.index, lb - flat_expr.offset)
            self.__helper.set_constraint_upper_bound(ct.index, ub - flat_expr.offset)
            self.__helper.add_terms_to_constraint(
                ct.index, flat_expr.vars, flat_expr.coeffs
            )
        else:
            raise TypeError(
                "Not supported:"
                f" Model.add_linear_constraint({type(linear_expr).__name__!r})"
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
        if isinstance(ct, mbh.BoundedLinearExpression):
            return _add_linear_constraint_to_helper(ct, self.__helper, name)
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
            raise TypeError(f"Not supported: Model.add({type(ct).__name__!r})")

    def linear_constraint_from_index(self, index: IntegerT) -> LinearConstraint:
        """Rebuilds a linear constraint object from the model and its index."""
        return LinearConstraint(self.__helper, index=index)

    # Enforced Linear constraints.

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
        elif isinstance(linear_expr, LinearExpr):
            flat_expr = mbh.FlatExpr(linear_expr)
            # pylint: disable=protected-access
            self.__helper.set_constraint_lower_bound(ct.index, lb - flat_expr.offset)
            self.__helper.set_constraint_upper_bound(ct.index, ub - flat_expr.offset)
            self.__helper.add_terms_to_constraint(
                ct.index, flat_expr.vars, flat_expr.coeffs
            )
        else:
            raise TypeError(
                "Not supported:"
                f" Model.add_enforced_linear_constraint({type(linear_expr).__name__!r})"
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
        if isinstance(ct, mbh.BoundedLinearExpression):
            return _add_enforced_linear_constraint_to_helper(
                ct, self.__helper, var, value, name
            )
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
            raise TypeError(f"Not supported: Model.add_enforced({type(ct).__name__!r}")

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
            flat_expr = mbh.FlatExpr(linear_expr)
            # pylint: disable=protected-access
            self.helper.set_objective_offset(flat_expr.offset)
            var_indices = [var.index for var in flat_expr.vars]
            self.helper.set_objective_coefficients(var_indices, flat_expr.coeffs)
        else:
            raise TypeError(
                "Not supported:"
                f" Model.minimize/maximize({type(linear_expr).__name__!r})"
            )

    @property
    def objective_offset(self) -> np.double:
        """Returns the fixed offset of the objective."""
        return self.__helper.objective_offset()

    @objective_offset.setter
    def objective_offset(self, value: NumberT) -> None:
        self.__helper.set_objective_offset(value)

    def objective_expression(self) -> "LinearExpr":
        """Returns the expression to optimize."""
        variables: list[Variable] = []
        coefficients: list[numbers.Real] = []
        for variable in self.get_variables():
            coeff = self.__helper.var_objective_coefficient(variable.index)
            if coeff != 0.0:
                variables.append(variable)
                coefficients.append(coeff)
        return mbh.FlatExpr(variables, coefficients, self.__helper.objective_offset())

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

    def write_to_mps_file(self, filename: str, obfuscate: bool = False) -> bool:
        options: mbh.MPModelExportOptions = mbh.MPModelExportOptions()
        options.obfuscate = obfuscate
        return self.__helper.write_to_mps_file(filename, options)

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
        """Reads a model from a LP string.

        Note that this code is very limited, and will not support any real lp.
        It is only intented to be use to parse test lp problems.

        Args:
          lp_string: The LP string to import.

        Returns:
          True if the import was successful.
        """
        return self.__helper.import_from_lp_string(lp_string)

    def import_from_lp_file(self, lp_file: str) -> bool:
        """Reads a model from a .lp file.

        Note that this code is very limited, and will not support any real lp.
        It is only intented to be use to parse test lp problems.

        Args:
          lp_file: The LP file to import.

        Returns:
          True if the import was successful.
        """
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
        elif isinstance(expr, LinearExpr):
            return self.__solve_helper.expression_value(expr)
        else:
            raise TypeError(f"Unknown expression {type(expr).__name__!r}")

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
            func=lambda v: self.__solve_helper.variable_value(v.index),
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
        raise TypeError("invalid type={type(value_or_series).__name!r}")
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
        raise TypeError("invalid type={type(value_or_series).__name!r}")
    return result


# Compatibility.
ModelBuilder = Model
ModelSolver = Solver
