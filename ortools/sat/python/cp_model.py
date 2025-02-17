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

"""Methods for building and solving CP-SAT models.

The following two sections describe the main
methods for building and solving CP-SAT models.

* [`CpModel`](#cp_model.CpModel): Methods for creating
models, including variables and constraints.
* [`CPSolver`](#cp_model.CpSolver): Methods for solving
a model and evaluating solutions.

The following methods implement callbacks that the
solver calls each time it finds a new solution.

* [`CpSolverSolutionCallback`](#cp_model.CpSolverSolutionCallback):
  A general method for implementing callbacks.
* [`ObjectiveSolutionPrinter`](#cp_model.ObjectiveSolutionPrinter):
  Print objective values and elapsed time for intermediate solutions.
* [`VarArraySolutionPrinter`](#cp_model.VarArraySolutionPrinter):
  Print intermediate solutions (variable values, time).
* [`VarArrayAndObjectiveSolutionPrinter`]
      (#cp_model.VarArrayAndObjectiveSolutionPrinter):
  Print both intermediate solutions and objective values.

Additional methods for solving CP-SAT models:

* [`Constraint`](#cp_model.Constraint): A few utility methods for modifying
  constraints created by `CpModel`.
* [`LinearExpr`](#lineacp_model.LinearExpr): Methods for creating constraints
  and the objective from large arrays of coefficients.

Other methods and functions listed are primarily used for developing OR-Tools,
rather than for solving specific optimization problems.
"""

import threading
import time
from typing import (
    Any,
    Callable,
    Dict,
    Iterable,
    Optional,
    Sequence,
    Tuple,
    Union,
    cast,
    overload,
)
import warnings

import numpy as np
import pandas as pd

from ortools.sat import cp_model_pb2
from ortools.sat import sat_parameters_pb2
from ortools.sat.python import cp_model_helper as cmh
from ortools.sat.python import cp_model_numbers as cmn
from ortools.util.python import sorted_interval_list

# Import external types.
Domain = sorted_interval_list.Domain
BoundedLinearExpression = cmh.BoundedLinearExpression
FlatFloatExpr = cmh.FlatFloatExpr
FlatIntExpr = cmh.FlatIntExpr
LinearExpr = cmh.LinearExpr
NotBooleanVariable = cmh.NotBooleanVariable


# The classes below allow linear expressions to be expressed naturally with the
# usual arithmetic operators + - * / and with constant numbers, which makes the
# python API very intuitive. See../ samples/*.py for examples.

INT_MIN = -(2**63)  # hardcoded to be platform independent.
INT_MAX = 2**63 - 1
INT32_MIN = -(2**31)
INT32_MAX = 2**31 - 1

# CpSolver status (exported to avoid importing cp_model_cp2).
UNKNOWN = cp_model_pb2.UNKNOWN
MODEL_INVALID = cp_model_pb2.MODEL_INVALID
FEASIBLE = cp_model_pb2.FEASIBLE
INFEASIBLE = cp_model_pb2.INFEASIBLE
OPTIMAL = cp_model_pb2.OPTIMAL

# Variable selection strategy
CHOOSE_FIRST = cp_model_pb2.DecisionStrategyProto.CHOOSE_FIRST
CHOOSE_LOWEST_MIN = cp_model_pb2.DecisionStrategyProto.CHOOSE_LOWEST_MIN
CHOOSE_HIGHEST_MAX = cp_model_pb2.DecisionStrategyProto.CHOOSE_HIGHEST_MAX
CHOOSE_MIN_DOMAIN_SIZE = cp_model_pb2.DecisionStrategyProto.CHOOSE_MIN_DOMAIN_SIZE
CHOOSE_MAX_DOMAIN_SIZE = cp_model_pb2.DecisionStrategyProto.CHOOSE_MAX_DOMAIN_SIZE

# Domain reduction strategy
SELECT_MIN_VALUE = cp_model_pb2.DecisionStrategyProto.SELECT_MIN_VALUE
SELECT_MAX_VALUE = cp_model_pb2.DecisionStrategyProto.SELECT_MAX_VALUE
SELECT_LOWER_HALF = cp_model_pb2.DecisionStrategyProto.SELECT_LOWER_HALF
SELECT_UPPER_HALF = cp_model_pb2.DecisionStrategyProto.SELECT_UPPER_HALF
SELECT_MEDIAN_VALUE = cp_model_pb2.DecisionStrategyProto.SELECT_MEDIAN_VALUE
SELECT_RANDOM_HALF = cp_model_pb2.DecisionStrategyProto.SELECT_RANDOM_HALF

# Search branching
AUTOMATIC_SEARCH = sat_parameters_pb2.SatParameters.AUTOMATIC_SEARCH
FIXED_SEARCH = sat_parameters_pb2.SatParameters.FIXED_SEARCH
PORTFOLIO_SEARCH = sat_parameters_pb2.SatParameters.PORTFOLIO_SEARCH
LP_SEARCH = sat_parameters_pb2.SatParameters.LP_SEARCH
PSEUDO_COST_SEARCH = sat_parameters_pb2.SatParameters.PSEUDO_COST_SEARCH
PORTFOLIO_WITH_QUICK_RESTART_SEARCH = (
    sat_parameters_pb2.SatParameters.PORTFOLIO_WITH_QUICK_RESTART_SEARCH
)
HINT_SEARCH = sat_parameters_pb2.SatParameters.HINT_SEARCH
PARTIAL_FIXED_SEARCH = sat_parameters_pb2.SatParameters.PARTIAL_FIXED_SEARCH
RANDOMIZED_SEARCH = sat_parameters_pb2.SatParameters.RANDOMIZED_SEARCH

# Type aliases
IntegralT = Union[int, np.int8, np.uint8, np.int32, np.uint32, np.int64, np.uint64]
IntegralTypes = (
    int,
    np.int8,
    np.uint8,
    np.int32,
    np.uint32,
    np.int64,
    np.uint64,
)
NumberT = Union[
    int,
    float,
    np.int8,
    np.uint8,
    np.int32,
    np.uint32,
    np.int64,
    np.uint64,
    np.double,
]
NumberTypes = (
    int,
    float,
    np.int8,
    np.uint8,
    np.int32,
    np.uint32,
    np.int64,
    np.uint64,
    np.double,
)

LiteralT = Union[cmh.Literal, IntegralT, bool]
BoolVarT = cmh.Literal
VariableT = Union["IntVar", IntegralT]

# We need to add 'IntVar' for pytype.
LinearExprT = Union[LinearExpr, "IntVar", IntegralT]
ObjLinearExprT = Union[LinearExpr, NumberT]

ArcT = Tuple[IntegralT, IntegralT, LiteralT]
_IndexOrSeries = Union[pd.Index, pd.Series]


def display_bounds(bounds: Sequence[int]) -> str:
    """Displays a flattened list of intervals."""
    out = ""
    for i in range(0, len(bounds), 2):
        if i != 0:
            out += ", "
        if bounds[i] == bounds[i + 1]:
            out += str(bounds[i])
        else:
            out += str(bounds[i]) + ".." + str(bounds[i + 1])
    return out


def short_name(model: cp_model_pb2.CpModelProto, i: int) -> str:
    """Returns a short name of an integer variable, or its negation."""
    if i < 0:
        return f"not({short_name(model, -i - 1)})"
    v = model.variables[i]
    if v.name:
        return v.name
    elif len(v.domain) == 2 and v.domain[0] == v.domain[1]:
        return str(v.domain[0])
    else:
        return f"[{display_bounds(v.domain)}]"


def short_expr_name(
    model: cp_model_pb2.CpModelProto, e: cp_model_pb2.LinearExpressionProto
) -> str:
    """Pretty-print LinearExpressionProto instances."""
    if not e.vars:
        return str(e.offset)
    if len(e.vars) == 1:
        var_name = short_name(model, e.vars[0])
        coeff = e.coeffs[0]
        result = ""
        if coeff == 1:
            result = var_name
        elif coeff == -1:
            result = f"-{var_name}"
        elif coeff != 0:
            result = f"{coeff} * {var_name}"
        if e.offset > 0:
            result = f"{result} + {e.offset}"
        elif e.offset < 0:
            result = f"{result} - {-e.offset}"
        return result
    # TODO(user): Support more than affine expressions.
    return str(e)


class IntVar(cmh.BaseIntVar):
    """An integer variable.

    An IntVar is an object that can take on any integer value within defined
    ranges. Variables appear in constraint like:

        x + y >= 5
        AllDifferent([x, y, z])

    Solving a model is equivalent to finding, for each variable, a single value
    from the set of initial values (called the initial domain), such that the
    model is feasible, or optimal if you provided an objective function.
    """

    def __init__(
        self,
        model: cp_model_pb2.CpModelProto,
        domain: Union[int, sorted_interval_list.Domain],
        is_boolean: bool,
        name: Optional[str],
    ) -> None:
        """See CpModel.new_int_var below."""
        self.__var: cp_model_pb2.IntegerVariableProto
        # Python do not support multiple __init__ methods.
        # This method is only called from the CpModel class.
        # We hack the parameter to support the two cases:
        # case 1:
        #     model is a CpModelProto, domain is a Domain, and name is a string.
        # case 2:
        #     model is a CpModelProto, domain is an index (int), and name is None.
        if isinstance(domain, IntegralTypes) and name is None:
            cmh.BaseIntVar.__init__(self, int(domain), is_boolean)
            self.__var = model.variables[domain]
        else:
            cmh.BaseIntVar.__init__(self, len(model.variables), is_boolean)
            self.__var = model.variables.add()
            self.__var.domain.extend(
                cast(sorted_interval_list.Domain, domain).flattened_intervals()
            )
            if name is not None:
                self.__var.name = name

    @property
    def proto(self) -> cp_model_pb2.IntegerVariableProto:
        """Returns the variable protobuf."""
        return self.__var

    def is_equal_to(self, other: Any) -> bool:
        """Returns true if self == other in the python sense."""
        if not isinstance(other, IntVar):
            return False
        return self.index == other.index

    def __str__(self) -> str:
        if not self.__var.name:
            if (
                len(self.__var.domain) == 2
                and self.__var.domain[0] == self.__var.domain[1]
            ):
                # Special case for constants.
                return str(self.__var.domain[0])
            elif self.is_boolean:
                return f"BooleanVar({self.__index})"
            else:
                return f"IntVar({self.__index})"
        else:
            return self.__var.name

    def __repr__(self) -> str:
        return f"{self}({display_bounds(self.__var.domain)})"

    @property
    def name(self) -> str:
        if not self.__var or not self.__var.name:
            return ""
        return self.__var.name

    # Pre PEP8 compatibility.
    # pylint: disable=invalid-name
    def Name(self) -> str:
        return self.name

    def Proto(self) -> cp_model_pb2.IntegerVariableProto:
        return self.proto

    # pylint: enable=invalid-name


class Constraint:
    """Base class for constraints.

    Constraints are built by the CpModel through the add<XXX> methods.
    Once created by the CpModel class, they are automatically added to the model.
    The purpose of this class is to allow specification of enforcement literals
    for this constraint.

        b = model.new_bool_var('b')
        x = model.new_int_var(0, 10, 'x')
        y = model.new_int_var(0, 10, 'y')

        model.add(x + 2 * y == 5).only_enforce_if(b.negated())
    """

    def __init__(
        self,
        cp_model: "CpModel",
    ) -> None:
        self.__index: int = len(cp_model.proto.constraints)
        self.__cp_model: "CpModel" = cp_model
        self.__constraint: cp_model_pb2.ConstraintProto = (
            cp_model.proto.constraints.add()
        )

    @overload
    def only_enforce_if(self, boolvar: Iterable[LiteralT]) -> "Constraint": ...

    @overload
    def only_enforce_if(self, *boolvar: LiteralT) -> "Constraint": ...

    def only_enforce_if(self, *boolvar) -> "Constraint":
        """Adds an enforcement literal to the constraint.

        This method adds one or more literals (that is, a boolean variable or its
        negation) as enforcement literals. The conjunction of all these literals
        determines whether the constraint is active or not. It acts as an
        implication, so if the conjunction is true, it implies that the constraint
        must be enforced. If it is false, then the constraint is ignored.

        BoolOr, BoolAnd, and linear constraints all support enforcement literals.

        Args:
          *boolvar: One or more Boolean literals.

        Returns:
          self.
        """
        for lit in expand_generator_or_tuple(boolvar):
            if (cmn.is_boolean(lit) and lit) or (
                isinstance(lit, IntegralTypes) and lit == 1
            ):
                # Always true. Do nothing.
                pass
            elif (cmn.is_boolean(lit) and not lit) or (
                isinstance(lit, IntegralTypes) and lit == 0
            ):
                self.__constraint.enforcement_literal.append(
                    self.__cp_model.new_constant(0).index
                )
            else:
                self.__constraint.enforcement_literal.append(
                    cast(cmh.Literal, lit).index
                )
        return self

    def with_name(self, name: str) -> "Constraint":
        """Sets the name of the constraint."""
        if name:
            self.__constraint.name = name
        else:
            self.__constraint.ClearField("name")
        return self

    @property
    def name(self) -> str:
        """Returns the name of the constraint."""
        if not self.__constraint or not self.__constraint.name:
            return ""
        return self.__constraint.name

    @property
    def index(self) -> int:
        """Returns the index of the constraint in the model."""
        return self.__index

    @property
    def proto(self) -> cp_model_pb2.ConstraintProto:
        """Returns the constraint protobuf."""
        return self.__constraint

    # Pre PEP8 compatibility.
    # pylint: disable=invalid-name
    OnlyEnforceIf = only_enforce_if
    WithName = with_name

    def Name(self) -> str:
        return self.name

    def Index(self) -> int:
        return self.index

    def Proto(self) -> cp_model_pb2.ConstraintProto:
        return self.proto

    # pylint: enable=invalid-name


class VariableList:
    """Stores all integer variables of the model."""

    def __init__(self) -> None:
        self.__var_list: list[IntVar] = []

    def append(self, var: IntVar) -> None:
        assert var.index == len(self.__var_list)
        self.__var_list.append(var)

    def get(self, index: int) -> IntVar:
        if index < 0 or index >= len(self.__var_list):
            raise ValueError("Index out of bounds.")
        return self.__var_list[index]

    def rebuild_expr(
        self,
        proto: cp_model_pb2.LinearExpressionProto,
    ) -> LinearExprT:
        """Recreate a LinearExpr from a LinearExpressionProto."""
        num_elements = len(proto.vars)
        if num_elements == 0:
            return proto.offset
        elif num_elements == 1:
            var = self.get(proto.vars[0])
            return LinearExpr.affine(
                var, proto.coeffs[0], proto.offset
            )  # pytype: disable=bad-return-type
        else:
            variables = []
            for var_index in range(len(proto.vars)):
                var = self.get(var_index)
                variables.append(var)
            if proto.offset != 0:
                coeffs = []
                coeffs.extend(proto.coeffs)
                coeffs.append(1)
                variables.append(proto.offset)
                return LinearExpr.weighted_sum(variables, coeffs)
            else:
                return LinearExpr.weighted_sum(variables, proto.coeffs)


class IntervalVar:
    """Represents an Interval variable.

    An interval variable is both a constraint and a variable. It is defined by
    three integer variables: start, size, and end.

    It is a constraint because, internally, it enforces that start + size == end.

    It is also a variable as it can appear in specific scheduling constraints:
    NoOverlap, NoOverlap2D, Cumulative.

    Optionally, an enforcement literal can be added to this constraint, in which
    case these scheduling constraints will ignore interval variables with
    enforcement literals assigned to false. Conversely, these constraints will
    also set these enforcement literals to false if they cannot fit these
    intervals into the schedule.

    Raises:
      ValueError: if start, size, end are not defined, or have the wrong type.
    """

    def __init__(
        self,
        model: cp_model_pb2.CpModelProto,
        var_list: VariableList,
        start: Union[cp_model_pb2.LinearExpressionProto, int],
        size: Optional[cp_model_pb2.LinearExpressionProto],
        end: Optional[cp_model_pb2.LinearExpressionProto],
        is_present_index: Optional[int],
        name: Optional[str],
    ) -> None:
        self.__model: cp_model_pb2.CpModelProto = model
        self.__var_list: VariableList = var_list
        self.__index: int
        self.__ct: cp_model_pb2.ConstraintProto
        # As with the IntVar::__init__ method, we hack the __init__ method to
        # support two use cases:
        #   case 1: called when creating a new interval variable.
        #      {start|size|end} are linear expressions, is_present_index is either
        #      None or the index of a Boolean literal. name is a string
        #   case 2: called when querying an existing interval variable.
        #      start_index is an int, all parameters after are None.
        if isinstance(start, int):
            if size is not None:
                raise ValueError("size should be None")
            if end is not None:
                raise ValueError("end should be None")
            if is_present_index is not None:
                raise ValueError("is_present_index should be None")
            self.__index = cast(int, start)
            self.__ct = model.constraints[self.__index]
        else:
            self.__index = len(model.constraints)
            self.__ct = self.__model.constraints.add()
            if start is None:
                raise TypeError("start is not defined")
            self.__ct.interval.start.CopyFrom(start)
            if size is None:
                raise TypeError("size is not defined")
            self.__ct.interval.size.CopyFrom(size)
            if end is None:
                raise TypeError("end is not defined")
            self.__ct.interval.end.CopyFrom(end)
            if is_present_index is not None:
                self.__ct.enforcement_literal.append(is_present_index)
            if name:
                self.__ct.name = name

    @property
    def index(self) -> int:
        """Returns the index of the interval constraint in the model."""
        return self.__index

    @property
    def proto(self) -> cp_model_pb2.IntervalConstraintProto:
        """Returns the interval protobuf."""
        return self.__ct.interval

    def __str__(self):
        return self.__ct.name

    def __repr__(self):
        interval = self.__ct.interval
        if self.__ct.enforcement_literal:
            return (
                f"{self.__ct.name}(start ="
                f" {short_expr_name(self.__model, interval.start)}, size ="
                f" {short_expr_name(self.__model, interval.size)}, end ="
                f" {short_expr_name(self.__model, interval.end)}, is_present ="
                f" {short_name(self.__model, self.__ct.enforcement_literal[0])})"
            )
        else:
            return (
                f"{self.__ct.name}(start ="
                f" {short_expr_name(self.__model, interval.start)}, size ="
                f" {short_expr_name(self.__model, interval.size)}, end ="
                f" {short_expr_name(self.__model, interval.end)})"
            )

    @property
    def name(self) -> str:
        if not self.__ct or not self.__ct.name:
            return ""
        return self.__ct.name

    def start_expr(self) -> LinearExprT:
        return self.__var_list.rebuild_expr(self.__ct.interval.start)

    def size_expr(self) -> LinearExprT:
        return self.__var_list.rebuild_expr(self.__ct.interval.size)

    def end_expr(self) -> LinearExprT:
        return self.__var_list.rebuild_expr(self.__ct.interval.end)

    # Pre PEP8 compatibility.
    # pylint: disable=invalid-name
    def Name(self) -> str:
        return self.name

    def Index(self) -> int:
        return self.index

    def Proto(self) -> cp_model_pb2.IntervalConstraintProto:
        return self.proto

    StartExpr = start_expr
    SizeExpr = size_expr
    EndExpr = end_expr

    # pylint: enable=invalid-name


def object_is_a_true_literal(literal: LiteralT) -> bool:
    """Checks if literal is either True, or a Boolean literals fixed to True."""
    if isinstance(literal, IntVar):
        proto = literal.proto
        return len(proto.domain) == 2 and proto.domain[0] == 1 and proto.domain[1] == 1
    if isinstance(literal, cmh.NotBooleanVariable):
        proto = literal.negated().proto
        return len(proto.domain) == 2 and proto.domain[0] == 0 and proto.domain[1] == 0
    if isinstance(literal, IntegralTypes):
        return int(literal) == 1
    return False


def object_is_a_false_literal(literal: LiteralT) -> bool:
    """Checks if literal is either False, or a Boolean literals fixed to False."""
    if isinstance(literal, IntVar):
        proto = literal.proto
        return len(proto.domain) == 2 and proto.domain[0] == 0 and proto.domain[1] == 0
    if isinstance(literal, cmh.NotBooleanVariable):
        proto = literal.negated().proto
        return len(proto.domain) == 2 and proto.domain[0] == 1 and proto.domain[1] == 1
    if isinstance(literal, IntegralTypes):
        return int(literal) == 0
    return False


class CpModel:
    """Methods for building a CP model.

    Methods beginning with:

    * ```New``` create integer, boolean, or interval variables.
    * ```add``` create new constraints and add them to the model.
    """

    def __init__(self) -> None:
        self.__model: cp_model_pb2.CpModelProto = cp_model_pb2.CpModelProto()
        self.__constant_map: Dict[IntegralT, int] = {}
        self.__var_list: VariableList = VariableList()

    # Naming.
    @property
    def name(self) -> str:
        """Returns the name of the model."""
        if not self.__model or not self.__model.name:
            return ""
        return self.__model.name

    @name.setter
    def name(self, name: str):
        """Sets the name of the model."""
        self.__model.name = name

    # Integer variable.

    def _append_int_var(self, var: IntVar) -> IntVar:
        """Appends an integer variable to the list of variables."""
        self.__var_list.append(var)
        return var

    def _get_int_var(self, index: int) -> IntVar:
        return self.__var_list.get(index)

    def rebuild_from_linear_expression_proto(
        self,
        proto: cp_model_pb2.LinearExpressionProto,
    ) -> LinearExpr:
        return self.__var_list.rebuild_expr(proto)

    def new_int_var(self, lb: IntegralT, ub: IntegralT, name: str) -> IntVar:
        """Create an integer variable with domain [lb, ub].

        The CP-SAT solver is limited to integer variables. If you have fractional
        values, scale them up so that they become integers; if you have strings,
        encode them as integers.

        Args:
          lb: Lower bound for the variable.
          ub: Upper bound for the variable.
          name: The name of the variable.

        Returns:
          a variable whose domain is [lb, ub].
        """
        domain_is_boolean = lb >= 0 and ub <= 1
        return self._append_int_var(
            IntVar(
                self.__model,
                sorted_interval_list.Domain(lb, ub),
                domain_is_boolean,
                name,
            )
        )

    def new_int_var_from_domain(
        self, domain: sorted_interval_list.Domain, name: str
    ) -> IntVar:
        """Create an integer variable from a domain.

        A domain is a set of integers specified by a collection of intervals.
        For example, `model.new_int_var_from_domain(cp_model.
             Domain.from_intervals([[1, 2], [4, 6]]), 'x')`

        Args:
          domain: An instance of the Domain class.
          name: The name of the variable.

        Returns:
            a variable whose domain is the given domain.
        """
        domain_is_boolean = domain.min() >= 0 and domain.max() <= 1
        return self._append_int_var(
            IntVar(self.__model, domain, domain_is_boolean, name)
        )

    def new_bool_var(self, name: str) -> IntVar:
        """Creates a 0-1 variable with the given name."""
        return self._append_int_var(
            IntVar(self.__model, sorted_interval_list.Domain(0, 1), True, name)
        )

    def new_constant(self, value: IntegralT) -> IntVar:
        """Declares a constant integer."""
        index: int = self.get_or_make_index_from_constant(value)
        return self._get_int_var(index)

    def new_int_var_series(
        self,
        name: str,
        index: pd.Index,
        lower_bounds: Union[IntegralT, pd.Series],
        upper_bounds: Union[IntegralT, pd.Series],
    ) -> pd.Series:
        """Creates a series of (scalar-valued) variables with the given name.

        Args:
          name (str): Required. The name of the variable set.
          index (pd.Index): Required. The index to use for the variable set.
          lower_bounds (Union[int, pd.Series]): A lower bound for variables in the
            set. If a `pd.Series` is passed in, it will be based on the
            corresponding values of the pd.Series.
          upper_bounds (Union[int, pd.Series]): An upper bound for variables in the
            set. If a `pd.Series` is passed in, it will be based on the
            corresponding values of the pd.Series.

        Returns:
          pd.Series: The variable set indexed by its corresponding dimensions.

        Raises:
          TypeError: if the `index` is invalid (e.g. a `DataFrame`).
          ValueError: if the `name` is not a valid identifier or already exists.
          ValueError: if the `lowerbound` is greater than the `upperbound`.
          ValueError: if the index of `lower_bound`, or `upper_bound` does not match
          the input index.
        """
        if not isinstance(index, pd.Index):
            raise TypeError("Non-index object is used as index")
        if not name.isidentifier():
            raise ValueError(f"name={name!r} is not a valid identifier")
        if (
            isinstance(lower_bounds, IntegralTypes)
            and isinstance(upper_bounds, IntegralTypes)
            and lower_bounds > upper_bounds
        ):
            raise ValueError(
                f"lower_bound={lower_bounds} is greater than"
                f" upper_bound={upper_bounds} for variable set={name}"
            )

        lower_bounds = _convert_to_integral_series_and_validate_index(
            lower_bounds, index
        )
        upper_bounds = _convert_to_integral_series_and_validate_index(
            upper_bounds, index
        )
        return pd.Series(
            index=index,
            data=[
                # pylint: disable=g-complex-comprehension
                self._append_int_var(
                    IntVar(
                        model=self.__model,
                        name=f"{name}[{i}]",
                        domain=sorted_interval_list.Domain(
                            lower_bounds[i], upper_bounds[i]
                        ),
                        is_boolean=lower_bounds[i] >= 0 and upper_bounds[i] <= 1,
                    )
                )
                for i in index
            ],
        )

    def new_bool_var_series(
        self,
        name: str,
        index: pd.Index,
    ) -> pd.Series:
        """Creates a series of (scalar-valued) variables with the given name.

        Args:
          name (str): Required. The name of the variable set.
          index (pd.Index): Required. The index to use for the variable set.

        Returns:
          pd.Series: The variable set indexed by its corresponding dimensions.

        Raises:
          TypeError: if the `index` is invalid (e.g. a `DataFrame`).
          ValueError: if the `name` is not a valid identifier or already exists.
        """
        if not isinstance(index, pd.Index):
            raise TypeError("Non-index object is used as index")
        if not name.isidentifier():
            raise ValueError(f"name={name!r} is not a valid identifier")
        return pd.Series(
            index=index,
            data=[
                # pylint: disable=g-complex-comprehension
                self._append_int_var(
                    IntVar(
                        model=self.__model,
                        name=f"{name}[{i}]",
                        domain=sorted_interval_list.Domain(0, 1),
                        is_boolean=True,
                    )
                )
                for i in index
            ],
        )

    # Linear constraints.

    def add_linear_constraint(
        self, linear_expr: LinearExprT, lb: IntegralT, ub: IntegralT
    ) -> Constraint:
        """Adds the constraint: `lb <= linear_expr <= ub`."""
        return self.add_linear_expression_in_domain(
            linear_expr, sorted_interval_list.Domain(lb, ub)
        )

    def add_linear_expression_in_domain(
        self,
        linear_expr: LinearExprT,
        domain: sorted_interval_list.Domain,
    ) -> Constraint:
        """Adds the constraint: `linear_expr` in `domain`."""
        if isinstance(linear_expr, LinearExpr):
            ble = BoundedLinearExpression(linear_expr, domain)
            if not ble.ok:
                raise TypeError(
                    "Cannot add a linear expression containing floating point"
                    f" coefficients or constants: {type(linear_expr).__name__!r}"
                )
            return self.add(ble)
        if isinstance(linear_expr, IntegralTypes):
            if not domain.contains(int(linear_expr)):
                return self.add_bool_or([])  # Evaluate to false.
            else:
                return self.add_bool_and([])  # Evaluate to true.
        raise TypeError(
            "not supported:"
            f" CpModel.add_linear_expression_in_domain({type(linear_expr).__name__!r})"
        )

    def add(self, ct: Union[BoundedLinearExpression, bool, np.bool_]) -> Constraint:
        """Adds a `BoundedLinearExpression` to the model.

        Args:
          ct: A [`BoundedLinearExpression`](#boundedlinearexpression).

        Returns:
          An instance of the `Constraint` class.

        Raises:
          TypeError: If the `ct` is not a `BoundedLinearExpression` or a Boolean.
        """
        if isinstance(ct, BoundedLinearExpression):
            result = Constraint(self)
            model_ct = self.__model.constraints[result.index]
            for var in ct.vars:
                model_ct.linear.vars.append(var.index)
            model_ct.linear.coeffs.extend(ct.coeffs)
            model_ct.linear.domain.extend(
                [
                    cmn.capped_subtraction(x, ct.offset)
                    for x in ct.bounds.flattened_intervals()
                ]
            )
            return result
        if ct and cmn.is_boolean(ct):
            return self.add_bool_or([True])
        if not ct and cmn.is_boolean(ct):
            return self.add_bool_or([])  # Evaluate to false.
        raise TypeError(f"not supported: CpModel.add({type(ct).__name__!r})")

    # General Integer Constraints.

    @overload
    def add_all_different(self, expressions: Iterable[LinearExprT]) -> Constraint: ...

    @overload
    def add_all_different(self, *expressions: LinearExprT) -> Constraint: ...

    def add_all_different(self, *expressions):
        """Adds AllDifferent(expressions).

        This constraint forces all expressions to have different values.

        Args:
          *expressions: simple expressions of the form a * var + constant.

        Returns:
          An instance of the `Constraint` class.
        """
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        expanded = expand_generator_or_tuple(expressions)
        model_ct.all_diff.exprs.extend(
            self.parse_linear_expression(x) for x in expanded
        )
        return ct

    def add_element(
        self,
        index: LinearExprT,
        expressions: Sequence[LinearExprT],
        target: LinearExprT,
    ) -> Constraint:
        """Adds the element constraint: `expressions[index] == target`.

        Args:
          index: The index of the selected expression in the array. It must be an
            affine expression (a * var + b).
          expressions: A list of affine expressions.
          target: The expression constrained to be equal to the selected expression.
            It must be an affine expression (a * var + b).

        Returns:
          An instance of the `Constraint` class.
        """

        if not expressions:
            raise ValueError("add_element expects a non-empty expressions array")

        if isinstance(index, IntegralTypes):
            expression: LinearExprT = list(expressions)[int(index)]
            return self.add(expression == target)

        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.element.linear_index.CopyFrom(self.parse_linear_expression(index))
        model_ct.element.exprs.extend(
            [self.parse_linear_expression(e) for e in expressions]
        )
        model_ct.element.linear_target.CopyFrom(self.parse_linear_expression(target))
        return ct

    def add_circuit(self, arcs: Sequence[ArcT]) -> Constraint:
        """Adds Circuit(arcs).

        Adds a circuit constraint from a sparse list of arcs that encode the graph.

        A circuit is a unique Hamiltonian cycle in a subgraph of the total
        graph. In case a node 'i' is not in the cycle, then there must be a
        loop arc 'i -> i' associated with a true literal. Otherwise
        this constraint will fail.

        Args:
          arcs: a list of arcs. An arc is a tuple (source_node, destination_node,
            literal). The arc is selected in the circuit if the literal is true.
            Both source_node and destination_node must be integers between 0 and the
            number of nodes - 1.

        Returns:
          An instance of the `Constraint` class.

        Raises:
          ValueError: If the list of arcs is empty.
        """
        if not arcs:
            raise ValueError("add_circuit expects a non-empty array of arcs")
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        for arc in arcs:
            model_ct.circuit.tails.append(arc[0])
            model_ct.circuit.heads.append(arc[1])
            model_ct.circuit.literals.append(self.get_or_make_boolean_index(arc[2]))
        return ct

    def add_multiple_circuit(self, arcs: Sequence[ArcT]) -> Constraint:
        """Adds a multiple circuit constraint, aka the 'VRP' constraint.

        The direct graph where arc #i (from tails[i] to head[i]) is present iff
        literals[i] is true must satisfy this set of properties:
        - #incoming arcs == 1 except for node 0.
        - #outgoing arcs == 1 except for node 0.
        - for node zero, #incoming arcs == #outgoing arcs.
        - There are no duplicate arcs.
        - Self-arcs are allowed except for node 0.
        - There is no cycle in this graph, except through node 0.

        Args:
          arcs: a list of arcs. An arc is a tuple (source_node, destination_node,
            literal). The arc is selected in the circuit if the literal is true.
            Both source_node and destination_node must be integers between 0 and the
            number of nodes - 1.

        Returns:
          An instance of the `Constraint` class.

        Raises:
          ValueError: If the list of arcs is empty.
        """
        if not arcs:
            raise ValueError("add_multiple_circuit expects a non-empty array of arcs")
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        for arc in arcs:
            model_ct.routes.tails.append(arc[0])
            model_ct.routes.heads.append(arc[1])
            model_ct.routes.literals.append(self.get_or_make_boolean_index(arc[2]))
        return ct

    def add_allowed_assignments(
        self,
        expressions: Sequence[LinearExprT],
        tuples_list: Iterable[Sequence[IntegralT]],
    ) -> Constraint:
        """Adds AllowedAssignments(expressions, tuples_list).

        An AllowedAssignments constraint is a constraint on an array of affine
        expressions, which requires that when all expressions are assigned values,
        the
        resulting array equals one of the  tuples in `tuple_list`.

        Args:
          expressions: A list of affine expressions (a * var + b).
          tuples_list: A list of admissible tuples. Each tuple must have the same
            length as the expressions, and the ith value of a tuple corresponds to
            the ith expression.

        Returns:
          An instance of the `Constraint` class.

        Raises:
          TypeError: If a tuple does not have the same size as the list of
              expressions.
          ValueError: If the array of expressions is empty.
        """

        if not expressions:
            raise ValueError(
                "add_allowed_assignments expects a non-empty expressions array"
            )

        ct: Constraint = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.table.exprs.extend(
            [self.parse_linear_expression(e) for e in expressions]
        )
        arity: int = len(expressions)
        for one_tuple in tuples_list:
            if len(one_tuple) != arity:
                raise TypeError(f"Tuple {one_tuple!r} has the wrong arity")

        # duck-typing (no explicit type checks here)
        try:
            for one_tuple in tuples_list:
                model_ct.table.values.extend(one_tuple)
        except ValueError as ex:
            raise TypeError(
                "add_xxx_assignment: Not an integer or does not fit in an int64_t:"
                f" {type(ex.args).__name__!r}"
            ) from ex

        return ct

    def add_forbidden_assignments(
        self,
        expressions: Sequence[LinearExprT],
        tuples_list: Iterable[Sequence[IntegralT]],
    ) -> Constraint:
        """Adds add_forbidden_assignments(expressions, [tuples_list]).

        A ForbiddenAssignments constraint is a constraint on an array of affine
        expressions where the list of impossible combinations is provided in the
        tuples list.

        Args:
          expressions: A list of affine expressions (a * var + b).
          tuples_list: A list of forbidden tuples. Each tuple must have the same
            length as the expressions, and the *i*th value of a tuple corresponds to
            the *i*th expression.

        Returns:
          An instance of the `Constraint` class.

        Raises:
          TypeError: If a tuple does not have the same size as the list of
                     expressions.
          ValueError: If the array of expressions is empty.
        """

        if not expressions:
            raise ValueError(
                "add_forbidden_assignments expects a non-empty expressions array"
            )

        index: int = len(self.__model.constraints)
        ct: Constraint = self.add_allowed_assignments(expressions, tuples_list)
        self.__model.constraints[index].table.negated = True
        return ct

    def add_automaton(
        self,
        transition_expressions: Sequence[LinearExprT],
        starting_state: IntegralT,
        final_states: Sequence[IntegralT],
        transition_triples: Sequence[Tuple[IntegralT, IntegralT, IntegralT]],
    ) -> Constraint:
        """Adds an automaton constraint.

        An automaton constraint takes a list of affine expressions (a * var + b) (of
        size *n*), an initial state, a set of final states, and a set of
        transitions. A transition is a triplet (*tail*, *transition*, *head*), where
        *tail* and *head* are states, and *transition* is the label of an arc from
        *head* to *tail*, corresponding to the value of one expression in the list
        of
        expressions.

        This automaton will be unrolled into a flow with *n* + 1 phases. Each phase
        contains the possible states of the automaton. The first state contains the
        initial state. The last phase contains the final states.

        Between two consecutive phases *i* and *i* + 1, the automaton creates a set
        of arcs. For each transition (*tail*, *transition*, *head*), it will add
        an arc from the state *tail* of phase *i* and the state *head* of phase
        *i* + 1. This arc is labeled by the value *transition* of the expression
        `expressions[i]`. That is, this arc can only be selected if `expressions[i]`
        is assigned the value *transition*.

        A feasible solution of this constraint is an assignment of expressions such
        that, starting from the initial state in phase 0, there is a path labeled by
        the values of the expressions that ends in one of the final states in the
        final phase.

        Args:
          transition_expressions: A non-empty list of affine expressions (a * var +
            b) whose values correspond to the labels of the arcs traversed by the
            automaton.
          starting_state: The initial state of the automaton.
          final_states: A non-empty list of admissible final states.
          transition_triples: A list of transitions for the automaton, in the
            following format (current_state, variable_value, next_state).

        Returns:
          An instance of the `Constraint` class.

        Raises:
          ValueError: if `transition_expressions`, `final_states`, or
            `transition_triples` are empty.
        """

        if not transition_expressions:
            raise ValueError(
                "add_automaton expects a non-empty transition_expressions array"
            )
        if not final_states:
            raise ValueError("add_automaton expects some final states")

        if not transition_triples:
            raise ValueError("add_automaton expects some transition triples")

        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.automaton.exprs.extend(
            [self.parse_linear_expression(e) for e in transition_expressions]
        )
        model_ct.automaton.starting_state = starting_state
        for v in final_states:
            model_ct.automaton.final_states.append(v)
        for t in transition_triples:
            if len(t) != 3:
                raise TypeError(f"Tuple {t!r} has the wrong arity (!= 3)")
            model_ct.automaton.transition_tail.append(t[0])
            model_ct.automaton.transition_label.append(t[1])
            model_ct.automaton.transition_head.append(t[2])
        return ct

    def add_inverse(
        self,
        variables: Sequence[VariableT],
        inverse_variables: Sequence[VariableT],
    ) -> Constraint:
        """Adds Inverse(variables, inverse_variables).

        An inverse constraint enforces that if `variables[i]` is assigned a value
        `j`, then `inverse_variables[j]` is assigned a value `i`. And vice versa.

        Args:
          variables: An array of integer variables.
          inverse_variables: An array of integer variables.

        Returns:
          An instance of the `Constraint` class.

        Raises:
          TypeError: if variables and inverse_variables have different lengths, or
              if they are empty.
        """

        if not variables or not inverse_variables:
            raise TypeError("The Inverse constraint does not accept empty arrays")
        if len(variables) != len(inverse_variables):
            raise TypeError(
                "In the inverse constraint, the two array variables and"
                " inverse_variables must have the same length."
            )
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.inverse.f_direct.extend([self.get_or_make_index(x) for x in variables])
        model_ct.inverse.f_inverse.extend(
            [self.get_or_make_index(x) for x in inverse_variables]
        )
        return ct

    def add_reservoir_constraint(
        self,
        times: Iterable[LinearExprT],
        level_changes: Iterable[LinearExprT],
        min_level: int,
        max_level: int,
    ) -> Constraint:
        """Adds Reservoir(times, level_changes, min_level, max_level).

        Maintains a reservoir level within bounds. The water level starts at 0, and
        at any time, it must be between min_level and max_level.

        If the affine expression `times[i]` is assigned a value t, then the current
        level changes by `level_changes[i]`, which is constant, at time t.

         Note that min level must be <= 0, and the max level must be >= 0. Please
         use fixed level_changes to simulate initial state.

         Therefore, at any time:
             sum(level_changes[i] if times[i] <= t) in [min_level, max_level]

        Args:
          times: A list of 1-var affine expressions (a * x + b) which specify the
            time of the filling or emptying the reservoir.
          level_changes: A list of integer values that specifies the amount of the
            emptying or filling. Currently, variable demands are not supported.
          min_level: At any time, the level of the reservoir must be greater or
            equal than the min level.
          max_level: At any time, the level of the reservoir must be less or equal
            than the max level.

        Returns:
          An instance of the `Constraint` class.

        Raises:
          ValueError: if max_level < min_level.

          ValueError: if max_level < 0.

          ValueError: if min_level > 0
        """

        if max_level < min_level:
            raise ValueError("Reservoir constraint must have a max_level >= min_level")

        if max_level < 0:
            raise ValueError("Reservoir constraint must have a max_level >= 0")

        if min_level > 0:
            raise ValueError("Reservoir constraint must have a min_level <= 0")

        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.reservoir.time_exprs.extend(
            [self.parse_linear_expression(x) for x in times]
        )
        model_ct.reservoir.level_changes.extend(
            [self.parse_linear_expression(x) for x in level_changes]
        )
        model_ct.reservoir.min_level = min_level
        model_ct.reservoir.max_level = max_level
        return ct

    def add_reservoir_constraint_with_active(
        self,
        times: Iterable[LinearExprT],
        level_changes: Iterable[LinearExprT],
        actives: Iterable[LiteralT],
        min_level: int,
        max_level: int,
    ) -> Constraint:
        """Adds Reservoir(times, level_changes, actives, min_level, max_level).

        Maintains a reservoir level within bounds. The water level starts at 0, and
        at any time, it must be between min_level and max_level.

        If the variable `times[i]` is assigned a value t, and `actives[i]` is
        `True`, then the current level changes by `level_changes[i]`, which is
        constant,
        at time t.

         Note that min level must be <= 0, and the max level must be >= 0. Please
         use fixed level_changes to simulate initial state.

         Therefore, at any time:
             sum(level_changes[i] * actives[i] if times[i] <= t) in [min_level,
             max_level]


        The array of boolean variables 'actives', if defined, indicates which
        actions are actually performed.

        Args:
          times: A list of 1-var affine expressions (a * x + b) which specify the
            time of the filling or emptying the reservoir.
          level_changes: A list of integer values that specifies the amount of the
            emptying or filling. Currently, variable demands are not supported.
          actives: a list of boolean variables. They indicates if the
            emptying/refilling events actually take place.
          min_level: At any time, the level of the reservoir must be greater or
            equal than the min level.
          max_level: At any time, the level of the reservoir must be less or equal
            than the max level.

        Returns:
          An instance of the `Constraint` class.

        Raises:
          ValueError: if max_level < min_level.

          ValueError: if max_level < 0.

          ValueError: if min_level > 0
        """

        if max_level < min_level:
            raise ValueError("Reservoir constraint must have a max_level >= min_level")

        if max_level < 0:
            raise ValueError("Reservoir constraint must have a max_level >= 0")

        if min_level > 0:
            raise ValueError("Reservoir constraint must have a min_level <= 0")

        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.reservoir.time_exprs.extend(
            [self.parse_linear_expression(x) for x in times]
        )
        model_ct.reservoir.level_changes.extend(
            [self.parse_linear_expression(x) for x in level_changes]
        )
        model_ct.reservoir.active_literals.extend(
            [self.get_or_make_boolean_index(x) for x in actives]
        )
        model_ct.reservoir.min_level = min_level
        model_ct.reservoir.max_level = max_level
        return ct

    def add_map_domain(
        self, var: IntVar, bool_var_array: Iterable[IntVar], offset: IntegralT = 0
    ):
        """Adds `var == i + offset <=> bool_var_array[i] == true for all i`."""

        for i, bool_var in enumerate(bool_var_array):
            b_index = bool_var.index
            var_index = var.index
            model_ct = self.__model.constraints.add()
            model_ct.linear.vars.append(var_index)
            model_ct.linear.coeffs.append(1)
            offset_as_int = int(offset)
            model_ct.linear.domain.extend([offset_as_int + i, offset_as_int + i])
            model_ct.enforcement_literal.append(b_index)

            model_ct = self.__model.constraints.add()
            model_ct.linear.vars.append(var_index)
            model_ct.linear.coeffs.append(1)
            model_ct.enforcement_literal.append(-b_index - 1)
            if offset + i - 1 >= INT_MIN:
                model_ct.linear.domain.extend([INT_MIN, offset_as_int + i - 1])
            if offset + i + 1 <= INT_MAX:
                model_ct.linear.domain.extend([offset_as_int + i + 1, INT_MAX])

    def add_implication(self, a: LiteralT, b: LiteralT) -> Constraint:
        """Adds `a => b` (`a` implies `b`)."""
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.bool_or.literals.append(self.get_or_make_boolean_index(b))
        model_ct.enforcement_literal.append(self.get_or_make_boolean_index(a))
        return ct

    @overload
    def add_bool_or(self, literals: Iterable[LiteralT]) -> Constraint: ...

    @overload
    def add_bool_or(self, *literals: LiteralT) -> Constraint: ...

    def add_bool_or(self, *literals):
        """Adds `Or(literals) == true`: sum(literals) >= 1."""
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.bool_or.literals.extend(
            [
                self.get_or_make_boolean_index(x)
                for x in expand_generator_or_tuple(literals)
            ]
        )
        return ct

    @overload
    def add_at_least_one(self, literals: Iterable[LiteralT]) -> Constraint: ...

    @overload
    def add_at_least_one(self, *literals: LiteralT) -> Constraint: ...

    def add_at_least_one(self, *literals):
        """Same as `add_bool_or`: `sum(literals) >= 1`."""
        return self.add_bool_or(*literals)

    @overload
    def add_at_most_one(self, literals: Iterable[LiteralT]) -> Constraint: ...

    @overload
    def add_at_most_one(self, *literals: LiteralT) -> Constraint: ...

    def add_at_most_one(self, *literals):
        """Adds `AtMostOne(literals)`: `sum(literals) <= 1`."""
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.at_most_one.literals.extend(
            [
                self.get_or_make_boolean_index(x)
                for x in expand_generator_or_tuple(literals)
            ]
        )
        return ct

    @overload
    def add_exactly_one(self, literals: Iterable[LiteralT]) -> Constraint: ...

    @overload
    def add_exactly_one(self, *literals: LiteralT) -> Constraint: ...

    def add_exactly_one(self, *literals):
        """Adds `ExactlyOne(literals)`: `sum(literals) == 1`."""
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.exactly_one.literals.extend(
            [
                self.get_or_make_boolean_index(x)
                for x in expand_generator_or_tuple(literals)
            ]
        )
        return ct

    @overload
    def add_bool_and(self, literals: Iterable[LiteralT]) -> Constraint: ...

    @overload
    def add_bool_and(self, *literals: LiteralT) -> Constraint: ...

    def add_bool_and(self, *literals):
        """Adds `And(literals) == true`."""
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.bool_and.literals.extend(
            [
                self.get_or_make_boolean_index(x)
                for x in expand_generator_or_tuple(literals)
            ]
        )
        return ct

    @overload
    def add_bool_xor(self, literals: Iterable[LiteralT]) -> Constraint: ...

    @overload
    def add_bool_xor(self, *literals: LiteralT) -> Constraint: ...

    def add_bool_xor(self, *literals):
        """Adds `XOr(literals) == true`.

        In contrast to add_bool_or and add_bool_and, it does not support
            .only_enforce_if().

        Args:
          *literals: the list of literals in the constraint.

        Returns:
          An `Constraint` object.
        """
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.bool_xor.literals.extend(
            [
                self.get_or_make_boolean_index(x)
                for x in expand_generator_or_tuple(literals)
            ]
        )
        return ct

    def add_min_equality(
        self, target: LinearExprT, exprs: Iterable[LinearExprT]
    ) -> Constraint:
        """Adds `target == Min(exprs)`."""
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.lin_max.exprs.extend(
            [self.parse_linear_expression(x, True) for x in exprs]
        )
        model_ct.lin_max.target.CopyFrom(self.parse_linear_expression(target, True))
        return ct

    def add_max_equality(
        self, target: LinearExprT, exprs: Iterable[LinearExprT]
    ) -> Constraint:
        """Adds `target == Max(exprs)`."""
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.lin_max.exprs.extend([self.parse_linear_expression(x) for x in exprs])
        model_ct.lin_max.target.CopyFrom(self.parse_linear_expression(target))
        return ct

    def add_division_equality(
        self, target: LinearExprT, num: LinearExprT, denom: LinearExprT
    ) -> Constraint:
        """Adds `target == num // denom` (integer division rounded towards 0)."""
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.int_div.exprs.append(self.parse_linear_expression(num))
        model_ct.int_div.exprs.append(self.parse_linear_expression(denom))
        model_ct.int_div.target.CopyFrom(self.parse_linear_expression(target))
        return ct

    def add_abs_equality(self, target: LinearExprT, expr: LinearExprT) -> Constraint:
        """Adds `target == Abs(expr)`."""
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.lin_max.exprs.append(self.parse_linear_expression(expr))
        model_ct.lin_max.exprs.append(self.parse_linear_expression(expr, True))
        model_ct.lin_max.target.CopyFrom(self.parse_linear_expression(target))
        return ct

    def add_modulo_equality(
        self, target: LinearExprT, expr: LinearExprT, mod: LinearExprT
    ) -> Constraint:
        """Adds `target = expr % mod`.

        It uses the C convention, that is the result is the remainder of the
        integral division rounded towards 0.

            For example:
            * 10 % 3 = 1
            * -10 % 3 = -1
            * 10 % -3 = 1
            * -10 % -3 = -1

        Args:
          target: the target expression.
          expr: the expression to compute the modulo of.
          mod: the modulus expression.

        Returns:
          A `Constraint` object.
        """
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.int_mod.exprs.append(self.parse_linear_expression(expr))
        model_ct.int_mod.exprs.append(self.parse_linear_expression(mod))
        model_ct.int_mod.target.CopyFrom(self.parse_linear_expression(target))
        return ct

    def add_multiplication_equality(
        self,
        target: LinearExprT,
        *expressions: Union[Iterable[LinearExprT], LinearExprT],
    ) -> Constraint:
        """Adds `target == expressions[0] * .. * expressions[n]`."""
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.int_prod.exprs.extend(
            [
                self.parse_linear_expression(expr)
                for expr in expand_generator_or_tuple(expressions)
            ]
        )
        model_ct.int_prod.target.CopyFrom(self.parse_linear_expression(target))
        return ct

    # Scheduling support

    def new_interval_var(
        self, start: LinearExprT, size: LinearExprT, end: LinearExprT, name: str
    ) -> IntervalVar:
        """Creates an interval variable from start, size, and end.

        An interval variable is a constraint, that is itself used in other
        constraints like NoOverlap.

        Internally, it ensures that `start + size == end`.

        Args:
          start: The start of the interval. It must be of the form a * var + b.
          size: The size of the interval. It must be of the form a * var + b.
          end: The end of the interval. It must be of the form a * var + b.
          name: The name of the interval variable.

        Returns:
          An `IntervalVar` object.
        """

        start_expr = self.parse_linear_expression(start)
        size_expr = self.parse_linear_expression(size)
        end_expr = self.parse_linear_expression(end)
        if len(start_expr.vars) > 1:
            raise TypeError(
                "cp_model.new_interval_var: start must be 1-var affine or constant."
            )
        if len(size_expr.vars) > 1:
            raise TypeError(
                "cp_model.new_interval_var: size must be 1-var affine or constant."
            )
        if len(end_expr.vars) > 1:
            raise TypeError(
                "cp_model.new_interval_var: end must be 1-var affine or constant."
            )
        return IntervalVar(
            self.__model,
            self.__var_list,
            start_expr,
            size_expr,
            end_expr,
            None,
            name,
        )

    def new_interval_var_series(
        self,
        name: str,
        index: pd.Index,
        starts: Union[LinearExprT, pd.Series],
        sizes: Union[LinearExprT, pd.Series],
        ends: Union[LinearExprT, pd.Series],
    ) -> pd.Series:
        """Creates a series of interval variables with the given name.

        Args:
          name (str): Required. The name of the variable set.
          index (pd.Index): Required. The index to use for the variable set.
          starts (Union[LinearExprT, pd.Series]): The start of each interval in the
            set. If a `pd.Series` is passed in, it will be based on the
            corresponding values of the pd.Series.
          sizes (Union[LinearExprT, pd.Series]): The size of each interval in the
            set. If a `pd.Series` is passed in, it will be based on the
            corresponding values of the pd.Series.
          ends (Union[LinearExprT, pd.Series]): The ends of each interval in the
            set. If a `pd.Series` is passed in, it will be based on the
            corresponding values of the pd.Series.

        Returns:
          pd.Series: The interval variable set indexed by its corresponding
          dimensions.

        Raises:
          TypeError: if the `index` is invalid (e.g. a `DataFrame`).
          ValueError: if the `name` is not a valid identifier or already exists.
          ValueError: if the all the indexes do not match.
        """
        if not isinstance(index, pd.Index):
            raise TypeError("Non-index object is used as index")
        if not name.isidentifier():
            raise ValueError(f"name={name!r} is not a valid identifier")

        starts = _convert_to_linear_expr_series_and_validate_index(starts, index)
        sizes = _convert_to_linear_expr_series_and_validate_index(sizes, index)
        ends = _convert_to_linear_expr_series_and_validate_index(ends, index)
        interval_array = []
        for i in index:
            interval_array.append(
                self.new_interval_var(
                    start=starts[i],
                    size=sizes[i],
                    end=ends[i],
                    name=f"{name}[{i}]",
                )
            )
        return pd.Series(index=index, data=interval_array)

    def new_fixed_size_interval_var(
        self, start: LinearExprT, size: IntegralT, name: str
    ) -> IntervalVar:
        """Creates an interval variable from start, and a fixed size.

        An interval variable is a constraint, that is itself used in other
        constraints like NoOverlap.

        Args:
          start: The start of the interval. It must be of the form a * var + b.
          size: The size of the interval. It must be an integer value.
          name: The name of the interval variable.

        Returns:
          An `IntervalVar` object.
        """
        start_expr = self.parse_linear_expression(start)
        size_expr = self.parse_linear_expression(size)
        end_expr = self.parse_linear_expression(start + size)
        if len(start_expr.vars) > 1:
            raise TypeError(
                "cp_model.new_interval_var: start must be affine or constant."
            )
        return IntervalVar(
            self.__model,
            self.__var_list,
            start_expr,
            size_expr,
            end_expr,
            None,
            name,
        )

    def new_fixed_size_interval_var_series(
        self,
        name: str,
        index: pd.Index,
        starts: Union[LinearExprT, pd.Series],
        sizes: Union[IntegralT, pd.Series],
    ) -> pd.Series:
        """Creates a series of interval variables with the given name.

        Args:
          name (str): Required. The name of the variable set.
          index (pd.Index): Required. The index to use for the variable set.
          starts (Union[LinearExprT, pd.Series]): The start of each interval in the
            set. If a `pd.Series` is passed in, it will be based on the
            corresponding values of the pd.Series.
          sizes (Union[IntegralT, pd.Series]): The fixed size of each interval in
            the set. If a `pd.Series` is passed in, it will be based on the
            corresponding values of the pd.Series.

        Returns:
          pd.Series: The interval variable set indexed by its corresponding
          dimensions.

        Raises:
          TypeError: if the `index` is invalid (e.g. a `DataFrame`).
          ValueError: if the `name` is not a valid identifier or already exists.
          ValueError: if the all the indexes do not match.
        """
        if not isinstance(index, pd.Index):
            raise TypeError("Non-index object is used as index")
        if not name.isidentifier():
            raise ValueError(f"name={name!r} is not a valid identifier")

        starts = _convert_to_linear_expr_series_and_validate_index(starts, index)
        sizes = _convert_to_integral_series_and_validate_index(sizes, index)
        interval_array = []
        for i in index:
            interval_array.append(
                self.new_fixed_size_interval_var(
                    start=starts[i],
                    size=sizes[i],
                    name=f"{name}[{i}]",
                )
            )
        return pd.Series(index=index, data=interval_array)

    def new_optional_interval_var(
        self,
        start: LinearExprT,
        size: LinearExprT,
        end: LinearExprT,
        is_present: LiteralT,
        name: str,
    ) -> IntervalVar:
        """Creates an optional interval var from start, size, end, and is_present.

        An optional interval variable is a constraint, that is itself used in other
        constraints like NoOverlap. This constraint is protected by a presence
        literal that indicates if it is active or not.

        Internally, it ensures that `is_present` implies `start + size ==
        end`.

        Args:
          start: The start of the interval. It must be of the form a * var + b.
          size: The size of the interval. It must be of the form a * var + b.
          end: The end of the interval. It must be of the form a * var + b.
          is_present: A literal that indicates if the interval is active or not. A
            inactive interval is simply ignored by all constraints.
          name: The name of the interval variable.

        Returns:
          An `IntervalVar` object.
        """

        # Creates the IntervalConstraintProto object.
        is_present_index = self.get_or_make_boolean_index(is_present)
        start_expr = self.parse_linear_expression(start)
        size_expr = self.parse_linear_expression(size)
        end_expr = self.parse_linear_expression(end)
        if len(start_expr.vars) > 1:
            raise TypeError(
                "cp_model.new_interval_var: start must be affine or constant."
            )
        if len(size_expr.vars) > 1:
            raise TypeError(
                "cp_model.new_interval_var: size must be affine or constant."
            )
        if len(end_expr.vars) > 1:
            raise TypeError(
                "cp_model.new_interval_var: end must be affine or constant."
            )
        return IntervalVar(
            self.__model,
            self.__var_list,
            start_expr,
            size_expr,
            end_expr,
            is_present_index,
            name,
        )

    def new_optional_interval_var_series(
        self,
        name: str,
        index: pd.Index,
        starts: Union[LinearExprT, pd.Series],
        sizes: Union[LinearExprT, pd.Series],
        ends: Union[LinearExprT, pd.Series],
        are_present: Union[LiteralT, pd.Series],
    ) -> pd.Series:
        """Creates a series of interval variables with the given name.

        Args:
          name (str): Required. The name of the variable set.
          index (pd.Index): Required. The index to use for the variable set.
          starts (Union[LinearExprT, pd.Series]): The start of each interval in the
            set. If a `pd.Series` is passed in, it will be based on the
            corresponding values of the pd.Series.
          sizes (Union[LinearExprT, pd.Series]): The size of each interval in the
            set. If a `pd.Series` is passed in, it will be based on the
            corresponding values of the pd.Series.
          ends (Union[LinearExprT, pd.Series]): The ends of each interval in the
            set. If a `pd.Series` is passed in, it will be based on the
            corresponding values of the pd.Series.
          are_present (Union[LiteralT, pd.Series]): The performed literal of each
            interval in the set. If a `pd.Series` is passed in, it will be based on
            the corresponding values of the pd.Series.

        Returns:
          pd.Series: The interval variable set indexed by its corresponding
          dimensions.

        Raises:
          TypeError: if the `index` is invalid (e.g. a `DataFrame`).
          ValueError: if the `name` is not a valid identifier or already exists.
          ValueError: if the all the indexes do not match.
        """
        if not isinstance(index, pd.Index):
            raise TypeError("Non-index object is used as index")
        if not name.isidentifier():
            raise ValueError(f"name={name!r} is not a valid identifier")

        starts = _convert_to_linear_expr_series_and_validate_index(starts, index)
        sizes = _convert_to_linear_expr_series_and_validate_index(sizes, index)
        ends = _convert_to_linear_expr_series_and_validate_index(ends, index)
        are_present = _convert_to_literal_series_and_validate_index(are_present, index)

        interval_array = []
        for i in index:
            interval_array.append(
                self.new_optional_interval_var(
                    start=starts[i],
                    size=sizes[i],
                    end=ends[i],
                    is_present=are_present[i],
                    name=f"{name}[{i}]",
                )
            )
        return pd.Series(index=index, data=interval_array)

    def new_optional_fixed_size_interval_var(
        self,
        start: LinearExprT,
        size: IntegralT,
        is_present: LiteralT,
        name: str,
    ) -> IntervalVar:
        """Creates an interval variable from start, and a fixed size.

        An interval variable is a constraint, that is itself used in other
        constraints like NoOverlap.

        Args:
          start: The start of the interval. It must be of the form a * var + b.
          size: The size of the interval. It must be an integer value.
          is_present: A literal that indicates if the interval is active or not. A
            inactive interval is simply ignored by all constraints.
          name: The name of the interval variable.

        Returns:
          An `IntervalVar` object.
        """
        start_expr = self.parse_linear_expression(start)
        size_expr = self.parse_linear_expression(size)
        end_expr = self.parse_linear_expression(start + size)
        if len(start_expr.vars) > 1:
            raise TypeError(
                "cp_model.new_interval_var: start must be affine or constant."
            )
        is_present_index = self.get_or_make_boolean_index(is_present)
        return IntervalVar(
            self.__model,
            self.__var_list,
            start_expr,
            size_expr,
            end_expr,
            is_present_index,
            name,
        )

    def new_optional_fixed_size_interval_var_series(
        self,
        name: str,
        index: pd.Index,
        starts: Union[LinearExprT, pd.Series],
        sizes: Union[IntegralT, pd.Series],
        are_present: Union[LiteralT, pd.Series],
    ) -> pd.Series:
        """Creates a series of interval variables with the given name.

        Args:
          name (str): Required. The name of the variable set.
          index (pd.Index): Required. The index to use for the variable set.
          starts (Union[LinearExprT, pd.Series]): The start of each interval in the
            set. If a `pd.Series` is passed in, it will be based on the
            corresponding values of the pd.Series.
          sizes (Union[IntegralT, pd.Series]): The fixed size of each interval in
            the set. If a `pd.Series` is passed in, it will be based on the
            corresponding values of the pd.Series.
          are_present (Union[LiteralT, pd.Series]): The performed literal of each
            interval in the set. If a `pd.Series` is passed in, it will be based on
            the corresponding values of the pd.Series.

        Returns:
          pd.Series: The interval variable set indexed by its corresponding
          dimensions.

        Raises:
          TypeError: if the `index` is invalid (e.g. a `DataFrame`).
          ValueError: if the `name` is not a valid identifier or already exists.
          ValueError: if the all the indexes do not match.
        """
        if not isinstance(index, pd.Index):
            raise TypeError("Non-index object is used as index")
        if not name.isidentifier():
            raise ValueError(f"name={name!r} is not a valid identifier")

        starts = _convert_to_linear_expr_series_and_validate_index(starts, index)
        sizes = _convert_to_integral_series_and_validate_index(sizes, index)
        are_present = _convert_to_literal_series_and_validate_index(are_present, index)
        interval_array = []
        for i in index:
            interval_array.append(
                self.new_optional_fixed_size_interval_var(
                    start=starts[i],
                    size=sizes[i],
                    is_present=are_present[i],
                    name=f"{name}[{i}]",
                )
            )
        return pd.Series(index=index, data=interval_array)

    def add_no_overlap(self, interval_vars: Iterable[IntervalVar]) -> Constraint:
        """Adds NoOverlap(interval_vars).

        A NoOverlap constraint ensures that all present intervals do not overlap
        in time.

        Args:
          interval_vars: The list of interval variables to constrain.

        Returns:
          An instance of the `Constraint` class.
        """
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.no_overlap.intervals.extend(
            [self.get_interval_index(x) for x in interval_vars]
        )
        return ct

    def add_no_overlap_2d(
        self,
        x_intervals: Iterable[IntervalVar],
        y_intervals: Iterable[IntervalVar],
    ) -> Constraint:
        """Adds NoOverlap2D(x_intervals, y_intervals).

        A NoOverlap2D constraint ensures that all present rectangles do not overlap
        on a plane. Each rectangle is aligned with the X and Y axis, and is defined
        by two intervals which represent its projection onto the X and Y axis.

        Furthermore, one box is optional if at least one of the x or y interval is
        optional.

        Args:
          x_intervals: The X coordinates of the rectangles.
          y_intervals: The Y coordinates of the rectangles.

        Returns:
          An instance of the `Constraint` class.
        """
        ct = Constraint(self)
        model_ct = self.__model.constraints[ct.index]
        model_ct.no_overlap_2d.x_intervals.extend(
            [self.get_interval_index(x) for x in x_intervals]
        )
        model_ct.no_overlap_2d.y_intervals.extend(
            [self.get_interval_index(x) for x in y_intervals]
        )
        return ct

    def add_cumulative(
        self,
        intervals: Iterable[IntervalVar],
        demands: Iterable[LinearExprT],
        capacity: LinearExprT,
    ) -> Constraint:
        """Adds Cumulative(intervals, demands, capacity).

        This constraint enforces that:

            for all t:
              sum(demands[i]
                if (start(intervals[i]) <= t < end(intervals[i])) and
                (intervals[i] is present)) <= capacity

        Args:
          intervals: The list of intervals.
          demands: The list of demands for each interval. Each demand must be >= 0.
            Each demand can be a 1-var affine expression (a * x + b).
          capacity: The maximum capacity of the cumulative constraint. It can be a
            1-var affine expression (a * x + b).

        Returns:
          An instance of the `Constraint` class.
        """
        cumulative = Constraint(self)
        model_ct = self.__model.constraints[cumulative.index]
        model_ct.cumulative.intervals.extend(
            [self.get_interval_index(x) for x in intervals]
        )
        for d in demands:
            model_ct.cumulative.demands.append(self.parse_linear_expression(d))
        model_ct.cumulative.capacity.CopyFrom(self.parse_linear_expression(capacity))
        return cumulative

    # Support for model cloning.
    def clone(self) -> "CpModel":
        """Reset the model, and creates a new one from a CpModelProto instance."""
        clone = CpModel()
        clone.proto.CopyFrom(self.proto)
        clone.rebuild_var_and_constant_map()
        return clone

    def __copy__(self):
        return self.clone()

    def __deepcopy__(self, memo):
        return self.clone()

    def rebuild_var_and_constant_map(self):
        """Internal method used during model cloning."""
        for i, var in enumerate(self.__model.variables):
            if len(var.domain) == 2 and var.domain[0] == var.domain[1]:
                self.__constant_map[var.domain[0]] = i
            is_boolean = (
                len(var.domain) == 2 and var.domain[0] >= 0 and var.domain[1] <= 1
            )
            self.__var_list.append(IntVar(self.__model, i, is_boolean, None))

    def get_bool_var_from_proto_index(self, index: int) -> IntVar:
        """Returns an already created Boolean variable from its index."""
        result = self._get_int_var(index)
        if not result.is_boolean:
            raise ValueError(
                f"get_bool_var_from_proto_index: index {index} does not reference a"
                " boolean variable"
            )
        return result

    def get_int_var_from_proto_index(self, index: int) -> IntVar:
        """Returns an already created integer variable from its index."""
        return self._get_int_var(index)

    def get_interval_var_from_proto_index(self, index: int) -> IntervalVar:
        """Returns an already created interval variable from its index."""
        if index < 0 or index >= len(self.__model.constraints):
            raise ValueError(
                f"get_interval_var_from_proto_index: out of bound index {index}"
            )
        ct = self.__model.constraints[index]
        if not ct.HasField("interval"):
            raise ValueError(
                f"get_interval_var_from_proto_index: index {index} does not"
                " reference an" + " interval variable"
            )

        return IntervalVar(self.__model, self.__var_list, index, None, None, None, None)

    # Helpers.

    def __str__(self) -> str:
        return str(self.__model)

    @property
    def proto(self) -> cp_model_pb2.CpModelProto:
        """Returns the underlying CpModelProto."""
        return self.__model

    def negated(self, index: int) -> int:
        return -index - 1

    def get_or_make_index(self, arg: VariableT) -> int:
        """Returns the index of a variable, its negation, or a number."""
        if isinstance(arg, IntVar):
            return arg.index
        if isinstance(arg, IntegralTypes):
            return self.get_or_make_index_from_constant(arg)
        raise TypeError(
            f"NotSupported: model.get_or_make_index({type(arg).__name__!r})"
        )

    def get_or_make_boolean_index(self, arg: LiteralT) -> int:
        """Returns an index from a boolean expression."""
        if isinstance(arg, IntVar):
            self.assert_is_boolean_variable(arg)
            return arg.index
        if isinstance(arg, cmh.NotBooleanVariable):
            self.assert_is_boolean_variable(arg.negated())
            return arg.index
        if isinstance(arg, IntegralTypes):
            if arg == ~False:  # -1
                return self.get_or_make_index_from_constant(1)
            if arg == ~True:  # -2
                return self.get_or_make_index_from_constant(0)
            arg = cmn.assert_is_zero_or_one(arg)
            return self.get_or_make_index_from_constant(arg)
        if cmn.is_boolean(arg):
            return self.get_or_make_index_from_constant(int(arg))
        raise TypeError(
            "not supported:" f" model.get_or_make_boolean_index({type(arg).__name__!r})"
        )

    def get_interval_index(self, arg: IntervalVar) -> int:
        if not isinstance(arg, IntervalVar):
            raise TypeError(
                f"NotSupported: model.get_interval_index({type(arg).__name__!r})"
            )
        return arg.index

    def get_or_make_index_from_constant(self, value: IntegralT) -> int:
        if value in self.__constant_map:
            return self.__constant_map[value]
        constant_var = self.new_int_var(value, value, "")
        self.__constant_map[value] = constant_var.index
        return constant_var.index

    def parse_linear_expression(
        self, linear_expr: LinearExprT, negate: bool = False
    ) -> cp_model_pb2.LinearExpressionProto:
        """Returns a LinearExpressionProto built from a LinearExpr instance."""
        result: cp_model_pb2.LinearExpressionProto = (
            cp_model_pb2.LinearExpressionProto()
        )
        mult = -1 if negate else 1
        if isinstance(linear_expr, IntegralTypes):
            result.offset = int(linear_expr) * mult
            return result

        # Raises TypeError if linear_expr is not an integer.
        flat_expr = cmh.FlatIntExpr(linear_expr)
        result.offset = flat_expr.offset
        for var in flat_expr.vars:
            result.vars.append(var.index)
        for coeff in flat_expr.coeffs:
            result.coeffs.append(coeff * mult)
        return result

    def _set_objective(self, obj: ObjLinearExprT, minimize: bool):
        """Sets the objective of the model."""
        self.clear_objective()
        if isinstance(obj, IntegralTypes):
            self.__model.objective.offset = int(obj)
            self.__model.objective.scaling_factor = 1.0
        elif isinstance(obj, LinearExpr):
            if obj.is_integer():
                int_obj = cmh.FlatIntExpr(obj)
                for var in int_obj.vars:
                    self.__model.objective.vars.append(var.index)
                if minimize:
                    self.__model.objective.scaling_factor = 1.0
                    self.__model.objective.offset = int_obj.offset
                    self.__model.objective.coeffs.extend(int_obj.coeffs)
                else:
                    self.__model.objective.scaling_factor = -1.0
                    self.__model.objective.offset = -int_obj.offset
                    for c in int_obj.coeffs:
                        self.__model.objective.coeffs.append(-c)
            else:
                float_obj = cmh.FlatFloatExpr(obj)
                for var in float_obj.vars:
                    self.__model.floating_point_objective.vars.append(var.index)
                self.__model.floating_point_objective.coeffs.extend(float_obj.coeffs)
                self.__model.floating_point_objective.maximize = not minimize
                self.__model.floating_point_objective.offset = float_obj.offset
        else:
            raise TypeError(
                f"TypeError: {type(obj).__name__!r} is not a valid objective"
            )

    def minimize(self, obj: ObjLinearExprT):
        """Sets the objective of the model to minimize(obj)."""
        self._set_objective(obj, minimize=True)

    def maximize(self, obj: ObjLinearExprT):
        """Sets the objective of the model to maximize(obj)."""
        self._set_objective(obj, minimize=False)

    def has_objective(self) -> bool:
        return self.__model.HasField("objective") or self.__model.HasField(
            "floating_point_objective"
        )

    def clear_objective(self):
        self.__model.ClearField("objective")
        self.__model.ClearField("floating_point_objective")

    def add_decision_strategy(
        self,
        variables: Sequence[IntVar],
        var_strategy: cp_model_pb2.DecisionStrategyProto.VariableSelectionStrategy,
        domain_strategy: cp_model_pb2.DecisionStrategyProto.DomainReductionStrategy,
    ) -> None:
        """Adds a search strategy to the model.

        Args:
          variables: a list of variables this strategy will assign.
          var_strategy: heuristic to choose the next variable to assign.
          domain_strategy: heuristic to reduce the domain of the selected variable.
            Currently, this is advanced code: the union of all strategies added to
            the model must be complete, i.e. instantiates all variables. Otherwise,
            solve() will fail.
        """

        strategy: cp_model_pb2.DecisionStrategyProto = (
            self.__model.search_strategy.add()
        )
        for v in variables:
            expr = strategy.exprs.add()
            if v.index >= 0:
                expr.vars.append(v.index)
                expr.coeffs.append(1)
            else:
                expr.vars.append(self.negated(v.index))
                expr.coeffs.append(-1)
                expr.offset = 1

        strategy.variable_selection_strategy = var_strategy
        strategy.domain_reduction_strategy = domain_strategy

    def model_stats(self) -> str:
        """Returns a string containing some model statistics."""
        return cmh.CpSatHelper.model_stats(self.__model)

    def validate(self) -> str:
        """Returns a string indicating that the model is invalid."""
        return cmh.CpSatHelper.validate_model(self.__model)

    def export_to_file(self, file: str) -> bool:
        """Write the model as a protocol buffer to 'file'.

        Args:
          file: file to write the model to. If the filename ends with 'txt', the
            model will be written as a text file, otherwise, the binary format will
            be used.

        Returns:
          True if the model was correctly written.
        """
        return cmh.CpSatHelper.write_model_to_file(self.__model, file)

    @overload
    def add_hint(self, var: IntVar, value: int) -> None: ...

    @overload
    def add_hint(self, literal: BoolVarT, value: bool) -> None: ...

    def add_hint(self, var, value) -> None:
        """Adds 'var == value' as a hint to the solver."""
        if var.index >= 0:
            self.__model.solution_hint.vars.append(self.get_or_make_index(var))
            self.__model.solution_hint.values.append(int(value))
        else:
            self.__model.solution_hint.vars.append(self.negated(var.index))
            self.__model.solution_hint.values.append(int(not value))

    def clear_hints(self):
        """Removes any solution hint from the model."""
        self.__model.ClearField("solution_hint")

    def add_assumption(self, lit: LiteralT) -> None:
        """Adds the literal to the model as assumptions."""
        self.__model.assumptions.append(self.get_or_make_boolean_index(lit))

    def add_assumptions(self, literals: Iterable[LiteralT]) -> None:
        """Adds the literals to the model as assumptions."""
        for lit in literals:
            self.add_assumption(lit)

    def clear_assumptions(self) -> None:
        """Removes all assumptions from the model."""
        self.__model.ClearField("assumptions")

    # Helpers.
    def assert_is_boolean_variable(self, x: LiteralT) -> None:
        if isinstance(x, IntVar):
            var = self.__model.variables[x.index]
            if len(var.domain) != 2 or var.domain[0] < 0 or var.domain[1] > 1:
                raise TypeError(
                    f"TypeError: {type(x).__name__!r} is not a boolean variable"
                )
        elif not isinstance(x, cmh.NotBooleanVariable):
            raise TypeError(
                f"TypeError: {type(x).__name__!r}  is not a boolean variable"
            )

    # Compatibility with pre PEP8
    # pylint: disable=invalid-name

    def Name(self) -> str:
        return self.name

    def SetName(self, name: str) -> None:
        self.name = name

    def Proto(self) -> cp_model_pb2.CpModelProto:
        return self.proto

    NewIntVar = new_int_var
    NewIntVarFromDomain = new_int_var_from_domain
    NewBoolVar = new_bool_var
    NewConstant = new_constant
    NewIntVarSeries = new_int_var_series
    NewBoolVarSeries = new_bool_var_series
    AddLinearConstraint = add_linear_constraint
    AddLinearExpressionInDomain = add_linear_expression_in_domain
    Add = add
    AddAllDifferent = add_all_different
    AddElement = add_element
    AddCircuit = add_circuit
    AddMultipleCircuit = add_multiple_circuit
    AddAllowedAssignments = add_allowed_assignments
    AddForbiddenAssignments = add_forbidden_assignments
    AddAutomaton = add_automaton
    AddInverse = add_inverse
    AddReservoirConstraint = add_reservoir_constraint
    AddReservoirConstraintWithActive = add_reservoir_constraint_with_active
    AddImplication = add_implication
    AddBoolOr = add_bool_or
    AddAtLeastOne = add_at_least_one
    AddAtMostOne = add_at_most_one
    AddExactlyOne = add_exactly_one
    AddBoolAnd = add_bool_and
    AddBoolXOr = add_bool_xor
    AddMinEquality = add_min_equality
    AddMaxEquality = add_max_equality
    AddDivisionEquality = add_division_equality
    AddAbsEquality = add_abs_equality
    AddModuloEquality = add_modulo_equality
    AddMultiplicationEquality = add_multiplication_equality
    NewIntervalVar = new_interval_var
    NewIntervalVarSeries = new_interval_var_series
    NewFixedSizeIntervalVar = new_fixed_size_interval_var
    NewOptionalIntervalVar = new_optional_interval_var
    NewOptionalIntervalVarSeries = new_optional_interval_var_series
    NewOptionalFixedSizeIntervalVar = new_optional_fixed_size_interval_var
    NewOptionalFixedSizeIntervalVarSeries = new_optional_fixed_size_interval_var_series
    AddNoOverlap = add_no_overlap
    AddNoOverlap2D = add_no_overlap_2d
    AddCumulative = add_cumulative
    Clone = clone
    GetBoolVarFromProtoIndex = get_bool_var_from_proto_index
    GetIntVarFromProtoIndex = get_int_var_from_proto_index
    GetIntervalVarFromProtoIndex = get_interval_var_from_proto_index
    Minimize = minimize
    Maximize = maximize
    HasObjective = has_objective
    ClearObjective = clear_objective
    AddDecisionStrategy = add_decision_strategy
    ModelStats = model_stats
    Validate = validate
    ExportToFile = export_to_file
    AddHint = add_hint
    ClearHints = clear_hints
    AddAssumption = add_assumption
    AddAssumptions = add_assumptions
    ClearAssumptions = clear_assumptions

    # pylint: enable=invalid-name


@overload
def expand_generator_or_tuple(
    args: Union[Tuple[LiteralT, ...], Iterable[LiteralT]],
) -> Union[Iterable[LiteralT], LiteralT]: ...


@overload
def expand_generator_or_tuple(
    args: Union[Tuple[LinearExprT, ...], Iterable[LinearExprT]],
) -> Union[Iterable[LinearExprT], LinearExprT]: ...


def expand_generator_or_tuple(args):
    if hasattr(args, "__len__"):  # Tuple
        if len(args) != 1:
            return args
        if isinstance(args[0], (NumberTypes, LinearExpr)):
            return args
    # Generator
    return args[0]


class CpSolver:
    """Main solver class.

    The purpose of this class is to search for a solution to the model provided
    to the solve() method.

    Once solve() is called, this class allows inspecting the solution found
    with the value() and boolean_value() methods, as well as general statistics
    about the solve procedure.
    """

    def __init__(self) -> None:
        self.__response_wrapper: Optional[cmh.ResponseWrapper] = None
        self.parameters: sat_parameters_pb2.SatParameters = (
            sat_parameters_pb2.SatParameters()
        )
        self.log_callback: Optional[Callable[[str], None]] = None
        self.best_bound_callback: Optional[Callable[[float], None]] = None
        self.__solve_wrapper: Optional[cmh.SolveWrapper] = None
        self.__lock: threading.Lock = threading.Lock()

    def solve(
        self,
        model: CpModel,
        solution_callback: Optional["CpSolverSolutionCallback"] = None,
    ) -> cp_model_pb2.CpSolverStatus:
        """Solves a problem and passes each solution to the callback if not null."""
        with self.__lock:
            self.__solve_wrapper = cmh.SolveWrapper()

        self.__solve_wrapper.set_parameters(self.parameters)
        if solution_callback is not None:
            self.__solve_wrapper.add_solution_callback(solution_callback)

        if self.log_callback is not None:
            self.__solve_wrapper.add_log_callback(self.log_callback)

        if self.best_bound_callback is not None:
            self.__solve_wrapper.add_best_bound_callback(self.best_bound_callback)

        self.__response_wrapper = (
            self.__solve_wrapper.solve_and_return_response_wrapper(model.proto)
        )

        if solution_callback is not None:
            self.__solve_wrapper.clear_solution_callback(solution_callback)

        with self.__lock:
            self.__solve_wrapper = None

        return self.__response_wrapper.status()

    def stop_search(self) -> None:
        """Stops the current search asynchronously."""
        with self.__lock:
            if self.__solve_wrapper:
                self.__solve_wrapper.stop_search()

    def value(self, expression: LinearExprT) -> int:
        """Returns the value of a linear expression after solve."""
        return self._checked_response.value(expression)

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

        Raises:
          RuntimeError: if solve() has not been called.
        """
        if self.__response_wrapper is None:
            raise RuntimeError("solve() has not been called.")
        return pd.Series(
            data=[self.__response_wrapper.value(var) for var in variables],
            index=_get_index(variables),
        )

    def boolean_value(self, literal: LiteralT) -> bool:
        """Returns the boolean value of a literal after solve."""
        return self._checked_response.boolean_value(literal)

    def boolean_values(self, variables: _IndexOrSeries) -> pd.Series:
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

        Raises:
          RuntimeError: if solve() has not been called.
        """
        if self.__response_wrapper is None:
            raise RuntimeError("solve() has not been called.")
        return pd.Series(
            data=[
                self.__response_wrapper.boolean_value(literal) for literal in variables
            ],
            index=_get_index(variables),
        )

    @property
    def objective_value(self) -> float:
        """Returns the value of the objective after solve."""
        return self._checked_response.objective_value()

    @property
    def best_objective_bound(self) -> float:
        """Returns the best lower (upper) bound found when min(max)imizing."""
        return self._checked_response.best_objective_bound()

    @property
    def num_booleans(self) -> int:
        """Returns the number of boolean variables managed by the SAT solver."""
        return self._checked_response.num_booleans()

    @property
    def num_conflicts(self) -> int:
        """Returns the number of conflicts since the creation of the solver."""
        return self._checked_response.num_conflicts()

    @property
    def num_branches(self) -> int:
        """Returns the number of search branches explored by the solver."""
        return self._checked_response.num_branches()

    @property
    def wall_time(self) -> float:
        """Returns the wall time in seconds since the creation of the solver."""
        return self._checked_response.wall_time()

    @property
    def user_time(self) -> float:
        """Returns the user time in seconds since the creation of the solver."""
        return self._checked_response.user_time()

    @property
    def response_proto(self) -> cp_model_pb2.CpSolverResponse:
        """Returns the response object."""
        return self._checked_response.response()

    def response_stats(self) -> str:
        """Returns some statistics on the solution found as a string."""
        return self._checked_response.response_stats()

    def sufficient_assumptions_for_infeasibility(self) -> Sequence[int]:
        """Returns the indices of the infeasible assumptions."""
        return self._checked_response.sufficient_assumptions_for_infeasibility()

    def status_name(self, status: Optional[Any] = None) -> str:
        """Returns the name of the status returned by solve()."""
        if status is None:
            status = self._checked_response.status()
        return cp_model_pb2.CpSolverStatus.Name(status)

    def solution_info(self) -> str:
        """Returns some information on the solve process.

        Returns some information on how the solution was found, or the reason
        why the model or the parameters are invalid.

        Raises:
          RuntimeError: if solve() has not been called.
        """
        return self._checked_response.solution_info()

    @property
    def _checked_response(self) -> cmh.ResponseWrapper:
        """Checks solve() has been called, and returns a response wrapper."""
        if self.__response_wrapper is None:
            raise RuntimeError("solve() has not been called.")
        return self.__response_wrapper

    # Compatibility with pre PEP8
    # pylint: disable=invalid-name

    def BestObjectiveBound(self) -> float:
        return self.best_objective_bound

    def BooleanValue(self, literal: LiteralT) -> bool:
        return self.boolean_value(literal)

    def BooleanValues(self, variables: _IndexOrSeries) -> pd.Series:
        return self.boolean_values(variables)

    def NumBooleans(self) -> int:
        return self.num_booleans

    def NumConflicts(self) -> int:
        return self.num_conflicts

    def NumBranches(self) -> int:
        return self.num_branches

    def ObjectiveValue(self) -> float:
        return self.objective_value

    def ResponseProto(self) -> cp_model_pb2.CpSolverResponse:
        return self.response_proto

    def ResponseStats(self) -> str:
        return self.response_stats()

    def Solve(
        self,
        model: CpModel,
        solution_callback: Optional["CpSolverSolutionCallback"] = None,
    ) -> cp_model_pb2.CpSolverStatus:
        return self.solve(model, solution_callback)

    def SolutionInfo(self) -> str:
        return self.solution_info()

    def StatusName(self, status: Optional[Any] = None) -> str:
        return self.status_name(status)

    def StopSearch(self) -> None:
        self.stop_search()

    def SufficientAssumptionsForInfeasibility(self) -> Sequence[int]:
        return self.sufficient_assumptions_for_infeasibility()

    def UserTime(self) -> float:
        return self.user_time

    def Value(self, expression: LinearExprT) -> int:
        return self.value(expression)

    def Values(self, variables: _IndexOrSeries) -> pd.Series:
        return self.values(variables)

    def WallTime(self) -> float:
        return self.wall_time

    def SolveWithSolutionCallback(
        self, model: CpModel, callback: "CpSolverSolutionCallback"
    ) -> cp_model_pb2.CpSolverStatus:
        """DEPRECATED Use solve() with the callback argument."""
        warnings.warn(
            "solve_with_solution_callback is deprecated; use solve() with"
            + "the callback argument.",
            DeprecationWarning,
        )
        return self.solve(model, callback)

    def SearchForAllSolutions(
        self, model: CpModel, callback: "CpSolverSolutionCallback"
    ) -> cp_model_pb2.CpSolverStatus:
        """DEPRECATED Use solve() with the right parameter.

        Search for all solutions of a satisfiability problem.

        This method searches for all feasible solutions of a given model.
        Then it feeds the solution to the callback.

        Note that the model cannot contain an objective.

        Args:
          model: The model to solve.
          callback: The callback that will be called at each solution.

        Returns:
          The status of the solve:

          * *FEASIBLE* if some solutions have been found
          * *INFEASIBLE* if the solver has proved there are no solution
          * *OPTIMAL* if all solutions have been found
        """
        warnings.warn(
            "search_for_all_solutions is deprecated; use solve() with"
            + "enumerate_all_solutions = True.",
            DeprecationWarning,
        )
        if model.has_objective():
            raise TypeError(
                "Search for all solutions is only defined on satisfiability problems"
            )
        # Store old parameter.
        enumerate_all = self.parameters.enumerate_all_solutions
        self.parameters.enumerate_all_solutions = True

        status: cp_model_pb2.CpSolverStatus = self.solve(model, callback)

        # Restore parameter.
        self.parameters.enumerate_all_solutions = enumerate_all
        return status


# pylint: enable=invalid-name


class CpSolverSolutionCallback(cmh.SolutionCallback):
    """Solution callback.

    This class implements a callback that will be called at each new solution
    found during search.

    The method on_solution_callback() will be called by the solver, and must be
    implemented. The current solution can be queried using the boolean_value()
    and value() methods.

    These methods returns the same information as their counterpart in the
    `CpSolver` class.
    """

    def __init__(self) -> None:
        cmh.SolutionCallback.__init__(self)

    def OnSolutionCallback(self) -> None:
        """Proxy for the same method in snake case."""
        self.on_solution_callback()

    def boolean_value(self, lit: LiteralT) -> bool:
        """Returns the boolean value of a boolean literal.

        Args:
            lit: A boolean variable or its negation.

        Returns:
            The Boolean value of the literal in the solution.

        Raises:
            RuntimeError: if `lit` is not a boolean variable or its negation.
        """
        if not self.has_response():
            raise RuntimeError("solve() has not been called.")
        return self.BooleanValue(lit)

    def value(self, expression: LinearExprT) -> int:
        """Evaluates an linear expression in the current solution.

        Args:
            expression: a linear expression of the model.

        Returns:
            An integer value equal to the evaluation of the linear expression
            against the current solution.

        Raises:
            RuntimeError: if 'expression' is not a LinearExpr.
        """
        if not self.has_response():
            raise RuntimeError("solve() has not been called.")
        return self.Value(expression)

    def has_response(self) -> bool:
        return self.HasResponse()

    def stop_search(self) -> None:
        """Stops the current search asynchronously."""
        if not self.has_response():
            raise RuntimeError("solve() has not been called.")
        self.StopSearch()

    @property
    def objective_value(self) -> float:
        """Returns the value of the objective after solve."""
        if not self.has_response():
            raise RuntimeError("solve() has not been called.")
        return self.ObjectiveValue()

    @property
    def best_objective_bound(self) -> float:
        """Returns the best lower (upper) bound found when min(max)imizing."""
        if not self.has_response():
            raise RuntimeError("solve() has not been called.")
        return self.BestObjectiveBound()

    @property
    def num_booleans(self) -> int:
        """Returns the number of boolean variables managed by the SAT solver."""
        if not self.has_response():
            raise RuntimeError("solve() has not been called.")
        return self.NumBooleans()

    @property
    def num_conflicts(self) -> int:
        """Returns the number of conflicts since the creation of the solver."""
        if not self.has_response():
            raise RuntimeError("solve() has not been called.")
        return self.NumConflicts()

    @property
    def num_branches(self) -> int:
        """Returns the number of search branches explored by the solver."""
        if not self.has_response():
            raise RuntimeError("solve() has not been called.")
        return self.NumBranches()

    @property
    def num_integer_propagations(self) -> int:
        """Returns the number of integer propagations done by the solver."""
        if not self.has_response():
            raise RuntimeError("solve() has not been called.")
        return self.NumIntegerPropagations()

    @property
    def num_boolean_propagations(self) -> int:
        """Returns the number of Boolean propagations done by the solver."""
        if not self.has_response():
            raise RuntimeError("solve() has not been called.")
        return self.NumBooleanPropagations()

    @property
    def deterministic_time(self) -> float:
        """Returns the determistic time in seconds since the creation of the solver."""
        if not self.has_response():
            raise RuntimeError("solve() has not been called.")
        return self.DeterministicTime()

    @property
    def wall_time(self) -> float:
        """Returns the wall time in seconds since the creation of the solver."""
        if not self.has_response():
            raise RuntimeError("solve() has not been called.")
        return self.WallTime()

    @property
    def user_time(self) -> float:
        """Returns the user time in seconds since the creation of the solver."""
        if not self.has_response():
            raise RuntimeError("solve() has not been called.")
        return self.UserTime()

    @property
    def response_proto(self) -> cp_model_pb2.CpSolverResponse:
        """Returns the response object."""
        if not self.has_response():
            raise RuntimeError("solve() has not been called.")
        return self.Response()


class ObjectiveSolutionPrinter(CpSolverSolutionCallback):
    """Display the objective value and time of intermediate solutions."""

    def __init__(self) -> None:
        CpSolverSolutionCallback.__init__(self)
        self.__solution_count = 0
        self.__start_time = time.time()

    def on_solution_callback(self) -> None:
        """Called on each new solution."""
        current_time = time.time()
        obj = self.objective_value
        print(
            f"Solution {self.__solution_count}, time ="
            f" {current_time - self.__start_time:0.2f} s, objective = {obj}",
            flush=True,
        )
        self.__solution_count += 1

    def solution_count(self) -> int:
        """Returns the number of solutions found."""
        return self.__solution_count


class VarArrayAndObjectiveSolutionPrinter(CpSolverSolutionCallback):
    """Print intermediate solutions (objective, variable values, time)."""

    def __init__(self, variables: Sequence[IntVar]) -> None:
        CpSolverSolutionCallback.__init__(self)
        self.__variables: Sequence[IntVar] = variables
        self.__solution_count: int = 0
        self.__start_time: float = time.time()

    def on_solution_callback(self) -> None:
        """Called on each new solution."""
        current_time = time.time()
        obj = self.objective_value
        print(
            f"Solution {self.__solution_count}, time ="
            f" {current_time - self.__start_time:0.2f} s, objective = {obj}"
        )
        for v in self.__variables:
            print(f"  {v} = {self.value(v)}", end=" ")
        print(flush=True)
        self.__solution_count += 1

    @property
    def solution_count(self) -> int:
        """Returns the number of solutions found."""
        return self.__solution_count


class VarArraySolutionPrinter(CpSolverSolutionCallback):
    """Print intermediate solutions (variable values, time)."""

    def __init__(self, variables: Sequence[IntVar]) -> None:
        CpSolverSolutionCallback.__init__(self)
        self.__variables: Sequence[IntVar] = variables
        self.__solution_count: int = 0
        self.__start_time: float = time.time()

    def on_solution_callback(self) -> None:
        """Called on each new solution."""
        current_time = time.time()
        print(
            f"Solution {self.__solution_count}, time ="
            f" {current_time - self.__start_time:0.2f} s"
        )
        for v in self.__variables:
            print(f"  {v} = {self.value(v)}", end=" ")
        print(flush=True)
        self.__solution_count += 1

    @property
    def solution_count(self) -> int:
        """Returns the number of solutions found."""
        return self.__solution_count


def _get_index(obj: _IndexOrSeries) -> pd.Index:
    """Returns the indices of `obj` as a `pd.Index`."""
    if isinstance(obj, pd.Series):
        return obj.index
    return obj


def _convert_to_integral_series_and_validate_index(
    value_or_series: Union[IntegralT, pd.Series], index: pd.Index
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
    if isinstance(value_or_series, IntegralTypes):
        return pd.Series(data=value_or_series, index=index)
    elif isinstance(value_or_series, pd.Series):
        if value_or_series.index.equals(index):
            return value_or_series
        else:
            raise ValueError("index does not match")
    else:
        raise TypeError(f"invalid type={type(value_or_series).__name__!r}")


def _convert_to_linear_expr_series_and_validate_index(
    value_or_series: Union[LinearExprT, pd.Series], index: pd.Index
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
    if isinstance(value_or_series, IntegralTypes):
        return pd.Series(data=value_or_series, index=index)
    elif isinstance(value_or_series, pd.Series):
        if value_or_series.index.equals(index):
            return value_or_series
        else:
            raise ValueError("index does not match")
    else:
        raise TypeError(f"invalid type={type(value_or_series).__name__!r}")


def _convert_to_literal_series_and_validate_index(
    value_or_series: Union[LiteralT, pd.Series], index: pd.Index
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
    if isinstance(value_or_series, IntegralTypes):
        return pd.Series(data=value_or_series, index=index)
    elif isinstance(value_or_series, pd.Series):
        if value_or_series.index.equals(index):
            return value_or_series
        else:
            raise ValueError("index does not match")
    else:
        raise TypeError(f"invalid type={type(value_or_series).__name__!r}")
