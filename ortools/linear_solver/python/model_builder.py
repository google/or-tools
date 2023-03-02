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
"""Methods for building and solving model_builder models.

The following two sections describe the main
methods for building and solving those models.

* [`ModelBuilder`](#model_builder.ModelBuilder): Methods for creating
models, including variables and constraints.
* [`ModelSolver`](#model_builder.ModelSolver): Methods for solving
a model and evaluating solutions.

Additional methods for solving ModelBuilder models:

* [`Constraint`](#model_builder.Constraint): A few utility methods for modifying
  constraints created by `ModelBuilder`.
* [`LinearExpr`](#model_builder.LinearExpr): Methods for creating constraints
  and the objective from large arrays of coefficients.

Other methods and functions listed are primarily used for developing OR-Tools,
rather than for solving specific optimization problems.
"""

import math
import numbers
from typing import Any, Callable, Dict, List, Literal, Optional, Union, Sequence, Tuple
import numpy as np
from numpy import typing as npt
from numpy.lib import mixins

from ortools.linear_solver.python import model_builder_helper as mbh
from ortools.linear_solver.python import pywrap_model_builder_helper as pwmb

# Custom types.
NumberT = Union[numbers.Number, np.number]
IntegerT = Union[numbers.Integral, np.integer]
LinearExprT = Union['LinearExpr', NumberT]
ConstraintT = Union['VarCompVar', 'BoundedLinearExpression', bool]
ShapeT = Union[IntegerT, Sequence[IntegerT]]
NumpyFuncT = Callable[[
    'VariableContainer',
    Optional[Union[NumberT, npt.NDArray[np.number], Sequence[NumberT]]],
], LinearExprT,]
SliceT = Union[slice, int, List[int], 'ellipsis',
               Tuple[Union[int, slice, List[int], 'ellipsis'], ...],]

# Forward solve statuses.
SolveStatus = pwmb.SolveStatus


class LinearExpr:
    """Holds an linear expression.

  A linear expression is built from constants and variables.
  For example, `x + 2.0 * (y - z + 1.0)`.

  Linear expressions are used in ModelBuilder models in constraints and in the
  objective:

  * You can define linear constraints as in:

  ```
  model.add(x + 2 * y <= 5.0)
  model.add(sum(array_of_vars) == 5.0)
  ```

  * In ModelBuilder, the objective is a linear expression:

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
    def sum(cls,
            expressions: Sequence[LinearExprT],
            *,
            constant: NumberT = 0.0) -> LinearExprT:
        """Creates `sum(expressions) + constant`.

    It can perform simple simplifications and returns different objects,
    including the input.

    Args:
      expressions: a sequence of linear expressions or constants.
      constant: a numerical constant.

    Returns:
      a LinearExpr instance or a numerical constant.
    """
        checked_constant: np.double = mbh.assert_is_a_number(constant)
        if not expressions:
            return checked_constant
        if len(expressions) == 1 and mbh.is_zero(checked_constant):
            return expressions[0]

        return LinearExpr.weighted_sum(expressions,
                                       np.ones(len(expressions)),
                                       constant=checked_constant)

    @classmethod
    def weighted_sum(
        cls,
        expressions: Sequence[LinearExprT],
        coefficients: Sequence[NumberT],
        *,
        constant: NumberT = 0.0,
    ) -> LinearExprT:
        """Creates `sum(expressions[i] * coefficients[i]) + constant`.

    It can perform simple simplifications and returns different object,
    including the input.

    Args:
      expressions: a sequence of linear expressions or constants.
      coefficients: a sequence of numerical constants.
      constant: a numerical constant.

    Returns:
      a LinearExpr instance or a numerical constant.
    """
        if len(expressions) != len(coefficients):
            raise ValueError(
                'LinearExpr.weighted_sum: expressions and coefficients have'
                ' different lengths')
        checked_constant: np.double = mbh.assert_is_a_number(constant)
        if not expressions:
            return checked_constant

        # Collect sub-arrays to concatenate.
        indices = []
        coeffs = []
        for e, c in zip(expressions, coefficients):
            if mbh.is_zero(c):
                continue

            if mbh.is_a_number(e):
                checked_constant += np.double(c * e)
            elif isinstance(e, Variable):
                indices.append(np.array([e.index], dtype=np.int32))
                coeffs.append(np.array([c], dtype=np.double))
            elif isinstance(e, _WeightedSum):
                checked_constant += np.double(c * e.constant)
                indices.append(e.variable_indices)
                coeffs.append(e.coefficients * c)

        if indices:
            return _WeightedSum(
                variable_indices=np.concatenate(indices, axis=0),
                coefficients=np.concatenate(coeffs, axis=0),
                constant=checked_constant,
            )
        return checked_constant

    @classmethod
    def term(
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
        checked_coefficient: np.double = mbh.assert_is_a_number(coefficient)
        checked_constant: np.double = mbh.assert_is_a_number(constant)

        if mbh.is_zero(checked_coefficient):
            return checked_constant
        if mbh.is_one(checked_coefficient) and mbh.is_zero(checked_constant):
            return expression
        if mbh.is_a_number(expression):
            return np.double(
                expression) * checked_coefficient + checked_constant
        if isinstance(expression, Variable):
            return _WeightedSum(
                variable_indices=np.array([expression.index], dtype=np.int32),
                coefficients=np.array([checked_coefficient], dtype=np.double),
                constant=checked_constant,
            )
        if isinstance(expression, _WeightedSum):
            return _WeightedSum(
                variable_indices=np.copy(expression.variable_indices),
                coefficients=expression.coefficients * checked_coefficient,
                constant=expression.constant * checked_coefficient +
                checked_constant,
            )
        raise TypeError(
            f'Unknown expression {expression!r} of type {type(expression)}')

    def __hash__(self):
        return object.__hash__(self)

    def __abs__(self):
        return NotImplemented

    def __add__(self, arg: LinearExprT) -> LinearExprT:
        if mbh.is_a_number(arg):
            return LinearExpr.sum([self], constant=arg)
        return LinearExpr.weighted_sum([self, arg], [1.0, 1.0], constant=0.0)

    def __radd__(self, arg: LinearExprT):
        return self.__add__(arg)

    def __sub__(self, arg: LinearExprT):
        if mbh.is_a_number(arg):
            return LinearExpr.sum([self], constant=arg * -1.0)
        return LinearExpr.weighted_sum([self, arg], [1.0, -1.0], constant=0.0)

    def __rsub__(self, arg: LinearExprT):
        return LinearExpr.weighted_sum([self, arg], [-1.0, 1.0], constant=0.0)

    def __mul__(self, arg: NumberT):
        arg = mbh.assert_is_a_number(arg)
        if mbh.is_one(arg):
            return self
        elif mbh.is_zero(arg):
            return 0.0
        return self.multiply_by(arg)

    def multiply_by(self, arg: NumberT) -> LinearExprT:
        raise NotImplementedError('LinearExpr.multiply_by')

    def __rmul__(self, arg: NumberT):
        return self.__mul__(arg)

    def __div__(self, arg: NumberT):
        coeff = mbh.assert_is_a_number(arg)
        if mbh.is_zero(coeff):
            raise ValueError(
                'Cannot call the division operator with a zero divisor')
        return self.__mul__(1.0 / coeff)

    def __truediv__(self, _):
        return NotImplemented

    def __mod__(self, _):
        return NotImplemented

    def __pow__(self, _):
        return NotImplemented

    def __lshift__(self, _):
        return NotImplemented

    def __rshift__(self, _):
        return NotImplemented

    def __and__(self, _):
        return NotImplemented

    def __or__(self, _):
        return NotImplemented

    def __xor__(self, _):
        return NotImplemented

    def __neg__(self):
        return self.__mul__(-1.0)

    def __bool__(self):
        raise NotImplementedError(
            f'Cannot use a LinearExpr {self} as a Boolean value')

    def __eq__(
            self, arg: Optional[LinearExprT]
    ) -> Union[bool, 'BoundedLinearExpression']:
        if arg is None:
            return False
        if mbh.is_a_number(arg):
            arg = mbh.assert_is_a_number(arg)
            return BoundedLinearExpression(self, arg, arg)
        else:
            return BoundedLinearExpression(self - arg, 0, 0)

    def __ge__(self, arg: LinearExprT) -> 'BoundedLinearExpression':
        if mbh.is_a_number(arg):
            arg = mbh.assert_is_a_number(arg)
            return BoundedLinearExpression(self, arg, math.inf)
        else:
            return BoundedLinearExpression(self - arg, 0, math.inf)

    def __le__(self, arg: LinearExprT) -> 'BoundedLinearExpression':
        if mbh.is_a_number(arg):
            arg = mbh.assert_is_a_number(arg)
            return BoundedLinearExpression(self, -math.inf, arg)
        else:
            return BoundedLinearExpression(self - arg, -math.inf, 0)

    def __ne__(self, arg: LinearExprT):
        return NotImplemented

    def __lt__(self, arg: LinearExprT):
        return NotImplemented

    def __gt__(self, arg: LinearExprT):
        return NotImplemented


class _WeightedSum(LinearExpr):
    """Represents sum(ai * xi) + b."""

    def __init__(
            self,
            *,
            variable_indices: npt.NDArray[np.int32],
            coefficients: npt.NDArray[np.double],
            constant: np.double = np.double(0.0),
    ):
        super().__init__()
        self.__variable_indices: npt.NDArray[np.int32] = variable_indices
        self.__coefficients: npt.NDArray[
            np.double] = mbh.assert_is_a_number_array(coefficients)
        self.__constant: np.double = constant

    def multiply_by(self, arg: NumberT) -> LinearExprT:
        if mbh.is_zero(arg):
            return 0.0
        if self.__variable_indices.size > 0:
            return _WeightedSum(
                variable_indices=np.copy(self.__variable_indices),
                coefficients=self.__coefficients * arg,
                constant=self.__constant * arg,
            )
        else:
            return self.constant * arg

    @property
    def variable_indices(self) -> npt.NDArray[np.int32]:
        return self.__variable_indices

    @property
    def coefficients(self) -> npt.NDArray[np.double]:
        return self.__coefficients

    @property
    def constant(self) -> np.double:
        return self.__constant

    def pretty_string(self, helper: pwmb.ModelBuilderHelper) -> str:
        """Pretty print a linear expression into a string."""
        output: str = ''
        for index, coeff in zip(self.variable_indices, self.coefficients):
            var_name = helper.var_name(index)
            if not var_name:
                var_name = f'unnamed_var_{index}'

            if not output and mbh.is_one(coeff):
                output = var_name
            elif not output and mbh.is_minus_one(coeff):
                output = f'-{var_name}'
            elif not output:
                output = f'{coeff} * {var_name}'
            elif mbh.is_one(coeff):
                output += f' + {var_name}'
            elif mbh.is_minus_one(coeff):
                output += f' - {var_name}'
            elif coeff > 0.0:
                output += f' + {coeff} * {var_name}'
            elif coeff < 0.0:
                output += ' - {-coeff} * {var_name}'
        if self.constant > 0:
            output += f' + {self.constant}'
        elif self.constant < 0:
            output += f' - {-self.constant}'
        if not output:
            output = '0.0'
        return output

    def __repr__(self):
        return (f'WeightedSum(indices = {self.variable_indices}, coefficients ='
                f' {self.coefficients}, constant = {self.constant})')


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
        helper: pwmb.ModelBuilderHelper,
        lb: NumberT,
        ub: Optional[NumberT],
        is_integral: Optional[bool],
        name: Optional[str],
    ):
        """See ModelBuilder.new_var below."""
        LinearExpr.__init__(self)
        self.__helper: pwmb.ModelBuilderHelper = helper
        # Python do not support multiple __init__ methods.
        # This method is only called from the ModelBuilder class.
        # We hack the parameter to support the two cases:
        # case 1:
        #     helper is a ModelBuilderHelper, lb is a double value, ub is a double
        #     value, is_integral is a Boolean value, and name is a string.
        # case 2:
        #     helper is a ModelBuilderHelper, lb is an index (int), ub is None,
        #     is_integral is None, and name is None.
        if mbh.is_integral(lb) and ub is None and is_integral is None:
            self.__index: np.int32 = np.int32(lb)
            self.__helper: pwmb.ModelBuilderHelper = helper
        else:
            index: np.int32 = helper.add_var()
            self.__index: np.int32 = np.int32(index)
            self.__helper: pwmb.ModelBuilderHelper = helper
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
    def helper(self) -> pwmb.ModelBuilderHelper:
        """Returns the underlying ModelBuilderHelper."""
        return self.__helper

    def is_equal_to(self, other: LinearExprT) -> bool:
        """Returns true if self == other in the python sense."""
        if not isinstance(other, Variable):
            return False
        return self.index == other.index and self.helper == other.helper

    def __str__(self) -> str:
        name = self.__helper.var_name(self.__index)
        if not name:
            if self.__helper.VarIsInteger(self.__index):
                return 'unnamed_int_var_%i' % self.__index
            else:
                return 'unnamed_num_var_%i' % self.__index
        return name

    def __repr__(self) -> str:
        index = self.__index
        name = self.__helper.var_name(index)
        lb = self.__helper.var_lower_bound(index)
        ub = self.__helper.var_upper_bound(index)
        is_integer = self.__helper.var_is_integral(index)
        if name:
            if is_integer:
                return f'{name}(index={index}, lb={lb}, ub={ub}, integer)'
            else:
                return f'{name}(index={index}, lb={lb}, ub={ub})'
        else:
            if is_integer:
                return f'unnamed_var(index={index}, lb={lb}, ub={ub}, integer)'
            else:
                return f'unnamed_var(index={index}, lb={lb}, ub={ub})'

    @property
    def name(self) -> str:
        """Returns the name of the variable."""
        return self.__helper.var_name(self.__index)

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
            return VarCompVar(self, arg, True)
        else:
            if mbh.is_a_number(arg):
                arg = mbh.assert_is_a_number(arg)
                return BoundedLinearExpression(self, arg, arg)
            else:
                return BoundedLinearExpression(self - arg, 0.0, 0.0)

    def __ne__(self, arg: LinearExprT) -> ConstraintT:
        if arg is None:
            return True
        if isinstance(arg, Variable):
            return VarCompVar(self, arg, False)
        return NotImplemented

    def __hash__(self):
        return hash((self.__helper, self.__index))

    def multiply_by(self, arg: NumberT) -> LinearExprT:
        return LinearExpr.weighted_sum([self], [arg], constant=0.0)


_REGISTERED_NUMPY_VARIABLE_FUNCS: Dict[Any, NumpyFuncT] = {}


class VariableContainer(mixins.NDArrayOperatorsMixin):
    """Variable container."""

    def __init__(self, helper: pwmb.ModelBuilderHelper,
                 indices: npt.NDArray[np.int32]):
        self.__helper: pwmb.ModelBuilderHelper = helper
        self.__variable_indices: npt.NDArray[np.int32] = indices

    @property
    def variable_indices(self) -> npt.NDArray[np.int32]:
        return self.__variable_indices

    def __getitem__(
        self,
        pos: SliceT,
    ) -> Union['VariableContainer', Variable]:
        # delegate the treatment of the 'pos' query to __variable_indices.
        index_or_slice: Union[np.int32, npt.NDArray[np.int32]] = (
            self.__variable_indices[pos])
        if np.isscalar(index_or_slice):
            return Variable(self.__helper, index_or_slice, None, None, None)
        else:
            return VariableContainer(self.__helper, index_or_slice)

    def index_at(
        self,
        pos: Union[slice, int, List[int], Tuple[Union[int, slice, List[int]],
                                                ...]],
    ) -> Union[np.int32, npt.NDArray[np.int32]]:
        """Returns the index of the variable at the position 'pos'."""
        return self.__variable_indices[pos]

    # pylint: disable=invalid-name
    @property
    def T(self) -> 'VariableContainer':
        """Returns a view upon the transposed numpy array of variables."""
        return VariableContainer(self.__helper, self.__variable_indices.T)

    # pylint: enable=invalid-name

    @property
    def shape(self) -> Sequence[int]:
        """Returns the shape of the numpy array."""
        return self.__variable_indices.shape

    @property
    def size(self) -> int:
        """Returns the number of variables in the numpy array."""
        return self.__variable_indices.size

    def flatten(self) -> 'VariableContainer':
        """returns the flattened array of variables."""
        return VariableContainer(self.__helper,
                                 self.__variable_indices.flatten())

    def __str__(self) -> str:
        return f'VariableContainer({self.__variable_indices})'

    def __repr__(self) -> str:
        return (
            f'VariableContainer({self.__helper}, {repr(self.__variable_indices)})'
        )

    def __len__(self):
        return self.__variable_indices.shape[0]

    def __array_ufunc__(
        self,
        ufunc: np.ufunc,
        method: Literal['__call__', 'reduce', 'reduceat', 'accumulate', 'outer',
                        'inner'],
        *inputs: Any,
        **kwargs: Any,
    ) -> LinearExprT:
        if method != '__call__':
            return NotImplemented
        function = _REGISTERED_NUMPY_VARIABLE_FUNCS.get(ufunc)
        if function is None:
            return NotImplemented
        if len(inputs) <= 2 and isinstance(inputs[0], VariableContainer):
            return function(*inputs, **kwargs)
        if len(inputs) == 2 and isinstance(inputs[1], VariableContainer):
            return function(inputs[1], inputs[0], **kwargs)
        return NotImplemented

    def __array_function__(self, func: Any, types: Any, inputs: Any,
                           kwargs: Any) -> LinearExprT:
        function = _REGISTERED_NUMPY_VARIABLE_FUNCS.get(func)
        if function is None:
            return NotImplemented
        if len(inputs) <= 2 and isinstance(inputs[0], VariableContainer):
            return function(*inputs, **kwargs)
        if len(inputs) == 2 and isinstance(inputs[1], VariableContainer):
            return function(inputs[1], inputs[0], **kwargs)
        return NotImplemented


def _implements(np_function: Any) -> Callable[[NumpyFuncT], NumpyFuncT]:
    """Register an __array_function__ implementation for VariableContainer objects."""

    def decorator(func: NumpyFuncT) -> NumpyFuncT:
        _REGISTERED_NUMPY_VARIABLE_FUNCS[np_function] = func
        return func

    return decorator


@_implements(np.sum)
def sum_variable_container(container: VariableContainer,
                           constant: NumberT = 0.0) -> LinearExprT:
    """Implementation of np.sum for VariableContainer objects."""
    indices: npt.NDArray[np.int32] = container.variable_indices
    return _WeightedSum(
        variable_indices=indices.flatten(),
        coefficients=np.ones(indices.size),
        constant=np.double(constant),
    )


@_implements(np.dot)
def dot_variable_container(
    container: VariableContainer,
    arg: Union[np.double, npt.NDArray[np.double]],
) -> LinearExprT:
    """Implementation of np.dot for VariableContainer objects."""
    if len(container.shape) != 1:
        raise TypeError(
            'dot_variable_container only supports 1D variable containers')
    indices: npt.NDArray[np.int32] = container.variable_indices
    if np.isscalar(arg):
        return _WeightedSum(
            variable_indices=indices.flatten(),
            coefficients=np.full(indices.size, arg),
            constant=0.0,
        )
    else:
        arg: npt.NDArray[np.double] = np.array(arg, dtype=np.double)
        assert container.shape == arg.shape, (container.shape, arg.shape)
        return _WeightedSum(
            variable_indices=indices.flatten(),
            coefficients=arg.flatten(),
            constant=0.0,
        )


class VarCompVar:
    """Represents var == /!= var."""

    def __init__(self, left: Variable, right: Variable, is_equality: bool):
        self.__left: Variable = left
        self.__right: Variable = right
        self.__is_equality: bool = is_equality

    def __str__(self) -> str:
        if self.__is_equality:
            return f'{self.__left} == {self.__right}'
        else:
            return f'{self.__left} != {self.__right}'

    def __repr__(self) -> str:
        return f'VarCompVar({self.__left}, {self.__right}, {self.__is_equality})'

    @property
    def left(self) -> Variable:
        return self.__left

    @property
    def right(self) -> Variable:
        return self.__right

    @property
    def is_equality(self) -> bool:
        return self.__is_equality

    def __bool__(self) -> bool:
        return bool(
            self.__left.index == self.__right.index) == self.__is_equality


# TODO(user): investigate storing left and right expressions.
class BoundedLinearExpression:
    """Represents a linear constraint: `lb <= linear expression <= ub`.

  The only use of this class is to be added to the ModelBuilder through
  `ModelBuilder.add(bounded expression)`, as in:

      model.Add(x + 2 * y -1 >= z)
  """

    def __init__(self, expr: LinearExprT, lb: NumberT, ub: NumberT):
        self.__expr: LinearExprT = expr
        self.__lb: np.double = mbh.assert_is_a_number(lb)
        self.__ub: np.double = mbh.assert_is_a_number(ub)

    def __str__(self) -> str:
        if self.__lb > -math.inf and self.__ub < math.inf:
            if self.__lb == self.__ub:
                return str(self.__expr) + ' == ' + str(self.__lb)
            else:
                return str(self.__lb) + ' <= ' + str(
                    self.__expr) + ' <= ' + str(self.__ub)
        elif self.__lb > -math.inf:
            return str(self.__expr) + ' >= ' + str(self.__lb)
        elif self.__ub < math.inf:
            return str(self.__expr) + ' <= ' + str(self.__ub)
        else:
            return 'True (unbounded expr ' + str(self.__expr) + ')'

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
            f'Cannot use a BoundedLinearExpression {self} as a Boolean value')


class LinearConstraint:
    """Stores a linear equation.

  Example:
      x = model.new_num_var(0, 10, 'x')
      y = model.new_num_var(0, 10, 'y')

      linear_constraint = model.add(x + 2 * y == 5)
  """

    def __init__(self, helper: pwmb.ModelBuilderHelper):
        self.__index: np.int32 = helper.add_linear_constraint()
        self.__helper: pwmb.ModelBuilderHelper = helper

    @property
    def index(self) -> np.int32:
        """Returns the index of the constraint in the helper."""
        return self.__index

    @property
    def helper(self) -> pwmb.ModelBuilderHelper:
        """Returns the ModelBuilderHelper instance."""
        return self.__helper

    @property
    def lower_bound(self) -> np.double:
        return self.__helper.constraint_lower_bound(self.__index)

    @lower_bound.setter
    def lower_bound(self, bound: NumberT) -> None:
        self.__helper.set_constraint_lower_bound(self.__index, bound)

    @property
    def upper_bound(self) -> np.double:
        return self.__helper.constraint_upper_bound(self.__index)

    @upper_bound.setter
    def upper_bound(self, bound: NumberT) -> None:
        self.__helper.set_constraint_upper_bound(self.__index, bound)

    @property
    def name(self) -> str:
        return self.__helper.constraint_name(self.__index)

    @name.setter
    def name(self, name: str) -> None:
        return self.__helper.set_constraint_name(self.__index, name)

    def add_term(self, var: Variable, coeff: NumberT) -> None:
        self.__helper.add_term_to_constraint(self.__index, var.index, coeff)


class ModelBuilder:
    """Methods for building a linear model.

  Methods beginning with:

  * ```new_``` create integer, boolean, or interval variables.
  * ```add_``` create new constraints and add them to the model.
  """

    def __init__(self):
        self.__helper: pwmb.ModelBuilderHelper = pwmb.ModelBuilderHelper()

    # Integer variable.

    def new_var(self, lb: NumberT, ub: NumberT, is_integer: bool,
                name: Optional[str]) -> Variable:
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

    def new_int_var(self,
                    lb: NumberT,
                    ub: NumberT,
                    name: Optional[str] = None) -> Variable:
        """Create an integer variable with domain [lb, ub].

    Args:
      lb: Lower bound of the variable.
      ub: Upper bound of the variable.
      name: The name of the variable.

    Returns:
      a variable whose domain is [lb, ub].
    """

        return self.new_var(lb, ub, True, name)

    def new_num_var(self,
                    lb: NumberT,
                    ub: NumberT,
                    name: Optional[str] = None) -> Variable:
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
        return self.new_var(0, 1, True, name)

    def new_constant(self, value: NumberT) -> Variable:
        """Declares a constant variable."""
        return self.new_var(value, value, False, None)

    def new_var_array(
        self,
        *,
        lower_bounds: npt.ArrayLike,
        upper_bounds: npt.ArrayLike,
        is_integral: npt.ArrayLike,
        shape: Optional[ShapeT] = None,
        name: Optional[str] = None,
    ) -> VariableContainer:
        """Creates a vector of variables from bounds, shape, is_integral."""
        # Convert the shape to a list of sizes if needed.
        if shape is not None and np.isscalar(shape):
            shape = [shape]

        if not np.isscalar(lower_bounds):
            if shape is None:
                shape = np.shape(lower_bounds)
            elif shape != np.shape(lower_bounds):
                raise ValueError(
                    'lower_bounds, upper_bounds, is_integral and shape must have'
                    ' compatible shapes (when defined)')

        if not np.isscalar(upper_bounds):
            if shape is None:
                shape = np.shape(upper_bounds)
            elif shape != np.shape(upper_bounds):
                raise ValueError(
                    'lower_bounds, upper_bounds, is_integral and shape must have'
                    ' compatible shapes (when defined)')

        if not np.isscalar(is_integral):
            if shape is None:
                shape = np.shape(is_integral)
            elif shape != np.shape(is_integral):
                raise ValueError(
                    'lower_bounds, upper_bounds, is_integral and shape must have'
                    ' compatible shapes (when defined)')

        if shape is None:
            raise ValueError('a shape must be defined')

        name = name or ''

        if (np.isscalar(lower_bounds) and np.isscalar(upper_bounds) and
                np.isscalar(is_integral)):
            var_indices = self.__helper.add_var_array(shape, lower_bounds,
                                                      upper_bounds, is_integral,
                                                      name)
            return VariableContainer(self.__helper, var_indices)

        # Convert scalars to np.arrays if needed.
        if np.isscalar(lower_bounds):
            lower_bounds = np.full(shape, lower_bounds)
        if np.isscalar(upper_bounds):
            upper_bounds = np.full(shape, upper_bounds)
        if np.isscalar(is_integral):
            is_integral = np.full(shape, is_integral)

        var_indices = self.__helper.add_var_array_with_bounds(
            lower_bounds, upper_bounds, is_integral, name)
        return VariableContainer(self.__helper, var_indices)

    def new_num_var_array(
        self,
        *,
        lower_bounds: npt.ArrayLike,
        upper_bounds: npt.ArrayLike,
        shape: Optional[ShapeT] = None,
        name: Optional[str] = None,
    ) -> VariableContainer:
        """Creates a vector of continuous variables from shape and bounds."""
        # Convert the shape to a list of sizes if needed.
        if shape is not None and np.isscalar(shape):
            shape = [shape]

        if not np.isscalar(lower_bounds):
            if shape is None:
                shape = np.shape(lower_bounds)
            elif shape != np.shape(lower_bounds):
                raise ValueError(
                    'lower_bounds, upper_bounds, and shape must have'
                    ' compatible shapes (when defined)')

        if not np.isscalar(upper_bounds):
            if shape is None:
                shape = np.shape(upper_bounds)
            elif shape != np.shape(upper_bounds):
                raise ValueError(
                    'lower_bounds, upper_bounds, and shape must have'
                    ' compatible shapes (when defined)')

        if shape is None:
            raise ValueError('a shape must be defined')

        name = name or ''

        if np.isscalar(lower_bounds) and np.isscalar(upper_bounds):
            var_indices = self.__helper.add_var_array(shape, lower_bounds,
                                                      upper_bounds, False, name)
            return VariableContainer(self.__helper, var_indices)

        # Convert scalars to np.arrays if needed.
        if np.isscalar(lower_bounds):
            lower_bounds = np.full(shape, lower_bounds)
        if np.isscalar(upper_bounds):
            upper_bounds = np.full(shape, upper_bounds)

        var_indices = self.__helper.add_var_array_with_bounds(
            lower_bounds, upper_bounds, np.zeros(shape, dtype=bool), name)
        return VariableContainer(self.__helper, var_indices)

    def new_int_var_array(
        self,
        *,
        lower_bounds: npt.ArrayLike,
        upper_bounds: npt.ArrayLike,
        shape: Optional[ShapeT] = None,
        name: Optional[str] = None,
    ) -> VariableContainer:
        """Creates a vector of integer variables from shape and bounds."""
        # Convert the shape to a list of sizes if needed.
        if shape is not None and np.isscalar(shape):
            shape = [shape]

        if not np.isscalar(lower_bounds):
            if shape is None:
                shape = np.shape(lower_bounds)
            elif shape != np.shape(lower_bounds):
                raise ValueError(
                    'lower_bounds, upper_bounds, and shape must have'
                    ' compatible shapes (when defined)')

        if not np.isscalar(upper_bounds):
            if shape is None:
                shape = np.shape(upper_bounds)
            elif shape != np.shape(upper_bounds):
                raise ValueError(
                    'lower_bounds, upper_bounds, and shape must have'
                    ' compatible shapes (when defined)')

        if shape is None:
            raise ValueError('a shape must be defined')

        name = name or ''

        if np.isscalar(lower_bounds) and np.isscalar(upper_bounds):
            var_indices = self.__helper.add_var_array(shape, lower_bounds,
                                                      upper_bounds, True, name)
            return VariableContainer(self.__helper, var_indices)

        # Convert scalars to np.arrays if needed.
        if np.isscalar(lower_bounds):
            lower_bounds = np.full(shape, lower_bounds)
        if np.isscalar(upper_bounds):
            upper_bounds = np.full(shape, upper_bounds)

        var_indices = self.__helper.add_var_array_with_bounds(
            lower_bounds, upper_bounds, np.ones(shape, dtype=bool), name)
        return VariableContainer(self.__helper, var_indices)

    def new_bool_var_array(
        self,
        shape: ShapeT,
        name: Optional[str] = None,
    ) -> VariableContainer:
        """Creates a vector of Boolean variables."""
        if mbh.is_integral(shape):
            shape = [shape]

        name = name or ''

        var_indices = self.__helper.add_var_array(shape, 0.0, 1.0, True, name)
        return VariableContainer(self.__helper, var_indices)

    def var_from_index(self, index: IntegerT) -> Variable:
        """Rebuilds a variable object from the model and its index."""
        return Variable(self.__helper, index, None, None, None)

    @property
    def num_variables(self) -> int:
        """Returns the number of variables in the model."""
        return self.__helper.num_variables()

    # Linear constraints.

    def add_linear_constraint(
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
        if mbh.is_a_number(linear_expr):
            self.__helper.set_constraint_lower_bound(ct.index, lb - linear_expr)
            self.__helper.set_constraint_upper_bound(ct.index, ub - linear_expr)
        elif isinstance(linear_expr, Variable):
            self.__helper.set_constraint_lower_bound(ct.index, lb)
            self.__helper.set_constraint_upper_bound(ct.index, ub)
            self.__helper.add_term_to_constraint(ct.index, linear_expr.index,
                                                 1.0)
        elif isinstance(linear_expr, _WeightedSum):
            self.__helper.set_constraint_lower_bound(ct.index,
                                                     lb - linear_expr.constant)
            self.__helper.set_constraint_upper_bound(ct.index,
                                                     ub - linear_expr.constant)
            self.__helper.add_terms_to_constraint(ct.index,
                                                  linear_expr.variable_indices,
                                                  linear_expr.coefficients)
        else:
            raise TypeError(
                f'Not supported: ModelBuilder.add_linear_constraint({linear_expr})'
                f' with type {type(linear_expr)}')
        return ct

    def add(self,
            ct: ConstraintT,
            name: Optional[str] = None) -> LinearConstraint:
        """Adds a `BoundedLinearExpression` to the model.

    Args:
      ct: A [`BoundedLinearExpression`](#boundedlinearexpression).
      name: An optional name.

    Returns:
      An instance of the `Constraint` class.
    """
        if isinstance(ct, BoundedLinearExpression):
            return self.add_linear_constraint(ct.expression, ct.lower_bound,
                                              ct.upper_bound, name)
        elif isinstance(ct, VarCompVar):
            if not ct.is_equality:
                raise TypeError('Not supported: ModelBuilder.Add(' + str(ct) +
                                ')')
            new_ct = LinearConstraint(self.__helper)
            new_ct.lower_bound = 0.0
            new_ct.upper_bound = 0.0
            new_ct.add_term(ct.left, 1.0)
            new_ct.add_term(ct.right, -1.0)
            return new_ct
        elif ct and isinstance(ct, bool):
            return self.add_linear_constraint(
                linear_expr=0.0)  # Evaluate to True.
        elif not ct and isinstance(ct, bool):
            return self.add_linear_constraint(1.0, 0.0,
                                              0.0)  # Evaluate to False.
        else:
            raise TypeError('Not supported: ModelBuilder.Add(' + str(ct) + ')')

    @property
    def num_constraints(self) -> int:
        return self.__helper.num_constraints()

    # Objective.
    def minimize(self, linear_expr: LinearExprT) -> None:
        self.__optimize(linear_expr, False)

    def maximize(self, linear_expr: LinearExprT) -> None:
        self.__optimize(linear_expr, True)

    def __optimize(self, linear_expr: LinearExprT, maximize: bool) -> None:
        """Defines the objective."""
        self.helper.clear_objective()
        self.__helper.set_maximize(maximize)
        if mbh.is_a_number(linear_expr):
            self.helper.set_objective_offset(linear_expr)
        elif isinstance(linear_expr, Variable):
            self.helper.set_var_objective_coefficient(linear_expr.index, 1.0)
        elif isinstance(linear_expr, _WeightedSum):
            self.helper.set_objective_offset(linear_expr.constant)
            self.__helper.set_objective_coefficients(
                linear_expr.variable_indices, linear_expr.coefficients)
        else:
            raise TypeError(
                f'Not supported: ModelBuilder.minimize/maximize({linear_expr})')

    @property
    def objective_offset(self) -> np.double:
        return self.__helper.objective_offset()

    @objective_offset.setter
    def objective_offset(self, value: NumberT) -> None:
        self.__helper.set_objective_offset(value)

    # Input/Output
    def export_to_lp_string(self, obfuscate: bool = False) -> str:
        options: pwmb.MPModelExportOptions = pwmb.MPModelExportOptions()
        options.obfuscate = obfuscate
        return self.__helper.export_to_lp_string(options)

    def export_to_mps_string(self, obfuscate: bool = False) -> str:
        options: pwmb.MPModelExportOptions = pwmb.MPModelExportOptions()
        options.obfuscate = obfuscate
        return self.__helper.export_to_mps_string(options)

    def import_from_mps_string(self, mps_string: str) -> bool:
        return self.__helper.import_from_mps_string(mps_string)

    def import_from_mps_file(self, mps_file: str) -> bool:
        return self.__helper.import_from_mps_file(mps_file)

    def import_from_lp_string(self, lp_string: str) -> bool:
        return self.__helper.import_from_lp_string(lp_string)

    def import_from_lp_file(self, lp_file: str) -> bool:
        return self.__helper.import_from_lp_file(lp_file)

    # Utilities
    @property
    def name(self) -> str:
        return self.__helper.name()

    @name.setter
    def name(self, name: str):
        self.__helper.set_name(name)

    @property
    def helper(self) -> pwmb.ModelBuilderHelper:
        """Returns the model builder helper."""
        return self.__helper


class ModelSolver:
    """Main solver class.

  The purpose of this class is to search for a solution to the model provided
  to the solve() method.

  Once solve() is called, this class allows inspecting the solution found
  with the value() method, as well as general statistics about the solve
  procedure.
  """

    def __init__(self, solver_name: str):
        self.__solve_helper: pwmb.ModelSolverHelper = pwmb.ModelSolverHelper(
            solver_name)
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

    def solve(self, model: ModelBuilder) -> SolveStatus:
        """Solves a problem and passes each solution to the callback if not null."""
        if self.log_callback is not None:
            self.__solve_helper.set_log_callback(self.log_callback)
        else:
            self.__solve_helper.clear_log_callback()
        self.__solve_helper.solve(model.helper)
        return SolveStatus(self.__solve_helper.status())

    def __check_has_feasible_solution(self) -> None:
        """Checks that solve has run and has found a feasible solution."""
        if not self.__solve_helper.has_solution():
            raise RuntimeError(
                'solve() has not be called, or no solution has been found.')

    def stop_search(self):
        """Stops the current search asynchronously."""
        self.__solve_helper.interrupt_solve()

    def value(self, expr: LinearExprT) -> np.double:
        """Returns the value of a linear expression after solve."""
        self.__check_has_feasible_solution()
        if mbh.is_a_number(expr):
            return expr
        elif isinstance(expr, Variable):
            return self.__solve_helper.var_value(expr.index)
        elif isinstance(expr, _WeightedSum):
            return self.__solve_helper.expression_value(expr.variable_indices,
                                                        expr.coefficients,
                                                        expr.constant)
        else:
            raise TypeError(f'Unknown expression {expr!r} of type {type(expr)}')

    def reduced_cost(self, var: Variable) -> np.double:
        """Returns the reduced cost of a linear expression after solve."""
        self.__check_has_feasible_solution()
        return self.__solve_helper.reduced_cost(var.index)

    def dual_value(self, ct: LinearConstraint) -> np.double:
        """Returns the dual value of a linear constraint after solve."""
        self.__check_has_feasible_solution()
        return self.__solve_helper.dual_value(ct.index)

    @property
    def objective_value(self) -> np.double:
        """Returns the value of the objective after solve."""
        self.__check_has_feasible_solution()
        return self.__solve_helper.objective_value()

    @property
    def best_objective_bound(self) -> np.double:
        """Returns the best lower (upper) bound found when min(max)imizing."""
        self.__check_has_feasible_solution()
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
