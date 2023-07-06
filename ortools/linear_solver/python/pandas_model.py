# Copyright 2010-2022 Google LLC
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

"""Pandas-native API for optimization."""

import abc
import collections
import dataclasses
import enum
import math
import sys
import typing
from typing import Callable, Mapping, NoReturn, Optional, Union

import numpy as np
import pandas as pd

from ortools.linear_solver import linear_solver_pb2
from ortools.linear_solver.python import model_builder_helper as mbh

_Number = Union[int, float, np.number]
_LinearType = Union[_Number, "_LinearBase"]

# The maximum number of terms to display in a linear expression's repr.
_MAX_LINEAR_EXPRESSION_REPR_TERMS = 5


class _LinearBase(metaclass=abc.ABCMeta):
    """Interface for types that can build linear expressions with +, -, * and /.

    Classes derived from LinearBase (plus float and int scalars) are used to build
    expression trees describing a linear expression. Operation nodes of the
    expression tree include:

      * _Sum: describes a deferred sum of LinearTypes.
      * _Product: describes a deferred product of a scalar and a LinearType.

    Leaf nodes of the expression tree include:

      * float and int scalars.
      * Variable: a single variable.
      * LinearExpression: a flattened form of a linear expression.
    """

    def __add__(self, arg: _LinearType) -> "_Sum":
        return _Sum(self, arg)

    def __radd__(self, arg: _LinearType) -> "_Sum":
        return self.__add__(arg)

    def __sub__(self, arg: _LinearType) -> "_Sum":
        return _Sum(self, -arg)

    def __rsub__(self, arg: _LinearType) -> "_Sum":
        return _Sum(-self, arg)

    def __mul__(self, arg: _Number) -> "_Product":
        return _Product(self, arg)

    def __rmul__(self, arg: _Number) -> "_Product":
        return self.__mul__(arg)

    def __truediv__(self, coeff: _Number) -> "_Product":
        return self.__mul__(1.0 / coeff)

    def __neg__(self) -> "_Product":
        return _Product(self, -1)

    def __bool__(self) -> NoReturn:
        raise NotImplementedError(
            f"Cannot use a LinearExpression as a Boolean value: {self}"
        )

    def __eq__(self, arg: _LinearType) -> "_BoundedLinearExpression":
        return _BoundedLinearExpression(
            _expression=self - arg, _lower_bound=0, _upper_bound=0
        )

    def __ge__(self, arg: _LinearType) -> "_BoundedLinearExpression":
        return _BoundedLinearExpression(
            _expression=self - arg, _lower_bound=0, _upper_bound=math.inf
        )

    def __le__(self, arg: _LinearType) -> "_BoundedLinearExpression":
        return _BoundedLinearExpression(
            _expression=self - arg, _lower_bound=-math.inf, _upper_bound=0
        )


@dataclasses.dataclass(repr=False, eq=False, frozen=True)
class _LinearExpression(_LinearBase):
    """For variables x, an expression: offset + sum_{i in I} coeff_i * x_i."""

    __slots__ = ("_terms", "_offset")

    _terms: Mapping["_Variable", float]
    _offset: float

    def __repr__(self):
        return self.__str__()

    def __str__(self):
        result = [str(self._offset)]
        sorted_keys = sorted(self._terms.keys(), key=str)
        num_displayed_terms = 0
        for variable in sorted_keys:
            if num_displayed_terms == _MAX_LINEAR_EXPRESSION_REPR_TERMS:
                result.append(" + ...")
                break
            coefficient = self._terms[variable]
            if coefficient == 0.0:
                continue
            if coefficient > 0:
                result.append(" + ")
            else:
                result.append(" - ")
            if abs(coefficient) != 1.0:
                result.append(f"{abs(coefficient)} * ")
            result.append(f"{variable}")
            num_displayed_terms += 1
        return "".join(result)


def _as_flat_linear_expression(base: _LinearType) -> _LinearExpression:
    """Converts floats, ints and Linear objects to a LinearExpression."""
    # pylint: disable=protected-access
    if isinstance(base, _LinearExpression):
        return base
    terms = collections.defaultdict(lambda: 0.0)
    offset: float = 0.0
    to_process = [(base, 1.0)]
    while to_process:  # Flatten AST of LinearTypes.
        expr, coeff = to_process.pop()
        if isinstance(expr, _Sum):
            to_process.append((expr._left, coeff))
            to_process.append((expr._right, coeff))
        elif isinstance(expr, _Variable):
            terms[expr] += coeff
        elif isinstance(expr, (int, float, np.number)):  # i.e. is _Number
            offset += coeff * expr
        elif isinstance(expr, _Product):
            to_process.append((expr._expression, coeff * expr._coefficient))
        elif isinstance(expr, _LinearExpression):
            offset += coeff * expr._offset
            for variable, variable_coefficient in expr._terms.items():
                terms[variable] += coeff * variable_coefficient
        else:
            raise TypeError(
                "Unrecognized linear expression: " + str(expr) + f" {type(expr)}"
            )
    return _LinearExpression(terms, offset)


@dataclasses.dataclass(repr=False, eq=False, frozen=True)
class _Sum(_LinearBase):
    """Represents the (deferred) sum of two expressions."""

    __slots__ = ("_left", "_right")

    _left: _LinearType
    _right: _LinearType

    def __repr__(self):
        return self.__str__()

    def __str__(self):
        return str(_as_flat_linear_expression(self))


@dataclasses.dataclass(repr=False, eq=False, frozen=True)
class _Product(_LinearBase):
    """Represents the (deferred) product of an expression by a constant."""

    __slots__ = ("_expression", "_coefficient")

    _expression: _LinearBase
    _coefficient: _Number

    def __repr__(self):
        return self.__str__()

    def __str__(self):
        return str(_as_flat_linear_expression(self))


@dataclasses.dataclass(repr=False, eq=False, frozen=True)
class _Variable(_LinearBase):
    """A variable (continuous or integral).

    A Variable is an object that can take on any value within its domain.
    Variables (e.g. x and y) appear in constraints like:

        x + y >= 5

    Solving a model is equivalent to finding, for each variable, a value in its
    domain, such that all constraints are satisfied.
    """

    __slots__ = ("_helper", "_index")

    _helper: mbh.ModelBuilderHelper
    _index: int

    def __str__(self):
        return self._name

    def __repr__(self):
        return self.__str__()

    @property
    def _name(self) -> str:
        var_name = self._helper.var_name(self._index)
        if var_name:
            return var_name
        return f"variable#{self._index}"

    @property
    def _lower_bound(self) -> _Number:
        """Returns the lower bound of the variable."""
        return self._helper.var_lower_bound(self._index)

    @property
    def _upper_bound(self) -> _Number:
        """Returns the upper bound of the variable."""
        return self._helper.var_upper_bound(self._index)

    @typing.overload
    def __eq__(self, rhs: "_Variable") -> "_VarEqVar":
        ...

    def __eq__(self, rhs: _LinearType) -> "_BoundedLinearBase":
        if isinstance(rhs, _Variable):
            return _VarEqVar(self, rhs)
        return _BoundedLinearExpression(
            _expression=self - rhs, _lower_bound=0, _upper_bound=0
        )

    def __hash__(self):
        return hash((self._helper, self._index))


def _create_variable(
    helper: mbh.ModelBuilderHelper,
    *,
    name: str,
    lower_bound: _Number,
    upper_bound: _Number,
    is_integral: bool,
) -> _Variable:
    """Creates a new variable in the helper.

    Args:
      helper (mbh.ModelBuilderHelper): The helper to create the variable.
      name (str): The name of the variable.
      lower_bound (Union[int, float]): The lower bound of the variable.
      upper_bound (Union[int, float]): The upper bound of the variable.
      is_integral (bool): Whether the variable can only take integer values.

    Returns:
      _Variable: A reference to the variable in the helper.
    """
    index = helper.add_var()
    helper.set_var_lower_bound(index, lower_bound)
    helper.set_var_upper_bound(index, upper_bound)
    helper.set_var_integrality(index, is_integral)
    helper.set_var_name(index, name)
    return _Variable(helper, index)


class _BoundedLinearBase(metaclass=abc.ABCMeta):
    """Interface for types that can build bounded linear (boolean) expressions.

    Classes derived from _BoundedLinearBase are used to build linear constraints
    to be satisfied.

      * BoundedLinearExpression: a linear expression with upper and lower bounds.
      * VarEqVar: an equality comparison between two variables.
    """

    @abc.abstractmethod
    def _create_linear_constraint(
        self, helper: mbh.ModelBuilderHelper, name: str
    ) -> "_LinearConstraint":
        """Creates a new linear constraint in the helper.

        Args:
          helper (mbh.ModelBuilderHelper): The helper to create the constraint.
          name (str): The name of the linear constraint.

        Returns:
          _LinearConstraint: A reference to the linear constraint in the helper.
        """


def _create_linear_constraint(
    constraint: Union[bool, _BoundedLinearBase],
    helper: mbh.ModelBuilderHelper,
    name: str,
):
    """Creates a new linear constraint in the helper.

    It handles boolean values (which might arise in the construction of
    _BoundedLinearExpressions).

    Args:
      constraint: The constraint to be created.
      helper: The helper to create the constraint.
      name: The name of the constraint to be created.

    Returns:
      _LinearConstraint: a constraint in the helper corresponding to the input.

    Raises:
      TypeError: If constraint is an invalid type.
    """
    if isinstance(constraint, bool):
        bound = 1  # constraint that is always infeasible: 1 <= nothing <= 1
        if constraint:
            bound = 0  # constraint that is always feasible: 0 <= nothing <= 0
        index = helper.add_linear_constraint()
        helper.set_constraint_lower_bound(index, bound)
        helper.set_constraint_upper_bound(index, bound)
        helper.set_constraint_name(index, name)
        return _LinearConstraint(helper, index)
    if isinstance(constraint, _BoundedLinearBase):
        # pylint: disable=protected-access
        return constraint._create_linear_constraint(helper, name)
    raise TypeError("invalid type={}".format(type(constraint)))


@dataclasses.dataclass(repr=False, eq=False, frozen=True)
class _BoundedLinearExpression(_BoundedLinearBase):
    """Represents a linear constraint: `lb <= linear expression <= ub`."""

    __slots__ = ("_expression", "_lower_bound", "_upper_bound")

    _expression: _LinearBase
    _lower_bound: _Number
    _upper_bound: _Number

    def __str__(self):
        if math.isfinite(self._lower_bound) and math.isfinite(self._upper_bound):
            if self._lower_bound == self._upper_bound:
                return f"{self._expression} == {self._lower_bound}"
            return f"{self._lower_bound} <= {self._expression} <= {self._upper_bound}"
        if math.isfinite(self._lower_bound):
            return f"{self._expression} >= {self._lower_bound}"
        if math.isfinite(self._upper_bound):
            return f"{self._expression} <= {self._upper_bound}"
        return f"{self._expression} free"

    def __repr__(self):
        return self.__str__()

    def __bool__(self) -> NoReturn:
        raise NotImplementedError(
            f"Cannot use a BoundedLinearExpression {self} as a Boolean value. If"
            " this message is due to code like `x >= 0` where x is a `pd.Series`,"
            " you can write it as `x.apply(lambda expr: expr >= 0)` instead."
        )

    def _create_linear_constraint(
        self, helper: mbh.ModelBuilderHelper, name: str
    ) -> "_LinearConstraint":
        index = helper.add_linear_constraint()
        expr = _as_flat_linear_expression(self._expression)
        # pylint: disable=protected-access
        for variable, coeff in expr._terms.items():
            helper.add_term_to_constraint(index, variable._index, coeff)
        helper.set_constraint_lower_bound(index, self._lower_bound - expr._offset)
        helper.set_constraint_upper_bound(index, self._upper_bound - expr._offset)
        # pylint: enable=protected-access
        helper.set_constraint_name(index, name)
        return _LinearConstraint(helper, index)


@dataclasses.dataclass(repr=False, eq=False, frozen=True)
class _VarEqVar(_BoundedLinearBase):
    """The result of the equality comparison between two Variables.

    We use an object here to delay the evaluation of equality so that we can use
    the operator== in two use-cases:

      1. when the user want to test that two Variable values reference the same
         variable. This is supported by having this object support implicit
         conversion to bool.

      2. when the user want to use the equality to create a constraint of equality
         between two variables.
    """

    __slots__ = ("_left", "_right")

    _left: _Variable
    _right: _Variable

    def __str__(self):
        return f"{self._left} == {self._right}"

    def __repr__(self):
        return self.__str__()

    def __bool__(self) -> bool:
        return hash(self._left) == hash(self._right)

    def _create_linear_constraint(
        self, helper: mbh.ModelBuilderHelper, name: str
    ) -> "_LinearConstraint":
        index = helper.add_linear_constraint()
        helper.set_constraint_lower_bound(index, 0.0)
        helper.set_constraint_upper_bound(index, 0.0)
        # pylint: disable=protected-access
        helper.add_term_to_constraint(index, self._left._index, 1.0)
        helper.add_term_to_constraint(index, self._right._index, -1.0)
        # pylint: enable=protected-access
        helper.set_constraint_name(index, name)
        return _LinearConstraint(helper, index)


@dataclasses.dataclass(repr=False, eq=False, frozen=True)
class _LinearConstraint:
    """A linear constraint for an optimization model.

    A LinearConstraint adds the following restriction on feasible solutions to an
    optimization model:

      lb <= sum_{i in I} a_i * x_i <= ub

    where x_i are the variables of the model. lb == ub is allowed and represents
    the equality constraint: sum_{i in I} a_i * x_i == b.
    """

    __slots__ = ("_helper", "_index")

    _helper: mbh.ModelBuilderHelper
    _index: int

    @property
    def _lower_bound(self) -> _Number:
        return self._helper.constraint_lower_bound(self._index)

    @property
    def _upper_bound(self) -> _Number:
        return self._helper.constraint_upper_bound(self._index)

    @property
    def _name(self) -> str:
        constraint_name = self._helper.constraint_name(self._index)
        if constraint_name:
            return constraint_name
        return f"linear_constraint#{self._index}"

    def __str__(self):
        return self._name

    def __repr__(self):
        return self.__str__()


_IndexOrSeries = Union[pd.Index, pd.Series]
_VariableOrConstraint = Union[_LinearConstraint, _Variable]


def _get_index(obj: _IndexOrSeries) -> pd.Index:
    """Returns the indices of `obj` as a `pd.Index`."""
    if isinstance(obj, pd.Series):
        return obj.index
    return obj


def _attribute_series(
    *,
    func: Callable[[_VariableOrConstraint], _Number],
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
    value_or_series: Union[bool, _Number, pd.Series], index: pd.Index
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
    if isinstance(value_or_series, (bool, int, float, np.number)):  # i.e. scalar
        result = pd.Series(data=value_or_series, index=index)
    elif isinstance(value_or_series, pd.Series):
        if value_or_series.index.equals(index):
            result = value_or_series
        else:
            raise ValueError("index does not match")
    else:
        raise TypeError("invalid type={}".format(type(value_or_series)))
    return result


@enum.unique
class ObjectiveSense(enum.Enum):
    """The sense (maximize or minimize) of the optimization objective."""

    MINIMIZE = enum.auto()
    MAXIMIZE = enum.auto()


class OptimizationModel:
    """Pandas-like API for optimization models.

    It is a wrapper around ortools, providing indexing functionality through
    Pandas for representing index dimensions (such as nodes, edges, skus, etc).
    """

    __slots__ = (
        "_helper",
        "_variables",
        "_linear_constraints",
    )

    def __init__(self, name: str = "") -> None:
        """Initializes an optimization model with the given name."""
        if not name.isidentifier():
            raise ValueError("name={} is not a valid identifier".format(name))
        self._helper: mbh.ModelBuilderHelper = mbh.ModelBuilderHelper()
        self._helper.set_name(name)
        self._variables: dict[str, pd.Series] = {}
        self._linear_constraints: dict[str, pd.Series] = {}

    def __str__(self):
        return (
            f"OptimizationModel(name={self.get_name()}) with the following"
            f" schema:\n{self.get_schema()}"
        )

    def __repr__(self):
        return self.__str__()

    def to_proto(self) -> linear_solver_pb2.MPModelProto:
        """Exports the optimization model to a ProtoBuf format."""
        return mbh.to_mpmodel_proto(self._helper)

    @typing.overload
    def _get_linear_constraints(self, constraints: Optional[pd.Index]) -> pd.Index:
        ...

    @typing.overload
    def _get_linear_constraints(self, constraints: pd.Series) -> pd.Series:
        ...

    def _get_linear_constraints(
        self, constraints: Optional[_IndexOrSeries] = None
    ) -> _IndexOrSeries:
        if constraints is None:
            return self.get_linear_constraints()
        return constraints

    @typing.overload
    def _get_variables(self, variables: Optional[pd.Index]) -> pd.Index:
        ...

    @typing.overload
    def _get_variables(self, variables: pd.Series) -> pd.Series:
        ...

    def _get_variables(
        self, variables: Optional[_IndexOrSeries] = None
    ) -> _IndexOrSeries:
        if variables is None:
            return self.get_variables()
        return variables

    def create_linear_constraints(
        self,
        name: str,
        bounded_exprs: Union[_BoundedLinearBase, pd.Series],
    ) -> pd.Series:
        """Sets a linear constraint set with the `name` based on `bounded_exprs`.

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')
        x = model.create_variables(
            name='x',
            index=pd.Index(range(3)),
            lower_bound=0,
        )

        model.create_linear_constraints(
            name='c',
            bounded_exprs=pd.Series([
                x.dot([10, 4, 5]) <= 600,
                x.dot([2, 2, 6]) <= 300,
                sum(x) <= 100,
            ]),
        )
        ```

        Args:
          name (str): Required. The name of the linear constraint set.
          bounded_exprs (Union[BoundedLinearBase, pd.Series]): Required. The linear
            inequalities defining the constraints, indexed by the underlying
            dimensions of the constraints. If a single BoundedLinearExpression is
            passed in, it will be converted into a `pd.Series` with no underlying
            dimension and with an index value of `0`.

        Returns:
          pd.Series: The constraint set indexed by its corresponding dimensions. It
          is equivalent to `get_linear_constraint_references(name=name)`.

        Raises:
          ValueError: if `name` is not a valid identifier, or already exists.
          TypeError: if `bounded_exprs` has an invalid type.
        """
        if not name.isidentifier():
            raise ValueError("name={} is not a valid identifier".format(name))
        if name in self._linear_constraints:
            raise ValueError(
                "name={} already exists as a set of linear constraints".format(name)
            )
        if isinstance(bounded_exprs, (bool, _BoundedLinearBase)):
            bounded_exprs = pd.Series(bounded_exprs)
        if not isinstance(bounded_exprs, pd.Series):
            raise TypeError("invalid type={}".format(type(bounded_exprs)))
        # Set the new linear constraints.
        self._linear_constraints[name] = pd.Series(
            index=bounded_exprs.index,
            data=[
                _create_linear_constraint(bool_expr, self._helper, f"{name}[{i}]")
                for (i, bool_expr) in zip(bounded_exprs.index, bounded_exprs)
            ],
        )
        return self.get_linear_constraint_references(name=name)

    def create_variables(
        self,
        name: str,
        index: pd.Index,
        lower_bound: Union[_Number, pd.Series] = -math.inf,
        upper_bound: Union[_Number, pd.Series] = math.inf,
        is_integer: Union[bool, pd.Series] = False,
    ) -> pd.Series:
        """Creates a set of (scalar-valued) variables with the given name.

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')

        model.create_variables(
            name='x',
            index=pd.Index(range(3)),
            lower_bound=0,
        )
        ```

        Args:
          name (str): Required. The name of the variable set.
          index (pd.Index): Required. The index to use for the variable set.
          lower_bound (Union[int, float, pd.Series]): Optional. A lower bound for
            variables in the set. If a `pd.Series` is passed in, it will be based on
            the corresponding values of the pd.Series. Defaults to -inf.
          upper_bound (Union[int, float, pd.Series]): Optional. An upper bound for
            variables in the set. If a `pd.Series` is passed in, it will be based on
            the corresponding values of the pd.Series. Defaults to +inf.
          is_integer (bool, pd.Series): Optional. Indicates if the variable can only
            take integer values. If a `pd.Series` is passed in, it will be based on
            the corresponding values of the pd.Series. Defaults to False.

        Returns:
          pd.Series: The variable set indexed by its corresponding dimensions. It is
          equivalent to the result from `get_variable_references(name=name)`.

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
        if name in self._variables:
            raise ValueError("name={} already exists".format(name))
        if (
            isinstance(lower_bound, (int, float, np.number))  # i.e. is _Number
            and isinstance(upper_bound, (int, float, np.number))  # i.e. is _Number
            and lower_bound > upper_bound
        ):
            raise ValueError(
                "lower_bound={} is greater than upper_bound={} for variable set={}".format(
                    lower_bound, upper_bound, name
                )
            )
        if (
            isinstance(is_integer, bool)
            and is_integer
            and isinstance(lower_bound, (int, float, np.number))  # i.e. is _Number
            and isinstance(upper_bound, (int, float, np.number))  # i.e. is _Number
            and math.isfinite(lower_bound)
            and math.isfinite(upper_bound)
            and math.ceil(lower_bound) > math.floor(upper_bound)
        ):
            raise ValueError(
                "ceil(lower_bound={})={}".format(lower_bound, math.ceil(lower_bound))
                + " is greater than floor("
                + "upper_bound={})={}".format(upper_bound, math.floor(upper_bound))
                + " for variable set={}".format(name)
            )
        lower_bounds = _convert_to_series_and_validate_index(lower_bound, index)
        upper_bounds = _convert_to_series_and_validate_index(upper_bound, index)
        is_integers = _convert_to_series_and_validate_index(is_integer, index)
        self._variables[name] = pd.Series(
            index=index,
            data=[
                # pylint: disable=g-complex-comprehension
                _create_variable(
                    helper=self._helper,
                    name=f"{name}[{i}]",
                    lower_bound=lower_bounds[i],
                    upper_bound=upper_bounds[i],
                    is_integral=is_integers[i],
                )
                for i in index
            ],
        )
        return self.get_variable_references(name=name)

    def get_linear_constraints(self, name: Optional[str] = None) -> pd.Index:
        """Gets the set of linear constraints.

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')
        x = model.create_variables(
            name='x',
            index=pd.Index(range(3)),
            lower_bound=0,
        )
        model.create_linear_constraints(
            name='c',
            bounded_exprs=pd.Series([
                x.dot([10, 4, 5]) <= 600,
                x.dot([2, 2, 6]) <= 300,
                sum(x) <= 100,
            ]),
        )

        model.get_linear_constraints()
        ```

        Args:
          name (str): Optional. The name of the linear constraint set. If it is
            unspecified, all linear constraints will be in scope.

        Returns:
          pd.Index: The set of linear constraints.

        Raises:
          KeyError: If name is provided but not found as a linear constraint set.
        """
        if not self._linear_constraints:
            return pd.Index(data=[], dtype=object, name="linear_constraint")
        if name:
            return pd.Index(
                data=self.get_linear_constraint_references(name=name).values,
                name="linear_constraint",
            )
        return pd.concat(
            [
                # pylint: disable=g-complex-comprehension
                pd.Series(
                    dtype=object,
                    index=pd.Index(constraints.values, name="linear_constraint"),
                )
                for constraints in self._linear_constraints.values()
            ]
        ).index

    def get_linear_constraint_expressions(
        self, constraints: Optional[_IndexOrSeries] = None
    ) -> pd.Series:
        """Gets the expressions of all linear constraints in the set.

        If `constraints` is a `pd.Index`, then the output will be indexed by the
        constraints. If `constraints` is a `pd.Series` indexed by the underlying
        dimensions, then the output will be indexed by the same underlying
        dimensions.

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')
        x = model.create_variables(
            name='x',
            index=pd.Index(range(3)),
            lower_bound=0,
        )
        model.create_linear_constraints(
            name='c',
            bounded_exprs=pd.Series([
                x.dot([10, 4, 5]) <= 600,
                x.dot([2, 2, 6]) <= 300,
                sum(x) <= 100,
            ]),
        )

        model.get_linear_constraint_expressions()
        ```

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
                    coefficient * _Variable(self._helper, variable_id)
                    for variable_id, coefficient in zip(
                        # pylint: disable=protected-access
                        c._helper.constraint_var_indices(c._index),
                        c._helper.constraint_coefficients(c._index),
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

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')
        x = model.create_variables(
            name='x',
            index=pd.Index(range(3)),
            lower_bound=0,
        )
        model.create_linear_constraints(
            name='c',
            bounded_exprs=pd.Series([
                x.dot([10, 4, 5]) <= 600,
                x.dot([2, 2, 6]) <= 300,
                sum(x) <= 100,
            ]),
        )

        model.get_linear_constraint_lower_bounds()
        ```

        Args:
          constraints (Union[pd.Index, pd.Series]): Optional. The set of linear
            constraints from which to get the lower bounds. If unspecified, all
            linear constraints will be in scope.

        Returns:
          pd.Series: The lower bounds of all linear constraints in the set.
        """
        return _attribute_series(
            func=lambda c: c._lower_bound,  # pylint: disable=protected-access
            values=self._get_linear_constraints(constraints),
        )

    def get_linear_constraint_references(self, name: str) -> pd.Series:
        """Gets references to all linear constraints in the set.

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')
        x = model.create_variables(
            name='x',
            index=pd.Index(range(3)),
            lower_bound=0,
        )
        model.create_linear_constraints(
            name='c',
            bounded_exprs=pd.Series([
                x.dot([10, 4, 5]) <= 600,
                x.dot([2, 2, 6]) <= 300,
                sum(x) <= 100,
            ]),
        )

        model.get_linear_constraint_references(name='c')
        ```

        Args:
          name (str): Required. The name of the linear constraint set.

        Returns:
          pd.Series: The references of the linear constraints in the set, indexed by
          their corresponding dimensions.

        Raises:
          KeyError: If name is not found in the set of linear constraints.
        """
        return self._linear_constraints[name]

    def get_linear_constraint_upper_bounds(
        self, constraints: Optional[_IndexOrSeries] = None
    ) -> pd.Series:
        """Gets the upper bounds of all linear constraints in the set.

        If `constraints` is a `pd.Index`, then the output will be indexed by the
        constraints. If `constraints` is a `pd.Series` indexed by the underlying
        dimensions, then the output will be indexed by the same underlying
        dimensions.

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')
        x = model.create_variables(
            name='x',
            index=pd.Index(range(3)),
            lower_bound=0,
        )
        model.create_linear_constraints(
            name='c',
            bounded_exprs=pd.Series([
                x.dot([10, 4, 5]) <= 600,
                x.dot([2, 2, 6]) <= 300,
                sum(x) <= 100,
            ]),
        )

        model.get_linear_constraint_upper_bounds()
        ```

        Args:
          constraints (Union[pd.Index, pd.Series]): Optional. The set of linear
            constraints. If unspecified, all linear constraints will be in scope.

        Returns:
          pd.Series: The upper bounds of all linear constraints in the set.
        """
        return _attribute_series(
            func=lambda c: c._upper_bound,  # pylint: disable=protected-access
            values=self._get_linear_constraints(constraints),
        )

    def get_name(self) -> str:
        """Returns the name of the model."""
        return self._helper.name()

    def get_objective_expression(self) -> _LinearExpression:
        """Returns the objective expression of the model."""
        return _as_flat_linear_expression(
            sum(
                # pylint: disable=protected-access
                variable * self._helper.var_objective_coefficient(variable._index)
                for variable in self.get_variables()
            )
            + self._helper.objective_offset()
        )

    def get_objective_sense(self) -> ObjectiveSense:
        """Returns the objective sense of the model.

        If no objective has been set, it will return MINIMIZE.
        """
        if self._helper.maximize():
            return ObjectiveSense.MAXIMIZE
        return ObjectiveSense.MINIMIZE

    def get_schema(self) -> pd.DataFrame:
        """Returns the schema of the model."""
        result = {"type": [], "name": [], "dimensions": [], "count": []}
        for name, variables in self._variables.items():
            result["type"].append("variable")
            result["name"].append(name)
            result["dimensions"].append(variables.index.names)
            result["count"].append(len(variables))
        for name, constraints in self._linear_constraints.items():
            result["type"].append("linear_constraint")
            result["name"].append(name)
            result["dimensions"].append(constraints.index.names)
            result["count"].append(len(constraints))
        return pd.DataFrame(result)

    def get_variables(self, name: Optional[str] = None) -> pd.Index:
        """Gets all variables in the set.

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')
        x = model.create_variables(
            name='x',
            index=pd.Index(range(3)),
            lower_bound=0,
        )

        model.get_variables()
        ```

        Args:
          name (str): Optional. The name of the variable set. If unspecified, all
            variables will be in scope.

        Returns:
          pd.Index: The set of variables in the set.

        Raises:
          KeyError: if `name` is not found in the set of variables.
        """
        if name:
            return pd.Index(
                data=self.get_variable_references(name=name).values, name="variable"
            )
        return pd.concat(
            [
                pd.Series(
                    dtype=object,
                    index=pd.Index(variables.values, name="variable"),
                )
                for variables in self._variables.values()
            ]
        ).index

    def get_variable_lower_bounds(
        self, variables: Optional[_IndexOrSeries] = None
    ) -> pd.Series:
        """Gets the lower bounds of all variables in the set.

        If `variables` is a `pd.Index`, then the output will be indexed by the
        variables. If `variables` is a `pd.Series` indexed by the underlying
        dimensions, then the output will be indexed by the same underlying
        dimensions.

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')
        x = model.create_variables(
            name='x',
            index=pd.Index(range(3)),
            lower_bound=0,
        )

        model.get_variable_lower_bounds()
        ```

        Args:
          variables (Union[pd.Index, pd.Series]): Optional. The set of variables
            from which to get the lower bounds. If unspecified, all variables will
            be in scope.

        Returns:
          pd.Series: The lower bounds of all variables in the set.
        """
        return _attribute_series(
            func=lambda v: v._lower_bound,  # pylint: disable=protected-access
            values=self._get_variables(variables),
        )

    def get_variable_references(self, name: str) -> pd.Series:
        """Gets all variables in the set.

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')
        x = model.create_variables(
            name='x',
            index=pd.Index(range(3)),
            lower_bound=0,
        )

        model.get_variable_references(name='x')
        ```

        Args:
          name (str): Required. The name of the variable set.

        Returns:
          pd.Series: The variable set indexed by its underlying dimensions.

        Raises:
          KeyError: if `name` is not found in the set of variables.
        """
        if name not in self._variables:
            raise KeyError("There is no variable set named {}".format(name))
        return self._variables[name]

    def get_variable_upper_bounds(
        self, variables: Optional[_IndexOrSeries] = None
    ) -> pd.Series:
        """Gets the upper bounds of all variables in the set.

        If `variables` is a `pd.Index`, then the output will be indexed by the
        variables. If `variables` is a `pd.Series` indexed by the underlying
        dimensions, then the output will be indexed by the same underlying
        dimensions.

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')
        x = model.create_variables(
            name='x',
            index=pd.Index(range(3)),
            lower_bound=0,
        )

        model.get_variable_upper_bounds()
        ```

        Args:
          variables (Union[pd.Index, pd.Series]): Optional. The set of variables
            from which to get the upper bounds. If unspecified, all variables will
            be in scope.

        Returns:
          pd.Series: The upper bounds of all variables in the set.
        """
        return _attribute_series(
            func=lambda v: v._upper_bound,  # pylint: disable=protected-access
            values=self._get_variables(variables),
        )

    def minimize(self, expression: _LinearType) -> None:
        """Set the objective to minimize the given `expression`.

        This is equivalent to `.set_objective(expression, ObjectiveSense.MINIMIZE)`.

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')
        x = model.create_variables(name='x', index=pd.Index(range(3)))

        model.minimize(expression=x.dot([10, 6, 4]))
        ```

        To clear the objective of the model, simply set a new objective to
        minimize an expression of zero.

        Args:
          expression (LinearType): Required. The expression to minimize.
        """
        self.set_objective(expression, ObjectiveSense.MINIMIZE)

    def maximize(self, expression: _LinearType) -> None:
        """Set the objective to maximize the given `expression`.

        This is equivalent to `.set_objective(expression, ObjectiveSense.MAXIMIZE)`.

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')
        x = model.create_variables(name='x', index=pd.Index(range(3)))

        model.maximize(expression=x.dot([10, 6, 4]))
        ```

        To clear the objective of the model, simply set a new objective to
        minimize an expression of zero.

        Args:
          expression (LinearType): Required. The expression to maximize.
        """
        self.set_objective(expression, ObjectiveSense.MAXIMIZE)

    def set_objective(self, expression: _LinearType, sense: ObjectiveSense) -> None:
        """Sets the objective to maximize or minimize the given `expression`.

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')
        x = model.create_variables(name='x', index=pd.Index(range(3)))

        model.set_objective(
            expression=x.dot([10, 6, 4]),
            sense=pdm.ObjectiveSense.MAXIMIZE,
        )
        ```

        To clear the objective of the model, simply set a new objective to
        minimize an expression of zero.

        Args:
          expression (LinearType): Required. The expression to maximize or minimize.
          sense (ObjectiveSense): Required. Either `MAXIMIZE` or `MINIMIZE`.
        """
        self._helper.clear_objective()
        expr: _LinearExpression = _as_flat_linear_expression(expression)
        # pylint: disable=protected-access
        self._helper.set_objective_offset(expr._offset)
        for variable, coeff in expr._terms.items():
            self._helper.set_var_objective_coefficient(variable._index, coeff)
        # pylint: enable=protected-access
        self._helper.set_maximize(sense == ObjectiveSense.MAXIMIZE)


@dataclasses.dataclass(frozen=True)
class SolveOptions:
    """The options for solving the optimization model.

    Attributes:
      time_limit_seconds (int): Optional. The time limit (in seconds) for solving
        the optimization model. Defaults to `sys.maxsize`.
      enable_output (bool): Optional. Whether to enable solver output logging.
        Defaults to False.
      solver_specific_parameters (str): Optional. The format for specifying the
        individual parameters is solver-specific and currently undocumented.
        Defaults to an empty string.
    """

    time_limit_seconds: int = sys.maxsize
    enable_output: bool = False
    solver_specific_parameters: str = ""


@enum.unique
class SolveStatus(enum.Enum):
    """The status of solving the optimization problem.

    The solve status provides a guarantee for claims that can be made about
    the optimization problem. The number of solve statuses might grow with future
    versions of this package.

    Attributes:
      UNKNOWN: The status of solving the optimization problem is unknown. This is
        the default status.
      OPTIMAL: The solution is feasible and proven by the solver to be optimal for
        the problem's objective.
      FEASIBLE: The solution is feasible, but the solver was unable to prove that
        it is optimal. (I.e. the solution is feasible for all constraints up to
        the underlying solver's tolerances.)
      INFEASIBLE: The optimization problem is proven by the solver to be
        infeasible. Therefore no solutions can be found.
      UNBOUNDED: The optimization problem is feasible, but it has been proven by
        the solver to have arbitrarily good solutions (i.e. there are no optimal
        solutions). The solver might not provide any feasible solutions.
    """

    UNKNOWN = enum.auto()
    OPTIMAL = enum.auto()
    FEASIBLE = enum.auto()
    INFEASIBLE = enum.auto()
    UNBOUNDED = enum.auto()


_solve_status: dict[mbh.SolveStatus, SolveStatus] = {
    mbh.SolveStatus.OPTIMAL: SolveStatus.OPTIMAL,
    mbh.SolveStatus.FEASIBLE: SolveStatus.FEASIBLE,
    mbh.SolveStatus.INFEASIBLE: SolveStatus.INFEASIBLE,
    mbh.SolveStatus.UNBOUNDED: SolveStatus.UNBOUNDED,
}


class _SolveResult:
    """The result of solving an optimization model.

    It allows you to query the status of the solution process and inspect the
    solution found (if any). In general, the workflow looks like:

    ```
    # pylint: disable=line-too-long
    from ortools.linear_solver.python import pandas_model as pdm

    solver = pdm.Solver(solver_type=<SolverType>)
    result = solver.solve(model)

    if result.get_status() in (pdm.SolveStatus.OPTIMAL, pdm.SolveStatus.FEASIBLE):
      # result.get_objective_value() and result.get_variable_values() will return
      # non-NA values for a feasible (if not optimal) solution to the problem.
    elif result.get_status() == pdm.SolveStatus.INFEASIBLE:
      # result.get_objective_value() and result.get_variable_values() will return
      # NA values.
    else:
      # result.get_objective_value() and result.get_variable_values() are not
      # well-defined.
    ```

    (This class is marked internal because it has a constructor [and fields] that
    are considered internal. All its public methods will be stable in future
    versions from an API perspective.)
    """

    __slots__ = ("_model", "_solver", "_status")

    def __init__(
        self,
        model: OptimizationModel,
        solver: mbh.ModelSolverHelper,
        status: mbh.SolveStatus,
    ):
        self._model = model
        self._solver = solver
        self._status: SolveStatus = _solve_status.get(status, SolveStatus.UNKNOWN)

    def get_status(self) -> SolveStatus:
        return self._status

    def get_variable_values(
        self,
        variables: Optional[_IndexOrSeries] = None,
    ) -> pd.Series:
        """Gets the variable values of variables in the set.

        If `variables` is a `pd.Index`, then the output will be indexed by the
        variables. If `variables` is a `pd.Series` indexed by the underlying
        dimensions, then the output will be indexed by the same underlying
        dimensions.

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')
        x = model.create_variables(
            name='x',
            index=pd.Index(range(3)),
            lower_bound=0,
        )
        model.create_linear_constraints(
            name='c',
            bounded_exprs=pd.Series([
                x.dot([10, 4, 5]) <= 600,
                x.dot([2, 2, 6]) <= 300,
                sum(x) <= 100,
            ]),
        )
        model.set_objective(
            expression=x.dot([10, 6, 4]),
            sense=pdm.ObjectiveSense.MAXIMIZE,
        )
        solver = pdm.Solver(solver_type=pdm.SolverType.GLOP)
        run = solver.solve(model)

        run.get_variable_values()
        ```

        Args:
          variables (Union[pd.Index, pd.Series]): Optional. The set of variables
            from which to get the values. If unspecified, all variables will be in
            scope.

        Returns:
          pd.Series: The values of all variables in the set.
        """
        # pylint: disable=protected-access
        model_variables = self._model._get_variables(variables)
        if not self._solver.has_solution():
            return _attribute_series(func=lambda v: pd.NA, values=model_variables)
        return _attribute_series(
            func=lambda v: self._solver.var_value(v._index),
            values=model_variables,
        )

    def get_objective_value(self) -> float:
        """Gets the objective value of the best primal feasible solution.

        Returns:
          float: The objective value of the best feasible solution. It will return
          NA if there are no feasible solutions.
        """
        if not self._solver.has_solution():
            return pd.NA
        return self._solver.objective_value()


@enum.unique
class SolverType(enum.Enum):
    """The underlying solver to use.

    Attributes:
      CP_SAT: Google's CP-SAT solver (first party). Supports problems where all
        variables are `is_integer=True` and have finite upper and lower_bounds.
        Experimental support is available to automatically rescale and discretize
        problems with continuous variables.
      GLOP: Google's GLOP linear solver (first party). It supports LP with primal
        and dual simplex methods, but does not support problems with variables
        where `is_integer=True`.
      SCIP: Solving Constraint Integer Programs (SCIP) solver (third party). It
        supports linear (LP) and mixed-integer linear (MILP) problems.
    """

    CP_SAT = enum.auto()
    GLOP = enum.auto()
    SCIP = enum.auto()


_solver_type_to_name: dict[SolverType, str] = {
    SolverType.CP_SAT: "CP_SAT",
    SolverType.GLOP: "GLOP",
    SolverType.SCIP: "SCIP",
}


@dataclasses.dataclass(frozen=True)
class Solver:
    """A solver factory for solvers of the corresponding type.

    The purpose of this class is to search for a solution to the model provided
    to the .solve(...) method. It is immutable and does not support incremental
    solves. Each call to .solve(model, options) manages its own state.

    In general, the workflow looks like:

    ```
    # pylint: disable=line-too-long
    from ortools.linear_solver.python import pandas_model as pdm

    model = ...
    solver = pdm.Solver(solver_type=<SolverType>)
    result = solver.solve(model=model, options=pdm.SolveOptions(...))

    if result.get_status() in (pdm.SolveStatus.OPTIMAL, pdm.SolveStatus.FEASIBLE):
      # result.get_objective_value() and result.get_variable_values() will return
      # non-NA values for a feasible (if not optimal) solution to the problem.
    elif result.get_status() == pdm.SolveStatus.INFEASIBLE:
      # result.get_objective_value() and result.get_variable_values() will return
      # NA values.
    else:
      # result.get_objective_value() and result.get_variable_values() are not
      # well-defined.
    ```

    Attributes:
      solver_type (SolverType): The type of solver to use (e.g. GLOP, SCIP).
    """

    solver_type: SolverType

    def solve(
        self,
        model: OptimizationModel,
        options: SolveOptions = SolveOptions(),
    ) -> _SolveResult:
        """Solves an optimization model.

        It will overwrite the previous state of all variables and constraints with
        the results of the solve.

        Example usage:

        ```
        # pylint: disable=line-too-long
        from ortools.linear_solver.python import pandas_model as pdm
        import pandas as pd

        model = pdm.OptimizationModel(name='example')
        x = model.create_variables(
            name='x',
            index=pd.Index(range(3)),
            lower_bound=0,
        )
        model.create_linear_constraints(
            name='c',
            bounded_exprs=pd.Series([
                x.dot([10, 4, 5]) <= 600,
                x.dot([2, 2, 6]) <= 300,
                sum(x) <= 100,
            ]),
        )
        model.set_objective(
            expression=x.dot([10, 6, 4]),
            sense=pdm.ObjectiveSense.MAXIMIZE,
        )

        solver = pdm.Solver(solver_type=pdm.SolverType.GLOP)
        solver.solve(model=model)
        ```

        Args:
          model (OptimizationModel): Required. The model to be solved.
          options (SolveOptions): Optional. The options to set for solving the
            model.

        Returns:
          SolveResult: The result of solving the model.

        Raises:
          ValueError: If `options.solver_specific_parameters` is invalid for the
          Solver (based on its `solver_type`).
          RuntimeError: On a solve error.
        """
        solver = mbh.ModelSolverHelper(_solver_type_to_name[self.solver_type])
        solver.enable_output(options.enable_output)
        solver.set_time_limit_in_seconds(options.time_limit_seconds)
        if options.solver_specific_parameters:
            # This does not panic if the parameters are not recognized by the solver.
            solver.set_solver_specific_parameters(options.solver_specific_parameters)
        solver.solve(model._helper)  # pylint: disable=protected-access
        return _SolveResult(model, solver, solver.status())
