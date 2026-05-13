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

"""Define Variables and Linear Expressions."""

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
    Mapping,
    NoReturn,
    Optional,
    Protocol,
    Tuple,
    Type,
    TypeVar,
    Union,
)

import immutabledict

from ortools.math_opt.elemental.python import enums
from ortools.math_opt.python import bounded_expressions
from ortools.math_opt.python import from_model
from ortools.math_opt.python.elemental import elemental


LinearTypes = Union[int, float, "LinearBase"]
QuadraticTypes = Union[int, float, "LinearBase", "QuadraticBase"]
LinearTypesExceptVariable = Union[
    float, int, "LinearTerm", "LinearExpression", "LinearSum", "LinearProduct"
]


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


LowerBoundedLinearExpression = bounded_expressions.LowerBoundedExpression["LinearBase"]
UpperBoundedLinearExpression = bounded_expressions.UpperBoundedExpression["LinearBase"]
BoundedLinearExpression = bounded_expressions.BoundedExpression["LinearBase"]


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
            self._first_variable.elemental is self._second_variable.elemental
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

LowerBoundedQuadraticExpression = bounded_expressions.LowerBoundedExpression[
    "QuadraticBase"
]
UpperBoundedQuadraticExpression = bounded_expressions.UpperBoundedExpression[
    "QuadraticBase"
]
BoundedQuadraticExpression = bounded_expressions.BoundedExpression["QuadraticBase"]

BoundedQuadraticTypesList = (
    LowerBoundedQuadraticExpression,
    UpperBoundedQuadraticExpression,
    BoundedQuadraticExpression,
)
BoundedQuadraticTypes = Union[BoundedQuadraticTypesList]


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
        BoundedLinearExpression
    ):  # pytype: disable=signature-mismatch  # overriding-return-type-checks
        # Note: when rhs is a QuadraticBase, this will cause rhs.__eq__(self) to be
        # invoked, which is defined.
        if isinstance(rhs, QuadraticBase):
            return NotImplemented
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
        # Note: when rhs is a QuadraticBase, this will cause rhs.__ge__(self) to be
        # invoked, which is defined.
        if isinstance(rhs, QuadraticBase):
            return NotImplemented
        if isinstance(rhs, (int, float)):
            return UpperBoundedLinearExpression(self, rhs)
        if isinstance(rhs, LinearBase):
            return BoundedLinearExpression(-math.inf, self - rhs, 0.0)
        if isinstance(rhs, bounded_expressions.BoundedExpression):
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
        # Note: when lhs is a QuadraticBase, this will cause lhs.__le__(self) to be
        # invoked, which is defined.
        if isinstance(lhs, QuadraticBase):
            return NotImplemented
        if isinstance(lhs, (int, float)):
            return LowerBoundedLinearExpression(self, lhs)
        if isinstance(lhs, LinearBase):
            return BoundedLinearExpression(0.0, self - lhs, math.inf)
        if isinstance(lhs, bounded_expressions.BoundedExpression):
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

    def __eq__(
        self, rhs: QuadraticTypes
    ) -> (
        BoundedQuadraticExpression
    ):  # pytype: disable=signature-mismatch  # overriding-return-type-checks
        if isinstance(rhs, (int, float)):
            return BoundedQuadraticExpression(rhs, self, rhs)
        if not isinstance(rhs, (LinearBase, QuadraticBase)):
            _raise_binary_operator_type_error("==", type(self), type(rhs))
        return BoundedQuadraticExpression(0.0, self - rhs, 0.0)

    def __ne__(
        self, rhs: QuadraticTypes
    ) -> (
        NoReturn
    ):  # pytype: disable=signature-mismatch  # overriding-return-type-checks
        _raise_ne_not_supported()

    @typing.overload
    def __le__(self, rhs: float) -> UpperBoundedQuadraticExpression: ...

    @typing.overload
    def __le__(
        self, rhs: Union[LinearBase, "QuadraticBase"]
    ) -> BoundedQuadraticExpression: ...

    @typing.overload
    def __le__(self, rhs: BoundedQuadraticExpression) -> NoReturn: ...

    def __le__(self, rhs):
        if isinstance(rhs, (int, float)):
            return UpperBoundedQuadraticExpression(self, rhs)
        if isinstance(rhs, (LinearBase, QuadraticBase)):
            return BoundedQuadraticExpression(-math.inf, self - rhs, 0.0)
        if isinstance(rhs, bounded_expressions.BoundedExpression):
            _raise_binary_operator_type_error(
                "<=", type(self), type(rhs), _EXPRESSION_COMP_EXPRESSION_MESSAGE
            )
        _raise_binary_operator_type_error("<=", type(self), type(rhs))

    @typing.overload
    def __ge__(self, lhs: float) -> LowerBoundedQuadraticExpression: ...

    @typing.overload
    def __ge__(
        self, lhs: Union[LinearBase, "QuadraticBase"]
    ) -> BoundedQuadraticExpression: ...

    @typing.overload
    def __ge__(self, lhs: BoundedQuadraticExpression) -> NoReturn: ...

    def __ge__(self, lhs):
        if isinstance(lhs, (int, float)):
            return LowerBoundedQuadraticExpression(self, lhs)
        if isinstance(lhs, (LinearBase, QuadraticBase)):
            return BoundedQuadraticExpression(0.0, self - lhs, math.inf)
        if isinstance(lhs, bounded_expressions.BoundedExpression):
            _raise_binary_operator_type_error(
                ">=", type(self), type(lhs), _EXPRESSION_COMP_EXPRESSION_MESSAGE
            )
        _raise_binary_operator_type_error(">=", type(self), type(lhs))

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


class Variable(LinearBase, from_model.FromModel):
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

    __slots__ = "_elemental", "_id"

    def __init__(self, elem: elemental.Elemental, vid: int) -> None:
        """Internal only, prefer Model functions (add_variable() and get_variable())."""
        if not isinstance(vid, int):
            raise TypeError(f"vid type should be int, was:{type(vid)}")
        self._elemental: elemental.Elemental = elem
        self._id: int = vid

    @property
    def lower_bound(self) -> float:
        return self._elemental.get_attr(
            enums.DoubleAttr1.VARIABLE_LOWER_BOUND, (self._id,)
        )

    @lower_bound.setter
    def lower_bound(self, value: float) -> None:
        self._elemental.set_attr(
            enums.DoubleAttr1.VARIABLE_LOWER_BOUND,
            (self._id,),
            value,
        )

    @property
    def upper_bound(self) -> float:
        return self._elemental.get_attr(
            enums.DoubleAttr1.VARIABLE_UPPER_BOUND, (self._id,)
        )

    @upper_bound.setter
    def upper_bound(self, value: float) -> None:
        self._elemental.set_attr(
            enums.DoubleAttr1.VARIABLE_UPPER_BOUND,
            (self._id,),
            value,
        )

    @property
    def integer(self) -> bool:
        return self._elemental.get_attr(enums.BoolAttr1.VARIABLE_INTEGER, (self._id,))

    @integer.setter
    def integer(self, value: bool) -> None:
        self._elemental.set_attr(enums.BoolAttr1.VARIABLE_INTEGER, (self._id,), value)

    @property
    def name(self) -> str:
        return self._elemental.get_element_name(enums.ElementType.VARIABLE, self._id)

    @property
    def id(self) -> int:
        return self._id

    @property
    def elemental(self) -> elemental.Elemental:
        """Internal use only."""
        return self._elemental

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
        return hash(self._id)

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
        for var in sorted_keys:
            # TODO(b/216492143): consider how to better deal with `NaN` and try to
            # match C++ implementation in cl/421649402. See TODO for StrAndReprTest in
            # linear_expression_test.py.
            coefficient = self._terms[var]
            if coefficient == 0.0:
                continue
            if coefficient > 0:
                result += " + "
            else:
                result += " - "
            result += str(abs(coefficient)) + " * " + str(var)
        return result

    def __repr__(self):
        result = f"LinearExpression({self.offset}, " + "{"
        result += ", ".join(
            f"{var!r}: {coefficient}" for var, coefficient in self._terms.items()
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
        for var in sorted_linear_keys:
            # TODO(b/216492143): consider how to better deal with `NaN` and try to
            # match C++ implementation in cl/421649402. See TODO for StrAndReprTest in
            # linear_expression_test.py.
            coefficient = self._linear_terms[var]
            if coefficient == 0.0:
                continue
            if coefficient > 0:
                result += " + "
            else:
                result += " - "
            result += str(abs(coefficient)) + " * " + str(var)
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
            f"{var!r}: {coefficient}" for var, coefficient in self._linear_terms.items()
        )
        result += "}, {"
        result += ", ".join(
            f"{key!r}: {coefficient}"
            for key, coefficient in self._quadratic_terms.items()
        )
        result += "})"
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
