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

"""A solver independent library for modeling optimization problems.

Example use to model the optimization problem:
   max 2.0 * x + y
   s.t. x + y <= 1.5
            x in {0.0, 1.0}
            y in [0.0, 2.5]

  model = mathopt.Model(name='my_model')
  x = model.add_binary_variable(name='x')
  y = model.add_variable(lb=0.0, ub=2.5, name='y')
  # We can directly use linear combinations of variables ...
  model.add_linear_constraint(x + y <= 1.5, name='c')
  # ... or build them incrementally.
  objective_expression = 0
  objective_expression += 2 * x
  objective_expression += y
  model.maximize(objective_expression)

  # May raise a RuntimeError on invalid input or internal solver errors.
  result = mathopt.solve(model, mathopt.SolverType.GSCIP)

  if result.termination.reason not in (mathopt.TerminationReason.OPTIMAL,
                                       mathopt.TerminationReason.FEASIBLE):
    raise RuntimeError(f'model failed to solve: {result.termination}')

  print(f'Objective value: {result.objective_value()}')
  print(f'Value for variable x: {result.variable_values()[x]}')
"""

import abc
import collections
import dataclasses
import math
import typing
from typing import (
    Any,
    DefaultDict,
    Deque,
    Generic,
    Iterable,
    Iterator,
    Mapping,
    NamedTuple,
    NoReturn,
    Optional,
    Protocol,
    Tuple,
    Type,
    TypeVar,
    Union,
)
import weakref

import immutabledict

from ortools.math_opt import model_pb2
from ortools.math_opt import model_update_pb2
from ortools.math_opt.python import hash_model_storage
from ortools.math_opt.python import model_storage

Storage = model_storage.ModelStorage
StorageClass = model_storage.ModelStorageImplClass

LinearTypes = Union[int, float, "LinearBase"]
QuadraticTypes = Union[int, float, "LinearBase", "QuadraticBase"]
LinearTypesExceptVariable = Union[
    float, int, "LinearTerm", "LinearExpression", "LinearSum", "LinearProduct"
]

_CHAINED_COMPARISON_MESSAGE = (
    "If you were trying to create a two-sided or "
    "ranged linear inequality of the form `lb <= "
    "expr <= ub`, try `(lb <= expr) <= ub` instead"
)
_EXPRESSION_COMP_EXPRESSION_MESSAGE = (
    "This error can occur when adding "
    "inequalities of the form `(a <= b) <= "
    "c` where (a, b, c) includes two or more"
    " non-constant linear expressions"
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


def _raise_ne_not_supported() -> NoReturn:
    raise TypeError("!= constraints are not supported")


class UpperBoundedLinearExpression:
    """An inequality of the form expression <= upper_bound.

    Where:
     * expression is a linear expression, and
     * upper_bound is a float
    """

    __slots__ = "_expression", "_upper_bound"

    def __init__(self, expression: "LinearBase", upper_bound: float) -> None:
        """Operator overloading can be used instead: e.g. `x + y <= 2.0`."""
        self._expression: "LinearBase" = expression
        self._upper_bound: float = upper_bound

    @property
    def expression(self) -> "LinearBase":
        return self._expression

    @property
    def upper_bound(self) -> float:
        return self._upper_bound

    def __ge__(self, lhs: float) -> "BoundedLinearExpression":
        if isinstance(lhs, (int, float)):
            return BoundedLinearExpression(lhs, self.expression, self.upper_bound)
        _raise_binary_operator_type_error(">=", type(self), type(lhs))

    def __bool__(self) -> bool:
        raise TypeError(
            "__bool__ is unsupported for UpperBoundedLinearExpression"
            + "\n"
            + _CHAINED_COMPARISON_MESSAGE
        )

    def __str__(self):
        return f"{self._expression!s} <= {self._upper_bound}"

    def __repr__(self):
        return f"{self._expression!r} <= {self._upper_bound}"


class LowerBoundedLinearExpression:
    """An inequality of the form expression >= lower_bound.

    Where:
     * expression is a linear expression, and
     * lower_bound is a float
    """

    __slots__ = "_expression", "_lower_bound"

    def __init__(self, expression: "LinearBase", lower_bound: float) -> None:
        """Operator overloading can be used instead: e.g. `x + y >= 2.0`."""
        self._expression: "LinearBase" = expression
        self._lower_bound: float = lower_bound

    @property
    def expression(self) -> "LinearBase":
        return self._expression

    @property
    def lower_bound(self) -> float:
        return self._lower_bound

    def __le__(self, rhs: float) -> "BoundedLinearExpression":
        if isinstance(rhs, (int, float)):
            return BoundedLinearExpression(self.lower_bound, self.expression, rhs)
        _raise_binary_operator_type_error("<=", type(self), type(rhs))

    def __bool__(self) -> bool:
        raise TypeError(
            "__bool__ is unsupported for LowerBoundedLinearExpression"
            + "\n"
            + _CHAINED_COMPARISON_MESSAGE
        )

    def __str__(self):
        return f"{self._expression!s} >= {self._lower_bound}"

    def __repr__(self):
        return f"{self._expression!r} >= {self._lower_bound}"


class BoundedLinearExpression:
    """An inequality of the form lower_bound <= expression <= upper_bound.

    Where:
     * expression is a linear expression
     * lower_bound is a float, and
     * upper_bound is a float

    Note: Because of limitations related to Python's handling of chained
    comparisons, bounded expressions cannot be directly created usign
    overloaded comparisons as in `lower_bound <= expression <= upper_bound`.
    One solution is to wrap one of the inequalities in parenthesis as in
    `(lower_bound <= expression) <= upper_bound`.
    """

    __slots__ = "_expression", "_lower_bound", "_upper_bound"

    def __init__(
        self, lower_bound: float, expression: "LinearBase", upper_bound: float
    ) -> None:
        self._expression: "LinearBase" = expression
        self._lower_bound: float = lower_bound
        self._upper_bound: float = upper_bound

    @property
    def expression(self) -> "LinearBase":
        return self._expression

    @property
    def lower_bound(self) -> float:
        return self._lower_bound

    @property
    def upper_bound(self) -> float:
        return self._upper_bound

    def __bool__(self) -> bool:
        raise TypeError(
            "__bool__ is unsupported for BoundedLinearExpression"
            + "\n"
            + _CHAINED_COMPARISON_MESSAGE
        )

    def __str__(self):
        return f"{self._lower_bound} <= {self._expression!s} <= {self._upper_bound}"

    def __repr__(self):
        return f"{self._lower_bound} <= {self._expression!r} <= {self._upper_bound}"


class VarEqVar:
    """The result of the equality comparison between two Variable.

    We use an object here to delay the evaluation of equality so that we can use
    the operator== in two use-cases:

      1. when the user want to test that two Variable values references the same
         variable. This is supported by having this object support implicit
         conversion to bool.

      2. when the user want to use the equality to create a constraint of equality
         between two variables.
    """

    __slots__ = "_first_variable", "_second_variable"

    def __init__(
        self,
        first_variable: "Variable",
        second_variable: "Variable",
    ) -> None:
        self._first_variable: "Variable" = first_variable
        self._second_variable: "Variable" = second_variable

    @property
    def first_variable(self) -> "Variable":
        return self._first_variable

    @property
    def second_variable(self) -> "Variable":
        return self._second_variable

    def __bool__(self) -> bool:
        return (
            self._first_variable.model is self._second_variable.model
            and self._first_variable.id == self._second_variable.id
        )

    def __str__(self):
        return f"{self.first_variable!s} == {self._second_variable!s}"

    def __repr__(self):
        return f"{self.first_variable!r} == {self._second_variable!r}"


BoundedLinearTypesList = (
    LowerBoundedLinearExpression,
    UpperBoundedLinearExpression,
    BoundedLinearExpression,
    VarEqVar,
)
BoundedLinearTypes = Union[BoundedLinearTypesList]


# TODO(b/231426528): consider using a frozen dataclass.
class QuadraticTermKey:
    """An id-ordered pair of variables used as a key for quadratic terms."""

    __slots__ = "_first_var", "_second_var"

    def __init__(self, a: "Variable", b: "Variable"):
        """Variables a and b will be ordered internally by their ids."""
        self._first_var: "Variable" = a
        self._second_var: "Variable" = b
        if self._first_var.id > self._second_var.id:
            self._first_var, self._second_var = self._second_var, self._first_var

    @property
    def first_var(self) -> "Variable":
        return self._first_var

    @property
    def second_var(self) -> "Variable":
        return self._second_var

    def __eq__(self, other: "QuadraticTermKey") -> bool:
        return bool(
            self._first_var == other._first_var
            and self._second_var == other._second_var
        )

    def __hash__(self) -> int:
        return hash((self._first_var, self._second_var))

    def __str__(self):
        return f"{self._first_var!s} * {self._second_var!s}"

    def __repr__(self):
        return f"QuadraticTermKey({self._first_var!r}, {self._second_var!r})"


@dataclasses.dataclass
class _ProcessedElements:
    """Auxiliary data class for LinearBase._flatten_once_and_add_to()."""

    terms: DefaultDict["Variable", float] = dataclasses.field(
        default_factory=lambda: collections.defaultdict(float)
    )
    offset: float = 0.0


@dataclasses.dataclass
class _QuadraticProcessedElements(_ProcessedElements):
    """Auxiliary data class for QuadraticBase._quadratic_flatten_once_and_add_to()."""

    quadratic_terms: DefaultDict["QuadraticTermKey", float] = dataclasses.field(
        default_factory=lambda: collections.defaultdict(float)
    )


class _ToProcessElements(Protocol):
    """Auxiliary to-process stack interface for LinearBase._flatten_once_and_add_to() and QuadraticBase._quadratic_flatten_once_and_add_to()."""

    __slots__ = ()

    def append(self, term: "LinearBase", scale: float) -> None:
        """Add a linear object and scale to the to-process stack."""


_T = TypeVar("_T", "LinearBase", Union["LinearBase", "QuadraticBase"])


class _ToProcessElementsImplementation(Generic[_T]):
    """Auxiliary data class for LinearBase._flatten_once_and_add_to()."""

    __slots__ = ("_queue",)

    def __init__(self, term: _T, scale: float) -> None:
        self._queue: Deque[Tuple[_T, float]] = collections.deque([(term, scale)])

    def append(self, term: _T, scale: float) -> None:
        self._queue.append((term, scale))

    def pop(self) -> Tuple[_T, float]:
        return self._queue.popleft()

    def __bool__(self) -> bool:
        return bool(self._queue)


_LinearToProcessElements = _ToProcessElementsImplementation["LinearBase"]
_QuadraticToProcessElements = _ToProcessElementsImplementation[
    Union["LinearBase", "QuadraticBase"]
]


class LinearBase(metaclass=abc.ABCMeta):
    """Interface for types that can build linear expressions with +, -, * and /.

    Classes derived from LinearBase (plus float and int scalars) are used to
    build expression trees describing a linear expression. Operations nodes of the
    expression tree include:

      * LinearSum: describes a deferred sum of LinearTypes objects.
      * LinearProduct: describes a deferred product of a scalar and a
        LinearTypes object.

    Leaf nodes of the expression tree include:

      * float and int scalars.
      * Variable: a single variable.
      * LinearTerm: the product of a scalar and a Variable object.
      * LinearExpression: the sum of a scalar and LinearTerm objects.

    LinearBase objects/expression-trees can be used directly to create
    constraints or objective functions. However, to facilitate their inspection,
    any LinearTypes object can be flattened to a LinearExpression
    through:

     as_flat_linear_expression(value: LinearTypes) -> LinearExpression:

    In addition, all LinearBase objects are immutable.

    Performance notes:

    Using an expression tree representation instead of an eager construction of
    LinearExpression objects reduces known inefficiencies associated with the
    use of operator overloading to construct linear expressions. In particular, we
    expect the runtime of as_flat_linear_expression() to be linear in the size of
    the expression tree. Additional performance can gained by using LinearSum(c)
    instead of sum(c) for a container c, as the latter creates len(c) LinearSum
    objects.
    """

    __slots__ = ()

    # TODO(b/216492143): explore requirements for this function so calculation of
    # coefficients and offsets follow expected associativity rules (so approximate
    # float calculations are as expected).
    # TODO(b/216492143): add more details of what subclasses need to do in
    # developers guide.
    @abc.abstractmethod
    def _flatten_once_and_add_to(
        self,
        scale: float,
        processed_elements: _ProcessedElements,
        target_stack: _ToProcessElements,
    ) -> None:
        """Flatten one level of tree if needed and add to targets.

        Classes derived from LinearBase only need to implement this function
        to enable transformation to LinearExpression through
        as_flat_linear_expression().

        Args:
          scale: multiply elements by this number when processing or adding to
            stack.
          processed_elements: where to add LinearTerms and scalars that can be
            processed immediately.
          target_stack: where to add LinearBase elements that are not scalars or
            LinearTerms (i.e. elements that need further flattening).
            Implementations should append() to this stack to avoid being recursive.
        """

    def __eq__(
        self, rhs: LinearTypes
    ) -> (
        "BoundedLinearExpression"
    ):  # pytype: disable=signature-mismatch  # overriding-return-type-checks
        if isinstance(rhs, (int, float)):
            return BoundedLinearExpression(rhs, self, rhs)
        if not isinstance(rhs, LinearBase):
            _raise_binary_operator_type_error("==", type(self), type(rhs))
        return BoundedLinearExpression(0.0, self - rhs, 0.0)

    def __ne__(
        self, rhs: LinearTypes
    ) -> (
        NoReturn
    ):  # pytype: disable=signature-mismatch  # overriding-return-type-checks
        _raise_ne_not_supported()

    @typing.overload
    def __le__(self, rhs: float) -> "UpperBoundedLinearExpression": ...

    @typing.overload
    def __le__(self, rhs: "LinearBase") -> "BoundedLinearExpression": ...

    @typing.overload
    def __le__(self, rhs: "BoundedLinearExpression") -> NoReturn: ...

    def __le__(self, rhs):
        if isinstance(rhs, (int, float)):
            return UpperBoundedLinearExpression(self, rhs)
        if isinstance(rhs, LinearBase):
            return BoundedLinearExpression(-math.inf, self - rhs, 0.0)
        if isinstance(rhs, BoundedLinearExpression):
            _raise_binary_operator_type_error(
                "<=", type(self), type(rhs), _EXPRESSION_COMP_EXPRESSION_MESSAGE
            )
        _raise_binary_operator_type_error("<=", type(self), type(rhs))

    @typing.overload
    def __ge__(self, lhs: float) -> "LowerBoundedLinearExpression": ...

    @typing.overload
    def __ge__(self, lhs: "LinearBase") -> "BoundedLinearExpression": ...

    @typing.overload
    def __ge__(self, lhs: "BoundedLinearExpression") -> NoReturn: ...

    def __ge__(self, lhs):
        if isinstance(lhs, (int, float)):
            return LowerBoundedLinearExpression(self, lhs)
        if isinstance(lhs, LinearBase):
            return BoundedLinearExpression(0.0, self - lhs, math.inf)
        if isinstance(lhs, BoundedLinearExpression):
            _raise_binary_operator_type_error(
                ">=", type(self), type(lhs), _EXPRESSION_COMP_EXPRESSION_MESSAGE
            )
        _raise_binary_operator_type_error(">=", type(self), type(lhs))

    def __add__(self, expr: LinearTypes) -> "LinearSum":
        if not isinstance(expr, (int, float, LinearBase)):
            return NotImplemented
        return LinearSum((self, expr))

    def __radd__(self, expr: LinearTypes) -> "LinearSum":
        if not isinstance(expr, (int, float, LinearBase)):
            return NotImplemented
        return LinearSum((expr, self))

    def __sub__(self, expr: LinearTypes) -> "LinearSum":
        if not isinstance(expr, (int, float, LinearBase)):
            return NotImplemented
        return LinearSum((self, -expr))

    def __rsub__(self, expr: LinearTypes) -> "LinearSum":
        if not isinstance(expr, (int, float, LinearBase, QuadraticBase)):
            return NotImplemented
        return LinearSum((expr, -self))

    @typing.overload
    def __mul__(self, other: float) -> "LinearProduct": ...

    @typing.overload
    def __mul__(self, other: "LinearBase") -> "LinearLinearProduct": ...

    def __mul__(self, other):
        if not isinstance(other, (int, float, LinearBase)):
            return NotImplemented
        if isinstance(other, LinearBase):
            return LinearLinearProduct(self, other)
        return LinearProduct(other, self)

    def __rmul__(self, constant: float) -> "LinearProduct":
        if not isinstance(constant, (int, float)):
            return NotImplemented
        return LinearProduct(constant, self)

    # TODO(b/216492143): explore numerical consequences of 1.0 / constant below.
    def __truediv__(self, constant: float) -> "LinearProduct":
        if not isinstance(constant, (int, float)):
            return NotImplemented
        return LinearProduct(1.0 / constant, self)

    def __neg__(self) -> "LinearProduct":
        return LinearProduct(-1.0, self)


class QuadraticBase(metaclass=abc.ABCMeta):
    """Interface for types that can build quadratic expressions with +, -, * and /.

    Classes derived from QuadraticBase and LinearBase (plus float and int scalars)
    are used to build expression trees describing a quadratic expression.
    Operations nodes of the expression tree include:

        * QuadraticSum: describes a deferred sum of QuadraticTypes objects.
        * QuadraticProduct: describes a deferred product of a scalar and a
          QuadraticTypes object.
        * LinearLinearProduct: describes a deferred product of two LinearTypes
          objects.

      Leaf nodes of the expression tree include:

        * float and int scalars.
        * Variable: a single variable.
        * LinearTerm: the product of a scalar and a Variable object.
        * LinearExpression: the sum of a scalar and LinearTerm objects.
        * QuadraticTerm: the product of a scalar and two Variable objects.
        * QuadraticExpression: the sum of a scalar, LinearTerm objects and
          QuadraticTerm objects.

    QuadraticBase objects/expression-trees can be used directly to create
    objective functions. However, to facilitate their inspection, any
    QuadraticTypes object can be flattened to a QuadraticExpression
    through:

     as_flat_quadratic_expression(value: QuadraticTypes) -> QuadraticExpression:

    In addition, all QuadraticBase objects are immutable.

    Performance notes:

    Using an expression tree representation instead of an eager construction of
    QuadraticExpression objects reduces known inefficiencies associated with the
    use of operator overloading to construct quadratic expressions. In particular,
    we expect the runtime of as_flat_quadratic_expression() to be linear in the
    size of the expression tree. Additional performance can gained by using
    QuadraticSum(c) instead of sum(c) for a container c, as the latter creates
    len(c) QuadraticSum objects.
    """

    __slots__ = ()

    # TODO(b/216492143): explore requirements for this function so calculation of
    # coefficients and offsets follow expected associativity rules (so approximate
    # float calculations are as expected).
    # TODO(b/216492143): add more details of what subclasses need to do in
    # developers guide.
    @abc.abstractmethod
    def _quadratic_flatten_once_and_add_to(
        self,
        scale: float,
        processed_elements: _QuadraticProcessedElements,
        target_stack: _QuadraticToProcessElements,
    ) -> None:
        """Flatten one level of tree if needed and add to targets.

        Classes derived from QuadraticBase only need to implement this function
        to enable transformation to QuadraticExpression through
        as_flat_quadratic_expression().

        Args:
          scale: multiply elements by this number when processing or adding to
            stack.
          processed_elements: where to add linear terms, quadratic terms and scalars
            that can be processed immediately.
          target_stack: where to add LinearBase and QuadraticBase elements that are
            not scalars or linear terms or quadratic terms (i.e. elements that need
            further flattening). Implementations should append() to this stack to
            avoid being recursive.
        """

    def __add__(self, expr: QuadraticTypes) -> "QuadraticSum":
        if not isinstance(expr, (int, float, LinearBase, QuadraticBase)):
            return NotImplemented
        return QuadraticSum((self, expr))

    def __radd__(self, expr: QuadraticTypes) -> "QuadraticSum":
        if not isinstance(expr, (int, float, LinearBase, QuadraticBase)):
            return NotImplemented
        return QuadraticSum((expr, self))

    def __sub__(self, expr: QuadraticTypes) -> "QuadraticSum":
        if not isinstance(expr, (int, float, LinearBase, QuadraticBase)):
            return NotImplemented
        return QuadraticSum((self, -expr))

    def __rsub__(self, expr: QuadraticTypes) -> "QuadraticSum":
        if not isinstance(expr, (int, float, LinearBase, QuadraticBase)):
            return NotImplemented
        return QuadraticSum((expr, -self))

    def __mul__(self, other: float) -> "QuadraticProduct":
        if not isinstance(other, (int, float)):
            return NotImplemented
        return QuadraticProduct(other, self)

    def __rmul__(self, other: float) -> "QuadraticProduct":
        if not isinstance(other, (int, float)):
            return NotImplemented
        return QuadraticProduct(other, self)

    # TODO(b/216492143): explore numerical consequences of 1.0 / constant below.
    def __truediv__(self, constant: float) -> "QuadraticProduct":
        if not isinstance(constant, (int, float)):
            return NotImplemented
        return QuadraticProduct(1.0 / constant, self)

    def __neg__(self) -> "QuadraticProduct":
        return QuadraticProduct(-1.0, self)


class Variable(LinearBase):
    """A decision variable for an optimization model.

    A decision variable takes a value from a domain, either the real numbers or
    the integers, and restricted to be in some interval [lb, ub] (where lb and ub
    can be infinite). The case of lb == ub is allowed, this means the variable
    must take a single value. The case of lb > ub is also allowed, this implies
    that the problem is infeasible.

    A Variable is configured as follows:
      * lower_bound: a float property, lb above. Should not be NaN nor +inf.
      * upper_bound: a float property, ub above. Should not be NaN nor -inf.
      * integer: a bool property, if the domain is integer or continuous.

    The name is optional, read only, and used only for debugging. Non-empty names
    should be distinct.

    Every Variable is associated with a Model (defined below). Note that data
    describing the variable (e.g. lower_bound) is owned by Model.storage, this
    class is simply a reference to that data. Do not create a Variable directly,
    use Model.add_variable() instead.
    """

    __slots__ = "__weakref__", "_model", "_id"

    def __init__(self, model: "Model", vid: int) -> None:
        """Do not invoke directly, use Model.add_variable()."""
        self._model: "Model" = model
        self._id: int = vid

    @property
    def lower_bound(self) -> float:
        return self.model.storage.get_variable_lb(self._id)

    @lower_bound.setter
    def lower_bound(self, value: float) -> None:
        self.model.storage.set_variable_lb(self._id, value)

    @property
    def upper_bound(self) -> float:
        return self.model.storage.get_variable_ub(self._id)

    @upper_bound.setter
    def upper_bound(self, value: float) -> None:
        self.model.storage.set_variable_ub(self._id, value)

    @property
    def integer(self) -> bool:
        return self.model.storage.get_variable_is_integer(self._id)

    @integer.setter
    def integer(self, value: bool) -> None:
        self.model.storage.set_variable_is_integer(self._id, value)

    @property
    def name(self) -> str:
        return self.model.storage.get_variable_name(self._id)

    @property
    def id(self) -> int:
        return self._id

    @property
    def model(self) -> "Model":
        return self._model

    def __str__(self):
        """Returns the name, or a string containing the id if the name is empty."""
        return self.name if self.name else f"variable_{self.id}"

    def __repr__(self):
        return f"<Variable id: {self.id}, name: {self.name!r}>"

    @typing.overload
    def __eq__(self, rhs: "Variable") -> "VarEqVar": ...

    @typing.overload
    def __eq__(self, rhs: LinearTypesExceptVariable) -> "BoundedLinearExpression": ...

    def __eq__(self, rhs):
        if isinstance(rhs, Variable):
            return VarEqVar(self, rhs)
        return super().__eq__(rhs)

    @typing.overload
    def __ne__(self, rhs: "Variable") -> bool: ...

    @typing.overload
    def __ne__(self, rhs: LinearTypesExceptVariable) -> NoReturn: ...

    def __ne__(self, rhs):
        if isinstance(rhs, Variable):
            return not self == rhs
        _raise_ne_not_supported()

    def __hash__(self) -> int:
        return hash((self.model, self.id))

    @typing.overload
    def __mul__(self, other: float) -> "LinearTerm": ...

    @typing.overload
    def __mul__(self, other: Union["Variable", "LinearTerm"]) -> "QuadraticTerm": ...

    @typing.overload
    def __mul__(self, other: "LinearBase") -> "LinearLinearProduct": ...

    def __mul__(self, other):
        if not isinstance(other, (int, float, LinearBase)):
            return NotImplemented
        if isinstance(other, Variable):
            return QuadraticTerm(QuadraticTermKey(self, other), 1.0)
        if isinstance(other, LinearTerm):
            return QuadraticTerm(
                QuadraticTermKey(self, other.variable), other.coefficient
            )
        if isinstance(other, LinearBase):
            return LinearLinearProduct(self, other)  # pytype: disable=bad-return-type
        return LinearTerm(self, other)

    def __rmul__(self, constant: float) -> "LinearTerm":
        if not isinstance(constant, (int, float)):
            return NotImplemented
        return LinearTerm(self, constant)

    # TODO(b/216492143): explore numerical consequences of 1.0 / constant below.
    def __truediv__(self, constant: float) -> "LinearTerm":
        if not isinstance(constant, (int, float)):
            return NotImplemented
        return LinearTerm(self, 1.0 / constant)

    def __neg__(self) -> "LinearTerm":
        return LinearTerm(self, -1.0)

    def _flatten_once_and_add_to(
        self,
        scale: float,
        processed_elements: _ProcessedElements,
        target_stack: _ToProcessElements,
    ) -> None:
        processed_elements.terms[self] += scale


class LinearTerm(LinearBase):
    """The product of a scalar and a variable.

    This class is immutable.
    """

    __slots__ = "_variable", "_coefficient"

    def __init__(self, variable: Variable, coefficient: float) -> None:
        self._variable: Variable = variable
        self._coefficient: float = coefficient

    @property
    def variable(self) -> Variable:
        return self._variable

    @property
    def coefficient(self) -> float:
        return self._coefficient

    def _flatten_once_and_add_to(
        self,
        scale: float,
        processed_elements: _ProcessedElements,
        target_stack: _ToProcessElements,
    ) -> None:
        processed_elements.terms[self._variable] += self._coefficient * scale

    @typing.overload
    def __mul__(self, other: float) -> "LinearTerm": ...

    @typing.overload
    def __mul__(self, other: Union["Variable", "LinearTerm"]) -> "QuadraticTerm": ...

    @typing.overload
    def __mul__(self, other: "LinearBase") -> "LinearLinearProduct": ...

    def __mul__(self, other):
        if not isinstance(other, (int, float, LinearBase)):
            return NotImplemented
        if isinstance(other, Variable):
            return QuadraticTerm(
                QuadraticTermKey(self._variable, other), self._coefficient
            )
        if isinstance(other, LinearTerm):
            return QuadraticTerm(
                QuadraticTermKey(self.variable, other.variable),
                self._coefficient * other.coefficient,
            )
        if isinstance(other, LinearBase):
            return LinearLinearProduct(self, other)  # pytype: disable=bad-return-type
        return LinearTerm(self._variable, self._coefficient * other)

    def __rmul__(self, constant: float) -> "LinearTerm":
        if not isinstance(constant, (int, float)):
            return NotImplemented
        return LinearTerm(self._variable, self._coefficient * constant)

    def __truediv__(self, constant: float) -> "LinearTerm":
        if not isinstance(constant, (int, float)):
            return NotImplemented
        return LinearTerm(self._variable, self._coefficient / constant)

    def __neg__(self) -> "LinearTerm":
        return LinearTerm(self._variable, -self._coefficient)

    def __str__(self):
        return f"{self._coefficient} * {self._variable}"

    def __repr__(self):
        return f"LinearTerm({self._variable!r}, {self._coefficient!r})"


class QuadraticTerm(QuadraticBase):
    """The product of a scalar and two variables.

    This class is immutable.
    """

    __slots__ = "_key", "_coefficient"

    def __init__(self, key: QuadraticTermKey, coefficient: float) -> None:
        self._key: QuadraticTermKey = key
        self._coefficient: float = coefficient

    @property
    def key(self) -> QuadraticTermKey:
        return self._key

    @property
    def coefficient(self) -> float:
        return self._coefficient

    def _quadratic_flatten_once_and_add_to(
        self,
        scale: float,
        processed_elements: _QuadraticProcessedElements,
        target_stack: _ToProcessElements,
    ) -> None:
        processed_elements.quadratic_terms[self._key] += self._coefficient * scale

    def __mul__(self, constant: float) -> "QuadraticTerm":
        if not isinstance(constant, (int, float)):
            return NotImplemented
        return QuadraticTerm(self._key, self._coefficient * constant)

    def __rmul__(self, constant: float) -> "QuadraticTerm":
        if not isinstance(constant, (int, float)):
            return NotImplemented
        return QuadraticTerm(self._key, self._coefficient * constant)

    def __truediv__(self, constant: float) -> "QuadraticTerm":
        if not isinstance(constant, (int, float)):
            return NotImplemented
        return QuadraticTerm(self._key, self._coefficient / constant)

    def __neg__(self) -> "QuadraticTerm":
        return QuadraticTerm(self._key, -self._coefficient)

    def __str__(self):
        return f"{self._coefficient} * {self._key!s}"

    def __repr__(self):
        return f"QuadraticTerm({self._key!r}, {self._coefficient})"


class LinearExpression(LinearBase):
    """For variables x, an expression: b + sum_{i in I} a_i * x_i.

    This class is immutable.
    """

    __slots__ = "__weakref__", "_terms", "_offset"

    # TODO(b/216492143): consider initializing from a dictionary.
    def __init__(self, /, other: LinearTypes = 0) -> None:
        self._offset: float = 0.0
        if isinstance(other, (int, float)):
            self._offset = float(other)
            self._terms: Mapping[Variable, float] = immutabledict.immutabledict()
            return

        to_process: _LinearToProcessElements = _LinearToProcessElements(other, 1.0)
        processed_elements = _ProcessedElements()
        while to_process:
            linear, coef = to_process.pop()
            linear._flatten_once_and_add_to(coef, processed_elements, to_process)
        # TODO(b/216492143): explore avoiding this copy.
        self._terms: Mapping[Variable, float] = immutabledict.immutabledict(
            processed_elements.terms
        )
        self._offset = processed_elements.offset

    @property
    def terms(self) -> Mapping[Variable, float]:
        return self._terms

    @property
    def offset(self) -> float:
        return self._offset

    def evaluate(self, variable_values: Mapping[Variable, float]) -> float:
        """Returns the value of this expression for given variable values.

        E.g. if this is 3 * x + 4 and variable_values = {x: 2.0}, then
        evaluate(variable_values) equals 10.0.

        See also mathopt.evaluate_expression(), which works on any type in
        QuadraticTypes.

        Args:
          variable_values: Must contain a value for every variable in expression.

        Returns:
          The value of this expression when replacing variables by their value.
        """
        result = self._offset
        for var, coef in sorted(
            self._terms.items(), key=lambda var_coef_pair: var_coef_pair[0].id
        ):
            result += coef * variable_values[var]
        return result

    def _flatten_once_and_add_to(
        self,
        scale: float,
        processed_elements: _ProcessedElements,
        target_stack: _ToProcessElements,
    ) -> None:
        for var, val in self._terms.items():
            processed_elements.terms[var] += val * scale
        processed_elements.offset += scale * self.offset

    # TODO(b/216492143): change __str__ to match C++ implementation in
    # cl/421649402.
    def __str__(self):
        """Returns the name, or a string containing the id if the name is empty."""
        result = str(self.offset)
        sorted_keys = sorted(self._terms.keys(), key=str)
        for variable in sorted_keys:
            # TODO(b/216492143): consider how to better deal with `NaN` and try to
            # match C++ implementation in cl/421649402. See TODO for StrAndReprTest in
            # linear_expression_test.py.
            coefficient = self._terms[variable]
            if coefficient == 0.0:
                continue
            if coefficient > 0:
                result += " + "
            else:
                result += " - "
            result += str(abs(coefficient)) + " * " + str(variable)
        return result

    def __repr__(self):
        result = f"LinearExpression({self.offset}, " + "{"
        result += ", ".join(
            f"{variable!r}: {coefficient}"
            for variable, coefficient in self._terms.items()
        )
        result += "})"
        return result


class QuadraticExpression(QuadraticBase):
    """For variables x, an expression: b + sum_{i in I} a_i * x_i + sum_{i,j in I, i<=j} a_i,j * x_i * x_j.

    This class is immutable.
    """

    __slots__ = "__weakref__", "_linear_terms", "_quadratic_terms", "_offset"

    # TODO(b/216492143): consider initializing from a dictionary.
    def __init__(self, other: QuadraticTypes) -> None:
        self._offset: float = 0.0
        if isinstance(other, (int, float)):
            self._offset = float(other)
            self._linear_terms: Mapping[Variable, float] = immutabledict.immutabledict()
            self._quadratic_terms: Mapping[QuadraticTermKey, float] = (
                immutabledict.immutabledict()
            )
            return

        to_process: _QuadraticToProcessElements = _QuadraticToProcessElements(
            other, 1.0
        )
        processed_elements = _QuadraticProcessedElements()
        while to_process:
            linear_or_quadratic, coef = to_process.pop()
            if isinstance(linear_or_quadratic, LinearBase):
                linear_or_quadratic._flatten_once_and_add_to(
                    coef, processed_elements, to_process
                )
            else:
                linear_or_quadratic._quadratic_flatten_once_and_add_to(
                    coef, processed_elements, to_process
                )
        # TODO(b/216492143): explore avoiding this copy.
        self._linear_terms: Mapping[Variable, float] = immutabledict.immutabledict(
            processed_elements.terms
        )
        self._quadratic_terms: Mapping[QuadraticTermKey, float] = (
            immutabledict.immutabledict(processed_elements.quadratic_terms)
        )
        self._offset = processed_elements.offset

    @property
    def linear_terms(self) -> Mapping[Variable, float]:
        return self._linear_terms

    @property
    def quadratic_terms(self) -> Mapping[QuadraticTermKey, float]:
        return self._quadratic_terms

    @property
    def offset(self) -> float:
        return self._offset

    def evaluate(self, variable_values: Mapping[Variable, float]) -> float:
        """Returns the value of this expression for given variable values.

        E.g. if this is 3 * x * x + 4 and variable_values = {x: 2.0}, then
        evaluate(variable_values) equals 16.0.

        See also mathopt.evaluate_expression(), which works on any type in
        QuadraticTypes.

        Args:
          variable_values: Must contain a value for every variable in expression.

        Returns:
          The value of this expression when replacing variables by their value.
        """
        result = self._offset
        for var, coef in sorted(
            self._linear_terms.items(),
            key=lambda var_coef_pair: var_coef_pair[0].id,
        ):
            result += coef * variable_values[var]
        for key, coef in sorted(
            self._quadratic_terms.items(),
            key=lambda quad_coef_pair: (
                quad_coef_pair[0].first_var.id,
                quad_coef_pair[0].second_var.id,
            ),
        ):
            result += (
                coef * variable_values[key.first_var] * variable_values[key.second_var]
            )
        return result

    def _quadratic_flatten_once_and_add_to(
        self,
        scale: float,
        processed_elements: _QuadraticProcessedElements,
        target_stack: _QuadraticToProcessElements,
    ) -> None:
        for var, val in self._linear_terms.items():
            processed_elements.terms[var] += val * scale
        for key, val in self._quadratic_terms.items():
            processed_elements.quadratic_terms[key] += val * scale
        processed_elements.offset += scale * self.offset

    # TODO(b/216492143): change __str__ to match C++ implementation in
    # cl/421649402.
    def __str__(self):
        result = str(self.offset)
        sorted_linear_keys = sorted(self._linear_terms.keys(), key=str)
        for variable in sorted_linear_keys:
            # TODO(b/216492143): consider how to better deal with `NaN` and try to
            # match C++ implementation in cl/421649402. See TODO for StrAndReprTest in
            # linear_expression_test.py.
            coefficient = self._linear_terms[variable]
            if coefficient == 0.0:
                continue
            if coefficient > 0:
                result += " + "
            else:
                result += " - "
            result += str(abs(coefficient)) + " * " + str(variable)
        sorted_quadratic_keys = sorted(self._quadratic_terms.keys(), key=str)
        for key in sorted_quadratic_keys:
            # TODO(b/216492143): consider how to better deal with `NaN` and try to
            # match C++ implementation in cl/421649402. See TODO for StrAndReprTest in
            # linear_expression_test.py.
            coefficient = self._quadratic_terms[key]
            if coefficient == 0.0:
                continue
            if coefficient > 0:
                result += " + "
            else:
                result += " - "
            result += str(abs(coefficient)) + " * " + str(key)
        return result

    def __repr__(self):
        result = f"QuadraticExpression({self.offset}, " + "{"
        result += ", ".join(
            f"{variable!r}: {coefficient}"
            for variable, coefficient in self._linear_terms.items()
        )
        result += "}, {"
        result += ", ".join(
            f"{key!r}: {coefficient}"
            for key, coefficient in self._quadratic_terms.items()
        )
        result += "})"
        return result


class LinearConstraint:
    """A linear constraint for an optimization model.

    A LinearConstraint adds the following restriction on feasible solutions to an
    optimization model:
      lb <= sum_{i in I} a_i * x_i <= ub
    where x_i are the decision variables of the problem. lb == ub is allowed, this
    models the equality constraint:
      sum_{i in I} a_i * x_i == b
    lb > ub is also allowed, but the optimization problem will be infeasible.

    A LinearConstraint can be configured as follows:
      * lower_bound: a float property, lb above. Should not be NaN nor +inf.
      * upper_bound: a float property, ub above. Should not be NaN nor -inf.
      * set_coefficient() and get_coefficient(): get and set the a_i * x_i
        terms. The variable must be from the same model as this constraint, and
        the a_i must be finite and not NaN. The coefficient for any variable not
        set is 0.0, and setting a coefficient to 0.0 removes it from I above.

    The name is optional, read only, and used only for debugging. Non-empty names
    should be distinct.

    Every LinearConstraint is associated with a Model (defined below). Note that
    data describing the linear constraint (e.g. lower_bound) is owned by
    Model.storage, this class is simply a reference to that data. Do not create a
    LinearConstraint directly, use Model.add_linear_constraint() instead.
    """

    __slots__ = "__weakref__", "_model", "_id"

    def __init__(self, model: "Model", cid: int) -> None:
        """Do not invoke directly, use Model.add_linear_constraint()."""
        self._model: "Model" = model
        self._id: int = cid

    @property
    def lower_bound(self) -> float:
        return self.model.storage.get_linear_constraint_lb(self._id)

    @lower_bound.setter
    def lower_bound(self, value: float) -> None:
        self.model.storage.set_linear_constraint_lb(self._id, value)

    @property
    def upper_bound(self) -> float:
        return self.model.storage.get_linear_constraint_ub(self._id)

    @upper_bound.setter
    def upper_bound(self, value: float) -> None:
        self.model.storage.set_linear_constraint_ub(self._id, value)

    @property
    def name(self) -> str:
        return self.model.storage.get_linear_constraint_name(self._id)

    @property
    def id(self) -> int:
        return self._id

    @property
    def model(self) -> "Model":
        return self._model

    def set_coefficient(self, variable: Variable, coefficient: float) -> None:
        self.model.check_compatible(variable)
        self.model.storage.set_linear_constraint_coefficient(
            self._id, variable.id, coefficient
        )

    def get_coefficient(self, variable: Variable) -> float:
        self.model.check_compatible(variable)
        return self.model.storage.get_linear_constraint_coefficient(
            self._id, variable.id
        )

    def terms(self) -> Iterator[LinearTerm]:
        """Yields the variable/coefficient pairs with nonzero coefficient for this linear constraint."""
        for variable in self.model.row_nonzeros(self):
            yield LinearTerm(
                variable=variable,
                coefficient=self.model.storage.get_linear_constraint_coefficient(
                    self._id, variable.id
                ),
            )

    def as_bounded_linear_expression(self) -> BoundedLinearExpression:
        """Returns the bounded expression from lower_bound, upper_bound and terms."""
        return BoundedLinearExpression(
            self.lower_bound, LinearSum(self.terms()), self.upper_bound
        )

    def __str__(self):
        """Returns the name, or a string containing the id if the name is empty."""
        return self.name if self.name else f"linear_constraint_{self.id}"

    def __repr__(self):
        return f"<LinearConstraint id: {self.id}, name: {self.name!r}>"


class Objective:
    """The objective for an optimization model.

    An objective is either of the form:
      min o + sum_{i in I} c_i * x_i + sum_{i, j in I, i <= j} q_i,j * x_i * x_j
    or
      max o + sum_{i in I} c_i * x_i + sum_{(i, j) in Q} q_i,j * x_i * x_j
    where x_i are the decision variables of the problem and where all pairs (i, j)
    in Q satisfy i <= j. The values of o, c_i and q_i,j should be finite and not
    NaN.

    The objective can be configured as follows:
      * offset: a float property, o above. Should be finite and not NaN.
      * is_maximize: a bool property, if the objective is to maximize or minimize.
      * set_linear_coefficient and get_linear_coefficient control the c_i * x_i
        terms. The variables must be from the same model as this objective, and
        the c_i must be finite and not NaN. The coefficient for any variable not
        set is 0.0, and setting a coefficient to 0.0 removes it from I above.
      * set_quadratic_coefficient and get_quadratic_coefficient control the
        q_i,j * x_i * x_j terms. The variables must be from the same model as this
        objective, and the q_i,j must be finite and not NaN. The coefficient for
        any pair of variables not set is 0.0, and setting a coefficient to 0.0
        removes the associated (i,j) from Q above.

    Every Objective is associated with a Model (defined below). Note that data
    describing the objective (e.g. offset) is owned by Model.storage, this class
    is simply a reference to that data. Do not create an Objective directly,
    use Model.objective to access the objective instead.

    The objective will be linear if only linear coefficients are set. This can be
    useful to avoid solve-time errors with solvers that do not accept quadratic
    objectives. To facilitate this linear objective guarantee we provide three
    functions to add to the objective:
      * add(), which accepts linear or quadratic expressions,
      * add_quadratic(), which also accepts linear or quadratic expressions and
        can be used to signal a quadratic objective is possible, and
      * add_linear(), which only accepts linear expressions and can be used to
        guarantee the objective remains linear.
    """

    __slots__ = ("_model",)

    def __init__(self, model: "Model") -> None:
        """Do no invoke directly, use Model.objective."""
        self._model: "Model" = model

    @property
    def is_maximize(self) -> bool:
        return self.model.storage.get_is_maximize()

    @is_maximize.setter
    def is_maximize(self, is_maximize: bool) -> None:
        self.model.storage.set_is_maximize(is_maximize)

    @property
    def offset(self) -> float:
        return self.model.storage.get_objective_offset()

    @offset.setter
    def offset(self, value: float) -> None:
        self.model.storage.set_objective_offset(value)

    @property
    def model(self) -> "Model":
        return self._model

    def set_linear_coefficient(self, variable: Variable, coef: float) -> None:
        self.model.check_compatible(variable)
        self.model.storage.set_linear_objective_coefficient(variable.id, coef)

    def get_linear_coefficient(self, variable: Variable) -> float:
        self.model.check_compatible(variable)
        return self.model.storage.get_linear_objective_coefficient(variable.id)

    def linear_terms(self) -> Iterator[LinearTerm]:
        """Yields variable coefficient pairs for variables with nonzero objective coefficient in undefined order."""
        yield from self.model.linear_objective_terms()

    def add(self, objective: QuadraticTypes) -> None:
        """Adds the provided expression `objective` to the objective function.

        To ensure the objective remains linear through type checks, use
        add_linear().

        Args:
          objective: the expression to add to the objective function.
        """
        if isinstance(objective, (LinearBase, int, float)):
            self.add_linear(objective)
        elif isinstance(objective, QuadraticBase):
            self.add_quadratic(objective)
        else:
            raise TypeError(
                "unsupported type in objective argument for "
                f"Objective.add(): {type(objective).__name__!r}"
            )

    def add_linear(self, objective: LinearTypes) -> None:
        """Adds the provided linear expression `objective` to the objective function."""
        if not isinstance(objective, (LinearBase, int, float)):
            raise TypeError(
                "unsupported type in objective argument for "
                f"Objective.add_linear(): {type(objective).__name__!r}"
            )
        objective_expr = as_flat_linear_expression(objective)
        self.offset += objective_expr.offset
        for var, coefficient in objective_expr.terms.items():
            self.set_linear_coefficient(
                var, self.get_linear_coefficient(var) + coefficient
            )

    def add_quadratic(self, objective: QuadraticTypes) -> None:
        """Adds the provided quadratic expression `objective` to the objective function."""
        if not isinstance(objective, (QuadraticBase, LinearBase, int, float)):
            raise TypeError(
                "unsupported type in objective argument for "
                f"Objective.add(): {type(objective).__name__!r}"
            )
        objective_expr = as_flat_quadratic_expression(objective)
        self.offset += objective_expr.offset
        for var, coefficient in objective_expr.linear_terms.items():
            self.set_linear_coefficient(
                var, self.get_linear_coefficient(var) + coefficient
            )
        for key, coefficient in objective_expr.quadratic_terms.items():
            self.set_quadratic_coefficient(
                key.first_var,
                key.second_var,
                self.get_quadratic_coefficient(key.first_var, key.second_var)
                + coefficient,
            )

    def set_quadratic_coefficient(
        self, first_variable: Variable, second_variable: Variable, coef: float
    ) -> None:
        self.model.check_compatible(first_variable)
        self.model.check_compatible(second_variable)
        self.model.storage.set_quadratic_objective_coefficient(
            first_variable.id, second_variable.id, coef
        )

    def get_quadratic_coefficient(
        self, first_variable: Variable, second_variable: Variable
    ) -> float:
        self.model.check_compatible(first_variable)
        self.model.check_compatible(second_variable)
        return self.model.storage.get_quadratic_objective_coefficient(
            first_variable.id, second_variable.id
        )

    def quadratic_terms(self) -> Iterator[QuadraticTerm]:
        """Yields quadratic terms with nonzero objective coefficient in undefined order."""
        yield from self.model.quadratic_objective_terms()

    def as_linear_expression(self) -> LinearExpression:
        if any(self.quadratic_terms()):
            raise TypeError("Cannot get a quadratic objective as a linear expression")
        return as_flat_linear_expression(self.offset + LinearSum(self.linear_terms()))

    def as_quadratic_expression(self) -> QuadraticExpression:
        return as_flat_quadratic_expression(
            self.offset
            + LinearSum(self.linear_terms())
            + QuadraticSum(self.quadratic_terms())
        )

    def clear(self) -> None:
        """Clears objective coefficients and offset. Does not change direction."""
        self.model.storage.clear_objective()


class LinearConstraintMatrixEntry(NamedTuple):
    linear_constraint: LinearConstraint
    variable: Variable
    coefficient: float


class UpdateTracker:
    """Tracks updates to an optimization model from a ModelStorage.

    Do not instantiate directly, instead create through
    ModelStorage.add_update_tracker().

    Querying an UpdateTracker after calling Model.remove_update_tracker will
    result in a model_storage.UsedUpdateTrackerAfterRemovalError.

    Example:
      mod = Model()
      x = mod.add_variable(0.0, 1.0, True, 'x')
      y = mod.add_variable(0.0, 1.0, True, 'y')
      tracker = mod.add_update_tracker()
      mod.set_variable_ub(x, 3.0)
      tracker.export_update()
        => "variable_updates: {upper_bounds: {ids: [0], values[3.0] }"
      mod.set_variable_ub(y, 2.0)
      tracker.export_update()
        => "variable_updates: {upper_bounds: {ids: [0, 1], values[3.0, 2.0] }"
      tracker.advance_checkpoint()
      tracker.export_update()
        => None
      mod.set_variable_ub(y, 4.0)
      tracker.export_update()
        => "variable_updates: {upper_bounds: {ids: [1], values[4.0] }"
      tracker.advance_checkpoint()
      mod.remove_update_tracker(tracker)
    """

    def __init__(self, storage_update_tracker: model_storage.StorageUpdateTracker):
        """Do not invoke directly, use Model.add_update_tracker() instead."""
        self.storage_update_tracker = storage_update_tracker

    def export_update(self) -> Optional[model_update_pb2.ModelUpdateProto]:
        """Returns changes to the model since last call to checkpoint/creation."""
        return self.storage_update_tracker.export_update()

    def advance_checkpoint(self) -> None:
        """Track changes to the model only after this function call."""
        return self.storage_update_tracker.advance_checkpoint()


class Model:
    """An optimization model.

    The objective function of the model can be linear or quadratic, and some
    solvers can only handle linear objective functions. For this reason Model has
    three versions of all objective setting functions:
      * A generic one (e.g. maximize()), which accepts linear or quadratic
        expressions,
      * a quadratic version (e.g. maximize_quadratic_objective()), which also
        accepts linear or quadratic expressions and can be used to signal a
        quadratic objective is possible, and
      * a linear version (e.g. maximize_linear_objective()), which only accepts
        linear expressions and can be used to avoid solve time errors for solvers
        that do not accept quadratic objectives.

    Attributes:
      name: A description of the problem, can be empty.
      objective: A function to maximize or minimize.
      storage: Implementation detail, do not access directly.
      _variable_ids: Maps variable ids to Variable objects.
      _linear_constraint_ids: Maps linear constraint ids to LinearConstraint
        objects.
    """

    __slots__ = "storage", "_variable_ids", "_linear_constraint_ids", "_objective"

    def __init__(
        self,
        *,
        name: str = "",
        storage_class: StorageClass = hash_model_storage.HashModelStorage,
    ) -> None:
        self.storage: Storage = storage_class(name)
        # Weak references are used to eliminate reference cycles (so that Model will
        # be destroyed when we reach zero references, instead of relying on GC cycle
        # detection). Do not access a variable or constraint from these maps
        # directly, as they may no longer exist, use _get_or_make_variable() and
        # _get_or_make_linear_constraint() defined below instead.
        self._variable_ids: weakref.WeakValueDictionary[int, Variable] = (
            weakref.WeakValueDictionary()
        )
        self._linear_constraint_ids: weakref.WeakValueDictionary[
            int, LinearConstraint
        ] = weakref.WeakValueDictionary()
        self._objective: Objective = Objective(self)

    @property
    def name(self) -> str:
        return self.storage.name

    @property
    def objective(self) -> Objective:
        return self._objective

    def add_variable(
        self,
        *,
        lb: float = -math.inf,
        ub: float = math.inf,
        is_integer: bool = False,
        name: str = "",
    ) -> Variable:
        """Adds a decision variable to the optimization model.

        Args:
          lb: The new variable must take at least this value (a lower bound).
          ub: The new variable must be at most this value (an upper bound).
          is_integer: Indicates if the variable can only take integer values
            (otherwise, the variable can take any continuous value).
          name: For debugging purposes only, but nonempty names must be distinct.

        Returns:
          A reference to the new decision variable.
        """
        variable_id = self.storage.add_variable(lb, ub, is_integer, name)
        result = Variable(self, variable_id)
        self._variable_ids[variable_id] = result
        return result

    def add_integer_variable(
        self, *, lb: float = -math.inf, ub: float = math.inf, name: str = ""
    ) -> Variable:
        return self.add_variable(lb=lb, ub=ub, is_integer=True, name=name)

    def add_binary_variable(self, *, name: str = "") -> Variable:
        return self.add_variable(lb=0.0, ub=1.0, is_integer=True, name=name)

    def get_variable(self, var_id: int) -> Variable:
        """Returns the Variable for the id var_id, or raises KeyError."""
        if not self.storage.variable_exists(var_id):
            raise KeyError(f"variable does not exist with id {var_id}")
        return self._get_or_make_variable(var_id)

    def delete_variable(self, variable: Variable) -> None:
        self.check_compatible(variable)
        self.storage.delete_variable(variable.id)
        del self._variable_ids[variable.id]

    def variables(self) -> Iterator[Variable]:
        """Yields the variables in the order of creation."""
        for var_id in self.storage.get_variables():
            yield self._get_or_make_variable(var_id)

    def maximize(self, objective: QuadraticTypes) -> None:
        """Sets the objective to maximize the provided expression `objective`."""
        self.set_objective(objective, is_maximize=True)

    def maximize_linear_objective(self, objective: LinearTypes) -> None:
        """Sets the objective to maximize the provided linear expression `objective`."""
        self.set_linear_objective(objective, is_maximize=True)

    def maximize_quadratic_objective(self, objective: QuadraticTypes) -> None:
        """Sets the objective to maximize the provided quadratic expression `objective`."""
        self.set_quadratic_objective(objective, is_maximize=True)

    def minimize(self, objective: QuadraticTypes) -> None:
        """Sets the objective to minimize the provided expression `objective`."""
        self.set_objective(objective, is_maximize=False)

    def minimize_linear_objective(self, objective: LinearTypes) -> None:
        """Sets the objective to minimize the provided linear expression `objective`."""
        self.set_linear_objective(objective, is_maximize=False)

    def minimize_quadratic_objective(self, objective: QuadraticTypes) -> None:
        """Sets the objective to minimize the provided quadratic expression `objective`."""
        self.set_quadratic_objective(objective, is_maximize=False)

    def set_objective(self, objective: QuadraticTypes, *, is_maximize: bool) -> None:
        """Sets the objective to optimize the provided expression `objective`."""
        if isinstance(objective, (LinearBase, int, float)):
            self.set_linear_objective(objective, is_maximize=is_maximize)
        elif isinstance(objective, QuadraticBase):
            self.set_quadratic_objective(objective, is_maximize=is_maximize)
        else:
            raise TypeError(
                "unsupported type in objective argument for "
                f"set_objective: {type(objective).__name__!r}"
            )

    def set_linear_objective(
        self, objective: LinearTypes, *, is_maximize: bool
    ) -> None:
        """Sets the objective to optimize the provided linear expression `objective`."""
        if not isinstance(objective, (LinearBase, int, float)):
            raise TypeError(
                "unsupported type in objective argument for "
                f"set_linear_objective: {type(objective).__name__!r}"
            )
        self.storage.clear_objective()
        self._objective.is_maximize = is_maximize
        objective_expr = as_flat_linear_expression(objective)
        self._objective.offset = objective_expr.offset
        for var, coefficient in objective_expr.terms.items():
            self._objective.set_linear_coefficient(var, coefficient)

    def set_quadratic_objective(
        self, objective: QuadraticTypes, *, is_maximize: bool
    ) -> None:
        """Sets the objective to optimize the provided linear expression `objective`."""
        if not isinstance(objective, (QuadraticBase, LinearBase, int, float)):
            raise TypeError(
                "unsupported type in objective argument for "
                f"set_quadratic_objective: {type(objective).__name__!r}"
            )
        self.storage.clear_objective()
        self._objective.is_maximize = is_maximize
        objective_expr = as_flat_quadratic_expression(objective)
        self._objective.offset = objective_expr.offset
        for var, coefficient in objective_expr.linear_terms.items():
            self._objective.set_linear_coefficient(var, coefficient)
        for key, coefficient in objective_expr.quadratic_terms.items():
            self._objective.set_quadratic_coefficient(
                key.first_var, key.second_var, coefficient
            )

    # TODO(b/227214976): Update the note below and link to pytype bug number.
    # Note: bounded_expr's type includes bool only as a workaround to a pytype
    # issue. Passing a bool for bounded_expr will raise an error in runtime.
    def add_linear_constraint(
        self,
        bounded_expr: Optional[Union[bool, BoundedLinearTypes]] = None,
        *,
        lb: Optional[float] = None,
        ub: Optional[float] = None,
        expr: Optional[LinearTypes] = None,
        name: str = "",
    ) -> LinearConstraint:
        """Adds a linear constraint to the optimization model.

        The simplest way to specify the constraint is by passing a one-sided or
        two-sided linear inequality as in:
          * add_linear_constraint(x + y + 1.0 <= 2.0),
          * add_linear_constraint(x + y >= 2.0), or
          * add_linear_constraint((1.0 <= x + y) <= 2.0).

        Note the extra parenthesis for two-sided linear inequalities, which is
        required due to some language limitations (see
        https://peps.python.org/pep-0335/ and https://peps.python.org/pep-0535/).
        If the parenthesis are omitted, a TypeError will be raised explaining the
        issue (if this error was not raised the first inequality would have been
        silently ignored because of the noted language limitations).

        The second way to specify the constraint is by setting lb, ub, and/o expr as
        in:
          * add_linear_constraint(expr=x + y + 1.0, ub=2.0),
          * add_linear_constraint(expr=x + y, lb=2.0),
          * add_linear_constraint(expr=x + y, lb=1.0, ub=2.0), or
          * add_linear_constraint(lb=1.0).
        Omitting lb is equivalent to setting it to -math.inf and omiting ub is
        equivalent to setting it to math.inf.

        These two alternatives are exclusive and a combined call like:
          * add_linear_constraint(x + y <= 2.0, lb=1.0), or
          * add_linear_constraint(x + y <= 2.0, ub=math.inf)
        will raise a ValueError. A ValueError is also raised if expr's offset is
        infinite.

        Args:
          bounded_expr: a linear inequality describing the constraint. Cannot be
            specified together with lb, ub, or expr.
          lb: The constraint's lower bound if bounded_expr is omitted (if both
            bounder_expr and lb are omitted, the lower bound is -math.inf).
          ub: The constraint's upper bound if bounded_expr is omitted (if both
            bounder_expr and ub are omitted, the upper bound is math.inf).
          expr: The constraint's linear expression if bounded_expr is omitted.
          name: For debugging purposes only, but nonempty names must be distinct.

        Returns:
          A reference to the new linear constraint.
        """
        normalized_inequality = as_normalized_linear_inequality(
            bounded_expr, lb=lb, ub=ub, expr=expr
        )
        lin_con_id = self.storage.add_linear_constraint(
            normalized_inequality.lb, normalized_inequality.ub, name
        )
        result = LinearConstraint(self, lin_con_id)
        self._linear_constraint_ids[lin_con_id] = result
        for var, coefficient in normalized_inequality.coefficients.items():
            result.set_coefficient(var, coefficient)
        return result

    def get_linear_constraint(self, lin_con_id: int) -> LinearConstraint:
        """Returns the LinearConstraint for the id lin_con_id."""
        if not self.storage.linear_constraint_exists(lin_con_id):
            raise KeyError(f"linear constraint does not exist with id {lin_con_id}")
        return self._get_or_make_linear_constraint(lin_con_id)

    def delete_linear_constraint(self, linear_constraint: LinearConstraint) -> None:
        self.check_compatible(linear_constraint)
        self.storage.delete_linear_constraint(linear_constraint.id)
        del self._linear_constraint_ids[linear_constraint.id]

    def linear_constraints(self) -> Iterator[LinearConstraint]:
        """Yields the linear constraints in the order of creation."""
        for lin_con_id in self.storage.get_linear_constraints():
            yield self._get_or_make_linear_constraint(lin_con_id)

    def row_nonzeros(self, linear_constraint: LinearConstraint) -> Iterator[Variable]:
        """Yields the variables with nonzero coefficient for this linear constraint."""
        for var_id in self.storage.get_variables_for_linear_constraint(
            linear_constraint.id
        ):
            yield self._get_or_make_variable(var_id)

    def column_nonzeros(self, variable: Variable) -> Iterator[LinearConstraint]:
        """Yields the linear constraints with nonzero coefficient for this variable."""
        for lin_con_id in self.storage.get_linear_constraints_with_variable(
            variable.id
        ):
            yield self._get_or_make_linear_constraint(lin_con_id)

    def linear_objective_terms(self) -> Iterator[LinearTerm]:
        """Yields variable coefficient pairs for variables with nonzero objective coefficient in undefined order."""
        for term in self.storage.get_linear_objective_coefficients():
            yield LinearTerm(
                variable=self._get_or_make_variable(term.variable_id),
                coefficient=term.coefficient,
            )

    def quadratic_objective_terms(self) -> Iterator[QuadraticTerm]:
        """Yields the quadratic terms with nonzero objective coefficient in undefined order."""
        for term in self.storage.get_quadratic_objective_coefficients():
            var1 = self._get_or_make_variable(term.id_key.id1)
            var2 = self._get_or_make_variable(term.id_key.id2)
            yield QuadraticTerm(
                key=QuadraticTermKey(var1, var2), coefficient=term.coefficient
            )

    def linear_constraint_matrix_entries(
        self,
    ) -> Iterator[LinearConstraintMatrixEntry]:
        """Yields the nonzero elements of the linear constraint matrix in undefined order."""
        for entry in self.storage.get_linear_constraint_matrix_entries():
            yield LinearConstraintMatrixEntry(
                linear_constraint=self._get_or_make_linear_constraint(
                    entry.linear_constraint_id
                ),
                variable=self._get_or_make_variable(entry.variable_id),
                coefficient=entry.coefficient,
            )

    def export_model(self) -> model_pb2.ModelProto:
        return self.storage.export_model()

    def add_update_tracker(self) -> UpdateTracker:
        """Creates an UpdateTracker registered on this model to view changes."""
        return UpdateTracker(self.storage.add_update_tracker())

    def remove_update_tracker(self, tracker: UpdateTracker):
        """Stops tracker from getting updates on changes to this model.

        An error will be raised if tracker was not created by this Model or if
        tracker has been previously removed.

        Using (via checkpoint or update) an UpdateTracker after it has been removed
        will result in an error.

        Args:
          tracker: The UpdateTracker to unregister.

        Raises:
          KeyError: The tracker was created by another model or was already removed.
        """
        self.storage.remove_update_tracker(tracker.storage_update_tracker)

    def check_compatible(
        self, var_or_constraint: Union[Variable, LinearConstraint]
    ) -> None:
        """Raises a ValueError if the model of var_or_constraint is not self."""
        if var_or_constraint.model is not self:
            raise ValueError(
                f"{var_or_constraint} is from model {var_or_constraint.model} and"
                f" cannot be used with model {self}"
            )

    def _get_or_make_variable(self, variable_id: int) -> Variable:
        result = self._variable_ids.get(variable_id)
        if result:
            return result
        result = Variable(self, variable_id)
        self._variable_ids[variable_id] = result
        return result

    def _get_or_make_linear_constraint(
        self, linear_constraint_id: int
    ) -> LinearConstraint:
        result = self._linear_constraint_ids.get(linear_constraint_id)
        if result:
            return result
        result = LinearConstraint(self, linear_constraint_id)
        self._linear_constraint_ids[linear_constraint_id] = result
        return result


class LinearSum(LinearBase):
    # TODO(b/216492143): consider what details to move elsewhere and/or replace
    # by examples, and do complexity analysis.
    """A deferred sum of LinearBase objects.

    LinearSum objects are automatically created when two linear objects are added
    and, as noted in the documentation for Linear, can reduce the inefficiencies.
    In particular, they are created when calling sum(iterable) when iterable is
    an Iterable[LinearTypes]. However, using LinearSum(iterable) instead
    can result in additional performance improvements:

      * sum(iterable): creates a nested set of LinearSum objects (e.g.
        `sum([a, b, c])` is `LinearSum(0, LinearSum(a, LinearSum(b, c)))`).
      * LinearSum(iterable): creates a single LinearSum that saves a tuple with
        all the LinearTypes objects in iterable (e.g.
        `LinearSum([a, b, c])` does not create additional objects).

    This class is immutable.
    """

    __slots__ = "__weakref__", "_elements"

    # Potentially unsafe use of Iterable argument is handled by immediate local
    # storage as tuple.
    def __init__(self, iterable: Iterable[LinearTypes]) -> None:
        """Creates a LinearSum object. A copy of iterable is saved as a tuple."""

        self._elements = tuple(iterable)
        for item in self._elements:
            if not isinstance(item, (LinearBase, int, float)):
                raise TypeError(
                    "unsupported type in iterable argument for "
                    f"LinearSum: {type(item).__name__!r}"
                )

    @property
    def elements(self) -> Tuple[LinearTypes, ...]:
        return self._elements

    def _flatten_once_and_add_to(
        self,
        scale: float,
        processed_elements: _ProcessedElements,
        target_stack: _ToProcessElements,
    ) -> None:
        for term in self._elements:
            if isinstance(term, (int, float)):
                processed_elements.offset += scale * float(term)
            else:
                target_stack.append(term, scale)

    def __str__(self):
        return str(as_flat_linear_expression(self))

    def __repr__(self):
        result = "LinearSum(("
        result += ", ".join(repr(linear) for linear in self._elements)
        result += "))"
        return result


class QuadraticSum(QuadraticBase):
    # TODO(b/216492143): consider what details to move elsewhere and/or replace
    # by examples, and do complexity analysis.
    """A deferred sum of QuadraticTypes objects.

    QuadraticSum objects are automatically created when a quadratic object is
    added to quadratic or linear objects and, as has performance optimizations
    similar to LinearSum.

    This class is immutable.
    """

    __slots__ = "__weakref__", "_elements"

    # Potentially unsafe use of Iterable argument is handled by immediate local
    # storage as tuple.
    def __init__(self, iterable: Iterable[QuadraticTypes]) -> None:
        """Creates a QuadraticSum object. A copy of iterable is saved as a tuple."""

        self._elements = tuple(iterable)
        for item in self._elements:
            if not isinstance(item, (LinearBase, QuadraticBase, int, float)):
                raise TypeError(
                    "unsupported type in iterable argument for "
                    f"QuadraticSum: {type(item).__name__!r}"
                )

    @property
    def elements(self) -> Tuple[QuadraticTypes, ...]:
        return self._elements

    def _quadratic_flatten_once_and_add_to(
        self,
        scale: float,
        processed_elements: _QuadraticProcessedElements,
        target_stack: _QuadraticToProcessElements,
    ) -> None:
        for term in self._elements:
            if isinstance(term, (int, float)):
                processed_elements.offset += scale * float(term)
            else:
                target_stack.append(term, scale)

    def __str__(self):
        return str(as_flat_quadratic_expression(self))

    def __repr__(self):
        result = "QuadraticSum(("
        result += ", ".join(repr(element) for element in self._elements)
        result += "))"
        return result


class LinearProduct(LinearBase):
    """A deferred multiplication computation for linear expressions.

    This class is immutable.
    """

    __slots__ = "_scalar", "_linear"

    def __init__(self, scalar: float, linear: LinearBase) -> None:
        if not isinstance(scalar, (float, int)):
            raise TypeError(
                "unsupported type for scalar argument in "
                f"LinearProduct: {type(scalar).__name__!r}"
            )
        if not isinstance(linear, LinearBase):
            raise TypeError(
                "unsupported type for linear argument in "
                f"LinearProduct: {type(linear).__name__!r}"
            )
        self._scalar: float = float(scalar)
        self._linear: LinearBase = linear

    @property
    def scalar(self) -> float:
        return self._scalar

    @property
    def linear(self) -> LinearBase:
        return self._linear

    def _flatten_once_and_add_to(
        self,
        scale: float,
        processed_elements: _ProcessedElements,
        target_stack: _ToProcessElements,
    ) -> None:
        target_stack.append(self._linear, self._scalar * scale)

    def __str__(self):
        return str(as_flat_linear_expression(self))

    def __repr__(self):
        result = f"LinearProduct({self._scalar!r}, "
        result += f"{self._linear!r})"
        return result


class QuadraticProduct(QuadraticBase):
    """A deferred multiplication computation for quadratic expressions.

    This class is immutable.
    """

    __slots__ = "_scalar", "_quadratic"

    def __init__(self, scalar: float, quadratic: QuadraticBase) -> None:
        if not isinstance(scalar, (float, int)):
            raise TypeError(
                "unsupported type for scalar argument in "
                f"QuadraticProduct: {type(scalar).__name__!r}"
            )
        if not isinstance(quadratic, QuadraticBase):
            raise TypeError(
                "unsupported type for linear argument in "
                f"QuadraticProduct: {type(quadratic).__name__!r}"
            )
        self._scalar: float = float(scalar)
        self._quadratic: QuadraticBase = quadratic

    @property
    def scalar(self) -> float:
        return self._scalar

    @property
    def quadratic(self) -> QuadraticBase:
        return self._quadratic

    def _quadratic_flatten_once_and_add_to(
        self,
        scale: float,
        processed_elements: _QuadraticProcessedElements,
        target_stack: _QuadraticToProcessElements,
    ) -> None:
        target_stack.append(self._quadratic, self._scalar * scale)

    def __str__(self):
        return str(as_flat_quadratic_expression(self))

    def __repr__(self):
        return f"QuadraticProduct({self._scalar}, {self._quadratic!r})"


class LinearLinearProduct(QuadraticBase):
    """A deferred multiplication of two linear expressions.

    This class is immutable.
    """

    __slots__ = "_first_linear", "_second_linear"

    def __init__(self, first_linear: LinearBase, second_linear: LinearBase) -> None:
        if not isinstance(first_linear, LinearBase):
            raise TypeError(
                "unsupported type for first_linear argument in "
                f"LinearLinearProduct: {type(first_linear).__name__!r}"
            )
        if not isinstance(second_linear, LinearBase):
            raise TypeError(
                "unsupported type for second_linear argument in "
                f"LinearLinearProduct: {type(second_linear).__name__!r}"
            )
        self._first_linear: LinearBase = first_linear
        self._second_linear: LinearBase = second_linear

    @property
    def first_linear(self) -> LinearBase:
        return self._first_linear

    @property
    def second_linear(self) -> LinearBase:
        return self._second_linear

    def _quadratic_flatten_once_and_add_to(
        self,
        scale: float,
        processed_elements: _QuadraticProcessedElements,
        target_stack: _QuadraticToProcessElements,
    ) -> None:
        # A recursion is avoided here because as_flat_linear_expression() must never
        # call _quadratic_flatten_once_and_add_to().
        first_expression = as_flat_linear_expression(self._first_linear)
        second_expression = as_flat_linear_expression(self._second_linear)
        processed_elements.offset += (
            first_expression.offset * second_expression.offset * scale
        )
        for first_var, first_val in first_expression.terms.items():
            processed_elements.terms[first_var] += (
                second_expression.offset * first_val * scale
            )
        for second_var, second_val in second_expression.terms.items():
            processed_elements.terms[second_var] += (
                first_expression.offset * second_val * scale
            )

        for first_var, first_val in first_expression.terms.items():
            for second_var, second_val in second_expression.terms.items():
                processed_elements.quadratic_terms[
                    QuadraticTermKey(first_var, second_var)
                ] += (first_val * second_val * scale)

    def __str__(self):
        return str(as_flat_quadratic_expression(self))

    def __repr__(self):
        result = "LinearLinearProduct("
        result += f"{self._first_linear!r}, "
        result += f"{self._second_linear!r})"
        return result


def as_flat_linear_expression(value: LinearTypes) -> LinearExpression:
    """Converts floats, ints and Linear objects to a LinearExpression."""
    if isinstance(value, LinearExpression):
        return value
    return LinearExpression(value)


def as_flat_quadratic_expression(value: QuadraticTypes) -> QuadraticExpression:
    """Converts floats, ints, LinearBase and QuadraticBase objects to a QuadraticExpression."""
    if isinstance(value, QuadraticExpression):
        return value
    return QuadraticExpression(value)


@dataclasses.dataclass
class NormalizedLinearInequality:
    """Represents an inequality lb <= expr <= ub where expr's offset is zero.

    The inequality is of the form:
        lb <= sum_{x in V} coefficients[x] * x <= ub
    where V is the set of keys of coefficients.
    """

    lb: float
    ub: float
    coefficients: Mapping[Variable, float]

    def __init__(self, *, lb: float, ub: float, expr: LinearTypes) -> None:
        """Raises a ValueError if expr's offset is infinite."""
        flat_expr = as_flat_linear_expression(expr)
        if math.isinf(flat_expr.offset):
            raise ValueError(
                "Trying to create a linear constraint whose expression has an"
                " infinite offset."
            )
        self.lb = lb - flat_expr.offset
        self.ub = ub - flat_expr.offset
        self.coefficients = flat_expr.terms


# TODO(b/227214976): Update the note below and link to pytype bug number.
# Note: bounded_expr's type includes bool only as a workaround to a pytype
# issue. Passing a bool for bounded_expr will raise an error in runtime.
def as_normalized_linear_inequality(
    bounded_expr: Optional[Union[bool, BoundedLinearTypes]] = None,
    *,
    lb: Optional[float] = None,
    ub: Optional[float] = None,
    expr: Optional[LinearTypes] = None,
) -> NormalizedLinearInequality:
    """Converts a linear constraint to a NormalizedLinearInequality.

    The simplest way to specify the constraint is by passing a one-sided or
    two-sided linear inequality as in:
      * as_normalized_linear_inequality(x + y + 1.0 <= 2.0),
      * as_normalized_linear_inequality(x + y >= 2.0), or
      * as_normalized_linear_inequality((1.0 <= x + y) <= 2.0).

    Note the extra parenthesis for two-sided linear inequalities, which is
    required due to some language limitations (see
    https://peps.python.org/pep-0335/ and https://peps.python.org/pep-0535/).
    If the parenthesis are omitted, a TypeError will be raised explaining the
    issue (if this error was not raised the first inequality would have been
    silently ignored because of the noted language limitations).

    The second way to specify the constraint is by setting lb, ub, and/o expr as
    in:
      * as_normalized_linear_inequality(expr=x + y + 1.0, ub=2.0),
      * as_normalized_linear_inequality(expr=x + y, lb=2.0),
      * as_normalized_linear_inequality(expr=x + y, lb=1.0, ub=2.0), or
      * as_normalized_linear_inequality(lb=1.0).
    Omitting lb is equivalent to setting it to -math.inf and omiting ub is
    equivalent to setting it to math.inf.

    These two alternatives are exclusive and a combined call like:
      * as_normalized_linear_inequality(x + y <= 2.0, lb=1.0), or
      * as_normalized_linear_inequality(x + y <= 2.0, ub=math.inf)
    will raise a ValueError. A ValueError is also raised if expr's offset is
    infinite.

    Args:
      bounded_expr: a linear inequality describing the constraint. Cannot be
        specified together with lb, ub, or expr.
      lb: The constraint's lower bound if bounded_expr is omitted (if both
        bounder_expr and lb are omitted, the lower bound is -math.inf).
      ub: The constraint's upper bound if bounded_expr is omitted (if both
        bounder_expr and ub are omitted, the upper bound is math.inf).
      expr: The constraint's linear expression if bounded_expr is omitted.

    Returns:
      A NormalizedLinearInequality representing the linear constraint.
    """
    if bounded_expr is None:
        if lb is None:
            lb = -math.inf
        if ub is None:
            ub = math.inf
        if expr is None:
            return NormalizedLinearInequality(lb=lb, ub=ub, expr=0)
        if not isinstance(expr, (LinearBase, int, float)):
            raise TypeError(
                f"unsupported type for expr argument: {type(expr).__name__!r}"
            )
        return NormalizedLinearInequality(lb=lb, ub=ub, expr=expr)

    if isinstance(bounded_expr, bool):
        raise TypeError(
            "unsupported type for bounded_expr argument:"
            " bool. This error can occur when trying to add != constraints "
            "(which are not supported) or inequalities/equalities with constant "
            "left-hand-side and right-hand-side (which are redundant or make a "
            "model infeasible)."
        )
    if not isinstance(
        bounded_expr,
        (
            LowerBoundedLinearExpression,
            UpperBoundedLinearExpression,
            BoundedLinearExpression,
            VarEqVar,
        ),
    ):
        raise TypeError(
            f"unsupported type for bounded_expr: {type(bounded_expr).__name__!r}"
        )
    if lb is not None:
        raise ValueError("lb cannot be specified together with a linear inequality")
    if ub is not None:
        raise ValueError("ub cannot be specified together with a linear inequality")
    if expr is not None:
        raise ValueError("expr cannot be specified together with a linear inequality")

    if isinstance(bounded_expr, VarEqVar):
        return NormalizedLinearInequality(
            lb=0.0,
            ub=0.0,
            expr=bounded_expr.first_variable - bounded_expr.second_variable,
        )

    if isinstance(
        bounded_expr, (LowerBoundedLinearExpression, BoundedLinearExpression)
    ):
        lb = bounded_expr.lower_bound
    else:
        lb = -math.inf
    if isinstance(
        bounded_expr, (UpperBoundedLinearExpression, BoundedLinearExpression)
    ):
        ub = bounded_expr.upper_bound
    else:
        ub = math.inf
    return NormalizedLinearInequality(lb=lb, ub=ub, expr=bounded_expr.expression)
