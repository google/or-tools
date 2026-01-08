#!/usr/bin/env python3
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

* [`CpModel`](#cp_model.CpModel): Methods for creating models, including
  variables and constraints.
* [`CpSolver`](#cp_model.CpSolver): Methods for solving a model and evaluating
  solutions.

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

from collections.abc import Callable, Iterable, Sequence
import copy
import threading
import time
from typing import (
    Any,
    Optional,
    Union,
    overload,
)
import warnings

import numpy as np
import pandas as pd

from ortools.sat.python import cp_model_helper as cmh
from ortools.util.python import sorted_interval_list

# Import external types.
BoundedLinearExpression = cmh.BoundedLinearExpression
Constraint = cmh.Constraint
CpModelProto = cmh.CpModelProto
CpSolverResponse = cmh.CpSolverResponse
CpSolverStatus = cmh.CpSolverStatus
Domain = sorted_interval_list.Domain
FlatFloatExpr = cmh.FlatFloatExpr
FlatIntExpr = cmh.FlatIntExpr
IntervalVar = cmh.IntervalVar
IntVar = cmh.IntVar
LinearExpr = cmh.LinearExpr
NotBooleanVariable = cmh.NotBooleanVariable
SatParameters = cmh.SatParameters


# The classes below allow linear expressions to be expressed naturally with the
# usual arithmetic operators + - * / and with constant numbers, which makes the
# python API very intuitive. See../ samples/*.py for examples.

INT_MIN = -(2**63)  # hardcoded to be platform independent.
INT_MAX = 2**63 - 1
INT32_MIN = -(2**31)
INT32_MAX = 2**31 - 1

# CpSolver status (exported to avoid importing cp_model_cp2).
UNKNOWN = cmh.CpSolverStatus.UNKNOWN
UNKNOWN = cmh.CpSolverStatus.UNKNOWN
MODEL_INVALID = cmh.CpSolverStatus.MODEL_INVALID
FEASIBLE = cmh.CpSolverStatus.FEASIBLE
INFEASIBLE = cmh.CpSolverStatus.INFEASIBLE
OPTIMAL = cmh.CpSolverStatus.OPTIMAL

# Variable selection strategy
CHOOSE_FIRST = cmh.DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_FIRST
CHOOSE_LOWEST_MIN = (
    cmh.DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_LOWEST_MIN
)
CHOOSE_HIGHEST_MAX = (
    cmh.DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_HIGHEST_MAX
)
CHOOSE_MIN_DOMAIN_SIZE = (
    cmh.DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_MIN_DOMAIN_SIZE
)
CHOOSE_MAX_DOMAIN_SIZE = (
    cmh.DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_MAX_DOMAIN_SIZE
)

# Domain reduction strategy
SELECT_MIN_VALUE = cmh.DecisionStrategyProto.DomainReductionStrategy.SELECT_MIN_VALUE
SELECT_MAX_VALUE = cmh.DecisionStrategyProto.DomainReductionStrategy.SELECT_MAX_VALUE
SELECT_LOWER_HALF = cmh.DecisionStrategyProto.DomainReductionStrategy.SELECT_LOWER_HALF
SELECT_UPPER_HALF = cmh.DecisionStrategyProto.DomainReductionStrategy.SELECT_UPPER_HALF
SELECT_MEDIAN_VALUE = (
    cmh.DecisionStrategyProto.DomainReductionStrategy.SELECT_MEDIAN_VALUE
)
SELECT_RANDOM_HALF = (
    cmh.DecisionStrategyProto.DomainReductionStrategy.SELECT_RANDOM_HALF
)

# Search branching
AUTOMATIC_SEARCH = cmh.SatParameters.SearchBranching.AUTOMATIC_SEARCH
FIXED_SEARCH = cmh.SatParameters.SearchBranching.FIXED_SEARCH
PORTFOLIO_SEARCH = cmh.SatParameters.SearchBranching.PORTFOLIO_SEARCH
LP_SEARCH = cmh.SatParameters.SearchBranching.LP_SEARCH
PSEUDO_COST_SEARCH = cmh.SatParameters.SearchBranching.PSEUDO_COST_SEARCH
PORTFOLIO_WITH_QUICK_RESTART_SEARCH = (
    cmh.SatParameters.SearchBranching.PORTFOLIO_WITH_QUICK_RESTART_SEARCH
)
HINT_SEARCH = cmh.SatParameters.SearchBranching.HINT_SEARCH
PARTIAL_FIXED_SEARCH = cmh.SatParameters.SearchBranching.PARTIAL_FIXED_SEARCH
RANDOMIZED_SEARCH = cmh.SatParameters.SearchBranching.RANDOMIZED_SEARCH

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

ArcT = tuple[IntegralT, IntegralT, LiteralT]
_IndexOrSeries = Union[pd.Index, pd.Series]


# Helper functions.
enable_warnings = False

# warnings.deprecated is python3.13+. Not compatible with Open Source (3.10+).
# pylint: disable=g-bare-generic
def deprecated(message: str) -> Callable[[Callable], Callable]:
    """Decorator that warns about a deprecated function."""

    def deprecated_decorator(func) -> Callable:
        def deprecated_func(*args, **kwargs):
            if enable_warnings:
                warnings.warn(
                    f"{func.__name__} is a deprecated function. {message}",
                    category=DeprecationWarning,
                    stacklevel=2,
                )
                warnings.simplefilter("default", DeprecationWarning)
            return func(*args, **kwargs)

        return deprecated_func

    return deprecated_decorator


def deprecated_method(func, old_name: str) -> Callable:
    """Wrapper that warns about a deprecated method."""

    def deprecated_func(*args, **kwargs) -> Any:
        if enable_warnings:
            warnings.warn(
                f"{old_name} is a deprecated function. Use {func.__name__} instead.",
                category=DeprecationWarning,
                stacklevel=2,
            )
            warnings.simplefilter("default", DeprecationWarning)
        return func(*args, **kwargs)

    return deprecated_func


# pylint: enable=g-bare-generic


def snake_case_to_camel_case(name: str) -> str:
    """Converts a snake_case name to CamelCase."""
    words = name.split("_")
    return (
        "".join(word.capitalize() for word in words)
        .replace("2d", "2D")
        .replace("Xor", "XOr")
    )


def object_is_a_true_literal(literal: LiteralT) -> bool:
    """Checks if literal is either True, or a Boolean literals fixed to True."""
    if isinstance(literal, IntVar):
        proto = literal.proto
        return len(proto.domain) == 2 and proto.domain[0] == 1 and proto.domain[1] == 1
    if isinstance(literal, cmh.NotBooleanVariable):
        proto = literal.negated().proto
        return len(proto.domain) == 2 and proto.domain[0] == 0 and proto.domain[1] == 0
    if isinstance(literal, (bool, np.bool_)):
        return bool(literal)
    if isinstance(literal, IntegralTypes):
        literal_as_int = int(literal)
        return literal_as_int == 1 or literal_as_int == ~False
    return False


def object_is_a_false_literal(literal: LiteralT) -> bool:
    """Checks if literal is either False, or a Boolean literals fixed to False."""
    if isinstance(literal, IntVar):
        proto = literal.proto
        return len(proto.domain) == 2 and proto.domain[0] == 0 and proto.domain[1] == 0
    if isinstance(literal, cmh.NotBooleanVariable):
        proto = literal.negated().proto
        return len(proto.domain) == 2 and proto.domain[0] == 1 and proto.domain[1] == 1
    if isinstance(literal, (bool, np.bool_)):
        return not bool(literal)
    if isinstance(literal, IntegralTypes):
        literal_as_int = int(literal)
        return literal_as_int == 0 or literal_as_int == ~True
    return False


def _get_index(obj: _IndexOrSeries) -> pd.Index:
    """Returns the indices of `obj` as a `pd.Index`."""
    if isinstance(obj, pd.Series):
        return obj.index
    return obj


@overload
def _convert_to_series_and_validate_index(
    value_or_series: Union[LinearExprT, pd.Series], index: pd.Index
) -> pd.Series: ...


@overload
def _convert_to_series_and_validate_index(
    value_or_series: Union[LiteralT, pd.Series], index: pd.Index
) -> pd.Series: ...


@overload
def _convert_to_series_and_validate_index(
    value_or_series: Union[IntegralT, pd.Series], index: pd.Index
) -> pd.Series: ...


def _convert_to_series_and_validate_index(value_or_series, index):
    """Returns a pd.Series of the given index with the corresponding values."""
    if isinstance(value_or_series, pd.Series):
        if value_or_series.index.equals(index):
            return value_or_series
        else:
            raise ValueError("index does not match")
    return pd.Series(data=value_or_series, index=index)


class CpModel(cmh.CpBaseModel):
    """Methods for building a CP model.

    Methods beginning with:

    * ```new_``` create integer, boolean, or interval variables.
    * ```add_``` create new constraints and add them to the model.
    """

    def __init__(self, model_proto: Optional[cmh.CpModelProto] = None) -> None:
        cmh.CpBaseModel.__init__(self, model_proto)
        self._add_pre_pep8_methods()

    # Naming.
    @property
    def name(self) -> str:
        """Returns the name of the model."""
        if not self.model_proto or not self.model_proto.name:
            return ""
        return self.model_proto.name

    @name.setter
    def name(self, name: str):
        """Sets the name of the model."""
        self.model_proto.name = name

    # Integer variable.
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
        return (
            IntVar(self.model_proto)
            .with_name(name)
            .with_domain(sorted_interval_list.Domain(lb, ub))
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
        return IntVar(self.model_proto).with_name(name).with_domain(domain)

    def new_bool_var(self, name: str) -> IntVar:
        """Creates a 0-1 variable with the given name."""
        return (
            IntVar(self.model_proto)
            .with_name(name)
            .with_domain(sorted_interval_list.Domain(0, 1))
        )

    def new_constant(self, value: IntegralT) -> IntVar:
        """Declares a constant integer."""
        return IntVar(self.model_proto, self.get_or_make_index_from_constant(value))

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

        lower_bounds = _convert_to_series_and_validate_index(lower_bounds, index)
        upper_bounds = _convert_to_series_and_validate_index(upper_bounds, index)
        return pd.Series(
            index=index,
            data=[
                # pylint: disable=g-complex-comprehension
                IntVar(self.model_proto)
                .with_name(f"{name}[{i}]")
                .with_domain(
                    sorted_interval_list.Domain(lower_bounds[i], upper_bounds[i])
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
                IntVar(self.model_proto)
                .with_name(f"{name}[{i}]")
                .with_domain(sorted_interval_list.Domain(0, 1))
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
            return self._add_bounded_linear_expression(ble)
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
            return self._add_bounded_linear_expression(ct)
        if ct and self.is_boolean_value(ct):
            return self.add_bool_or([True])
        if not ct and self.is_boolean_value(ct):
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
        return self._add_all_different(*expressions)

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

        return self._add_element(index, expressions, target)

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
        return self._add_circuit(arcs)

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
        return self._add_routes(arcs)

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

        return self._add_table(expressions, tuples_list, False)

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

        return self._add_table(expressions, tuples_list, True)

    def add_automaton(
        self,
        transition_expressions: Sequence[LinearExprT],
        starting_state: IntegralT,
        final_states: Sequence[IntegralT],
        transition_triples: Sequence[tuple[IntegralT, IntegralT, IntegralT]],
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

        return self._add_automaton(
            transition_expressions,
            starting_state,
            final_states,
            transition_triples,
        )

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
        return self._add_inverse(variables, inverse_variables)

    def add_reservoir_constraint(
        self,
        times: Sequence[LinearExprT],
        level_changes: Sequence[LinearExprT],
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

        return self._add_reservoir(
            times,
            level_changes,
            [],
            min_level,
            max_level,
        )

    def add_reservoir_constraint_with_active(
        self,
        times: Sequence[LinearExprT],
        level_changes: Sequence[LinearExprT],
        actives: Sequence[LiteralT],
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

        if not times:
            raise ValueError("Reservoir constraint must have a non-empty times array")

        return self._add_reservoir(
            times,
            level_changes,
            actives,
            min_level,
            max_level,
        )

    def add_map_domain(
        self, var: IntVar, bool_var_array: Iterable[IntVar], offset: IntegralT = 0
    ):
        """Adds `var == i + offset <=> bool_var_array[i] == true for all i`."""
        for i, bool_var in enumerate(bool_var_array):
            self.add(var == i + offset).only_enforce_if(bool_var)
            self.add(var != i + offset).only_enforce_if(~bool_var)

    def add_implication(self, a: LiteralT, b: LiteralT) -> Constraint:
        """Adds `a => b` (`a` implies `b`)."""
        return self.add_bool_and(b).only_enforce_if(a)

    @overload
    def add_bool_or(self, literals: Iterable[LiteralT]) -> Constraint: ...

    @overload
    def add_bool_or(self, *literals: LiteralT) -> Constraint: ...

    def add_bool_or(self, *literals):
        """Adds `Or(literals) == true`: sum(literals) >= 1."""
        return self._add_bool_argument_constraint(
            cmh.BoolArgumentConstraint.bool_or, *literals
        )

    @overload
    def add_at_least_one(self, literals: Iterable[LiteralT]) -> Constraint: ...

    @overload
    def add_at_least_one(self, *literals: LiteralT) -> Constraint: ...

    def add_at_least_one(self, *literals):
        """Same as `add_bool_or`: `sum(literals) >= 1`."""
        return self._add_bool_argument_constraint(
            cmh.BoolArgumentConstraint.bool_or, *literals
        )

    @overload
    def add_at_most_one(self, literals: Iterable[LiteralT]) -> Constraint: ...

    @overload
    def add_at_most_one(self, *literals: LiteralT) -> Constraint: ...

    def add_at_most_one(self, *literals) -> Constraint:
        """Adds `AtMostOne(literals)`: `sum(literals) <= 1`."""
        return self._add_bool_argument_constraint(
            cmh.BoolArgumentConstraint.at_most_one, *literals
        )

    @overload
    def add_exactly_one(self, literals: Iterable[LiteralT]) -> Constraint: ...

    @overload
    def add_exactly_one(self, *literals: LiteralT) -> Constraint: ...

    def add_exactly_one(self, *literals):
        """Adds `ExactlyOne(literals)`: `sum(literals) == 1`."""
        return self._add_bool_argument_constraint(
            cmh.BoolArgumentConstraint.exactly_one, *literals
        )

    @overload
    def add_bool_and(self, literals: Iterable[LiteralT]) -> Constraint: ...

    @overload
    def add_bool_and(self, *literals: LiteralT) -> Constraint: ...

    def add_bool_and(self, *literals):
        """Adds `And(literals) == true`."""
        return self._add_bool_argument_constraint(
            cmh.BoolArgumentConstraint.bool_and, *literals
        )

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
        return self._add_bool_argument_constraint(
            cmh.BoolArgumentConstraint.bool_xor, *literals
        )

    @overload
    def add_min_equality(
        self, target: LinearExprT, expressions: Iterable[LinearExprT]
    ) -> Constraint: ...

    @overload
    def add_min_equality(
        self, target: LinearExprT, *expressions: LinearExprT
    ) -> Constraint: ...

    def add_min_equality(self, target, *expressions) -> Constraint:
        """Adds `target == Min(expressions)`."""
        return self._add_linear_argument_constraint(
            cmh.LinearArgumentConstraint.min, target, *expressions
        )

    @overload
    def add_max_equality(
        self, target: LinearExprT, expressions: Iterable[LinearExprT]
    ) -> Constraint: ...

    @overload
    def add_max_equality(
        self, target: LinearExprT, *expressions: LinearExprT
    ) -> Constraint: ...

    def add_max_equality(self, target, *expressions) -> Constraint:
        """Adds `target == Max(expressions)`."""
        return self._add_linear_argument_constraint(
            cmh.LinearArgumentConstraint.max, target, *expressions
        )

    def add_division_equality(
        self, target: LinearExprT, num: LinearExprT, denom: LinearExprT
    ) -> Constraint:
        """Adds `target == num // denom` (integer division rounded towards 0)."""
        return self._add_linear_argument_constraint(
            cmh.LinearArgumentConstraint.div, target, [num, denom]
        )

    def add_abs_equality(self, target: LinearExprT, expr: LinearExprT) -> Constraint:
        """Adds `target == Abs(expr)`."""
        return self._add_linear_argument_constraint(
            cmh.LinearArgumentConstraint.max, target, [expr, -expr]
        )

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
        return self._add_linear_argument_constraint(
            cmh.LinearArgumentConstraint.mod, target, [expr, mod]
        )

    def add_multiplication_equality(
        self,
        target: LinearExprT,
        *expressions: Union[Iterable[LinearExprT], LinearExprT],
    ) -> Constraint:
        """Adds `target == expressions[0] * .. * expressions[n]`."""
        return self._add_linear_argument_constraint(
            cmh.LinearArgumentConstraint.prod, target, *expressions
        )

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
        return self._new_interval_var(name, start, size, end, [])

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

        starts = _convert_to_series_and_validate_index(starts, index)
        sizes = _convert_to_series_and_validate_index(sizes, index)
        ends = _convert_to_series_and_validate_index(ends, index)
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
        return self._new_interval_var(name, start, size, start + size, [])

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

        starts = _convert_to_series_and_validate_index(starts, index)
        sizes = _convert_to_series_and_validate_index(sizes, index)
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
        return self._new_interval_var(
            name,
            start,
            size,
            end,
            [is_present],
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

        starts = _convert_to_series_and_validate_index(starts, index)
        sizes = _convert_to_series_and_validate_index(sizes, index)
        ends = _convert_to_series_and_validate_index(ends, index)
        are_present = _convert_to_series_and_validate_index(are_present, index)

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
        return self._new_interval_var(
            name,
            start,
            size,
            start + size,
            [is_present],
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

        starts = _convert_to_series_and_validate_index(starts, index)
        sizes = _convert_to_series_and_validate_index(sizes, index)
        are_present = _convert_to_series_and_validate_index(are_present, index)
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

    def add_no_overlap(self, intervals: Iterable[IntervalVar]) -> Constraint:
        """Adds NoOverlap(interval_vars).

        A NoOverlap constraint ensures that all present intervals do not overlap
        in time.

        Args:
          intervals: The list of interval variables to constrain.

        Returns:
          An instance of the `Constraint` class.
        """
        return self._add_no_overlap(intervals)

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
        return self._add_no_overlap_2d(x_intervals, y_intervals)

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
        return self._add_cumulative(intervals, demands, capacity)

    # Support for model cloning.
    def clone(self) -> "CpModel":
        """Reset the model, and creates a new one from a CpModelProto instance."""
        clone = CpModel()
        clone.proto.copy_from(self.proto)
        clone.rebuild_constant_map()
        return clone

    def __copy__(self):
        return CpModel(self.model_proto)

    def __deepcopy__(self, memo):
        return CpModel(copy.deepcopy(self.model_proto, memo))

    def get_bool_var_from_proto_index(self, index: int) -> IntVar:
        """Returns an already created Boolean variable from its index."""
        if index < 0 or index >= len(self.model_proto.variables):
            raise ValueError(
                f"get_bool_var_from_proto_index: out of bound index {index}"
            )
        result = IntVar(self.model_proto, index)
        if not result.is_boolean:
            raise TypeError(
                f"get_bool_var_from_proto_index: index {index} does not reference a"
                " boolean variable"
            )
        return result

    def get_int_var_from_proto_index(self, index: int) -> IntVar:
        """Returns an already created integer variable from its index."""
        if index < 0 or index >= len(self.model_proto.variables):
            raise ValueError(
                f"get_int_var_from_proto_index: out of bound index {index}"
            )
        return IntVar(self.model_proto, index)

    def get_interval_var_from_proto_index(self, index: int) -> IntervalVar:
        """Returns an already created interval variable from its index."""
        if index < 0 or index >= len(self.model_proto.constraints):
            raise ValueError(
                f"get_interval_var_from_proto_index: out of bound index {index}"
            )
        ct = self.model_proto.constraints[index]
        if not ct.has_interval():
            raise ValueError(
                f"get_interval_var_from_proto_index: index {index} does not"
                " reference an" + " interval variable"
            )

        return IntervalVar(self.model_proto, index)

    def __str__(self) -> str:
        return str(self.model_proto)

    @property
    def proto(self) -> cmh.CpModelProto:
        """Returns the underlying CpModelProto."""
        return self.model_proto

    def negated(self, index: int) -> int:
        return -index - 1

    def _set_objective(self, obj: ObjLinearExprT, maximize: bool):
        """Sets the objective of the model."""
        self.clear_objective()
        if isinstance(obj, IntegralTypes):
            self.model_proto.objective.offset = int(obj)
            self.model_proto.objective.scaling_factor = 1.0
        elif isinstance(obj, LinearExpr):
            if obj.is_integer():
                int_obj = cmh.FlatIntExpr(obj)
                for var in int_obj.vars:
                    self.model_proto.objective.vars.append(var.index)
                if maximize:
                    self.model_proto.objective.scaling_factor = -1.0
                    self.model_proto.objective.offset = -int_obj.offset
                    for c in int_obj.coeffs:
                        self.model_proto.objective.coeffs.append(-c)
                else:
                    self.model_proto.objective.scaling_factor = 1.0
                    self.model_proto.objective.offset = int_obj.offset
                    self.model_proto.objective.coeffs.extend(int_obj.coeffs)
            else:
                float_obj = cmh.FlatFloatExpr(obj)
                for var in float_obj.vars:
                    self.model_proto.floating_point_objective.vars.append(var.index)
                self.model_proto.floating_point_objective.coeffs.extend(
                    float_obj.coeffs
                )
                self.model_proto.floating_point_objective.maximize = maximize
                self.model_proto.floating_point_objective.offset = float_obj.offset
        else:
            raise TypeError(
                f"TypeError: {type(obj).__name__!r} is not a valid objective"
            )

    def minimize(self, obj: ObjLinearExprT):
        """Sets the objective of the model to minimize(obj)."""
        self._set_objective(obj, maximize=False)

    def maximize(self, obj: ObjLinearExprT):
        """Sets the objective of the model to maximize(obj)."""
        self._set_objective(obj, maximize=True)

    def has_objective(self) -> bool:
        return (
            self.model_proto.has_objective()
            or self.model_proto.has_floating_point_objective()
        )

    def clear_objective(self):
        self.model_proto.clear_objective()
        self.model_proto.clear_floating_point_objective()

    def add_decision_strategy(
        self,
        variables: Iterable[IntVar],
        var_strategy: cmh.DecisionStrategyProto.VariableSelectionStrategy,
        domain_strategy: cmh.DecisionStrategyProto.DomainReductionStrategy,
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

        strategy: cmh.DecisionStrategyProto = self.model_proto.search_strategy.add()
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
        return cmh.CpSatHelper.model_stats(self.model_proto)

    def validate(self) -> str:
        """Returns a string indicating that the model is invalid."""
        return cmh.CpSatHelper.validate_model(self.model_proto)

    def export_to_file(self, file: str) -> bool:
        """Write the model as a protocol buffer to 'file'.

        Args:
          file: file to write the model to. If the filename ends with 'txt', the
            model will be written as a text file, otherwise, the binary format will
            be used.

        Returns:
          True if the model was correctly written.
        """
        return cmh.CpSatHelper.write_model_to_file(self.model_proto, file)

    def remove_all_names(self) -> None:
        """Removes all names from the model."""
        self.model_proto.clear_name()
        for v in self.model_proto.variables:
            v.clear_name()
        for c in self.model_proto.constraints:
            c.clear_name()

    @overload
    def add_hint(self, var: IntVar, value: int) -> None: ...

    @overload
    def add_hint(self, literal: BoolVarT, value: bool) -> None: ...

    def add_hint(self, var, value) -> None:
        """Adds 'var == value' as a hint to the solver."""
        if var.index >= 0:
            self.model_proto.solution_hint.vars.append(var.index)
            self.model_proto.solution_hint.values.append(int(value))
        else:
            self.model_proto.solution_hint.vars.append(self.negated(var.index))
            self.model_proto.solution_hint.values.append(int(not value))

    def clear_hints(self):
        """Removes any solution hint from the model."""
        self.model_proto.clear_solution_hint()

    def add_assumption(self, lit: LiteralT) -> None:
        """Adds the literal to the model as assumptions."""
        self.model_proto.assumptions.append(self.get_or_make_boolean_index(lit))

    def add_assumptions(self, literals: Iterable[LiteralT]) -> None:
        """Adds the literals to the model as assumptions."""
        for lit in literals:
            self.add_assumption(lit)

    def clear_assumptions(self) -> None:
        """Removes all assumptions from the model."""
        self.model_proto.assumptions.clear()

    # Compatibility with pre PEP8
    # pylint: disable=invalid-name

    def _add_pre_pep8_methods(self) -> None:
        for method_name in dir(self):
            if callable(getattr(self, method_name)) and (
                method_name.startswith("add_")
                or method_name.startswith("new_")
                or method_name.startswith("clear_")
            ):
                pre_pep8_name = snake_case_to_camel_case(method_name)
                setattr(
                    self,
                    pre_pep8_name,
                    deprecated_method(getattr(self, method_name), pre_pep8_name),
                )

        for other_method_name in [
            "add",
            "clone",
            "get_bool_var_from_proto_index",
            "get_int_var_from_proto_index",
            "get_interval_var_from_proto_index",
            "minimize",
            "maximize",
            "has_objective",
            "model_stats",
            "validate",
            "export_to_file",
        ]:
            pre_pep8_name = snake_case_to_camel_case(other_method_name)
            setattr(
                self,
                pre_pep8_name,
                deprecated_method(getattr(self, other_method_name), pre_pep8_name),
            )

    @deprecated("Use name property instead.")
    def Name(self) -> str:
        return self.name

    @deprecated("Use name property instead.")
    def SetName(self, name: str) -> None:
        self.name = name

    @deprecated("Use proto property instead.")
    def Proto(self) -> cmh.CpModelProto:
        return self.proto

    # pylint: enable=invalid-name


class CpSolver:
    """Main solver class.

    The purpose of this class is to search for a solution to the model provided
    to the solve() method.

    Once solve() is called, this class allows inspecting the solution found
    with the value() and boolean_value() methods, as well as general statistics
    about the solve procedure.
    """

    def __init__(self) -> None:
        self.__response: Optional[cmh.CpSolverResponse] = None
        self.parameters: cmh.SatParameters = cmh.SatParameters()
        self.log_callback: Optional[Callable[[str], None]] = None
        self.best_bound_callback: Optional[Callable[[float], None]] = None
        self.__solve_wrapper: Optional[cmh.SolveWrapper] = None
        self.__lock: threading.Lock = threading.Lock()

    def solve(
        self,
        model: CpModel,
        solution_callback: Optional["CpSolverSolutionCallback"] = None,
    ) -> cmh.CpSolverStatus:
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

        self.__response = self.__solve_wrapper.solve(model.proto)

        if solution_callback is not None:
            self.__solve_wrapper.clear_solution_callback(solution_callback)

        with self.__lock:
            self.__solve_wrapper = None

        return self.__response.status

    def stop_search(self) -> None:
        """Stops the current search asynchronously."""
        with self.__lock:
            if self.__solve_wrapper:
                self.__solve_wrapper.stop_search()

    def value(self, expression: LinearExprT) -> int:
        """Returns the value of a linear expression after solve."""
        return cmh.ResponseHelper.value(self._checked_response, expression)

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
        response: cmh.CpSolverResponse = self._checked_response
        return pd.Series(
            data=[cmh.ResponseHelper.value(response, var) for var in variables],
            index=_get_index(variables),
        )

    def float_value(self, expression: LinearExprT) -> float:
        """Returns the value of a linear expression after solve."""
        return cmh.ResponseHelper.float_value(self._checked_response, expression)

    def float_values(self, expressions: _IndexOrSeries) -> pd.Series:
        """Returns the float values of the input linear expressions.

        If `expressions` is a `pd.Index`, then the output will be indexed by the
        variables. If `variables` is a `pd.Series` indexed by the underlying
        dimensions, then the output will be indexed by the same underlying
        dimensions.

        Args:
          expressions (Union[pd.Index, pd.Series]): The set of expressions from
            which to get the values.

        Returns:
          pd.Series: The values of all variables in the set.

        Raises:
          RuntimeError: if solve() has not been called.
        """
        response: cmh.CpSolverResponse = self._checked_response
        return pd.Series(
            data=[
                cmh.ResponseHelper.float_value(response, expr) for expr in expressions
            ],
            index=_get_index(expressions),
        )

    def boolean_value(self, literal: LiteralT) -> bool:
        """Returns the boolean value of a literal after solve."""
        return cmh.ResponseHelper.boolean_value(self._checked_response, literal)

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
        response: cmh.CpSolverResponse = self._checked_response
        return pd.Series(
            data=[
                cmh.ResponseHelper.boolean_value(response, literal)
                for literal in variables
            ],
            index=_get_index(variables),
        )

    @property
    def objective_value(self) -> float:
        """Returns the value of the objective after solve."""
        return self._checked_response.objective_value

    @property
    def best_objective_bound(self) -> float:
        """Returns the best lower (upper) bound found when min(max)imizing."""
        return self._checked_response.best_objective_bound

    @property
    def num_booleans(self) -> int:
        """Returns the number of boolean variables managed by the SAT solver."""
        return self._checked_response.num_booleans

    @property
    def num_conflicts(self) -> int:
        """Returns the number of conflicts since the creation of the solver."""
        return self._checked_response.num_conflicts

    @property
    def num_branches(self) -> int:
        """Returns the number of search branches explored by the solver."""
        return self._checked_response.num_branches

    @property
    def num_binary_propagations(self) -> int:
        """Returns the number of Boolean propagations done by the solver."""
        return self._checked_response.num_binary_propagations

    @property
    def num_integer_propagations(self) -> int:
        """Returns the number of integer propagations done by the solver."""
        return self._checked_response.num_integer_propagations

    @property
    def deterministic_time(self) -> float:
        """Returns the deterministic time in seconds since the creation of the solver."""
        return self._checked_response.deterministic_time

    @property
    def wall_time(self) -> float:
        """Returns the wall time in seconds since the creation of the solver."""
        return self._checked_response.wall_time

    @property
    def user_time(self) -> float:
        """Returns the user time in seconds since the creation of the solver."""
        return self._checked_response.user_time

    @property
    def solve_log(self) -> str:
        """Returns the solve log.

        To enable this, the parameter log_to_response must be set to True.
        """
        return self._checked_response.solve_log

    @property
    def solve_info(self) -> str:
        """Returns the information about the solve."""
        return self._checked_response.solve_info

    @property
    def response_proto(self) -> cmh.CpSolverResponse:
        """Returns the response object."""
        return self._checked_response

    def response_stats(self) -> str:
        """Returns some statistics on the solution found as a string."""
        return cmh.CpSatHelper.solver_response_stats(self._checked_response)

    def sufficient_assumptions_for_infeasibility(self) -> Sequence[int]:
        """Returns the indices of the infeasible assumptions."""
        return cmh.ResponseHelper.sufficient_assumptions_for_infeasibility(
            self._checked_response
        )

    def status_name(self, status: Optional[Any] = None) -> str:
        """Returns the name of the status returned by solve()."""
        if status is None:
            status = self._checked_response.status()
        return status.name

    def solution_info(self) -> str:
        """Returns some information on the solve process.

        Returns some information on how the solution was found, or the reason
        why the model or the parameters are invalid.

        Raises:
          RuntimeError: if solve() has not been called.
        """
        return self._checked_response.solution_info

    @property
    def _checked_response(self) -> cmh.CpSolverResponse:
        """Checks solve() has been called, and returns a response wrapper."""
        if self.__response is None:
            raise RuntimeError("solve() has not been called.")
        return self.__response

    # Compatibility with pre PEP8
    # pylint: disable=invalid-name

    @deprecated("Use best_objective_bound property instead.")
    def BestObjectiveBound(self) -> float:
        return self.best_objective_bound

    @deprecated("Use boolean_value() method instead.")
    def BooleanValue(self, lit: LiteralT) -> bool:
        return self.boolean_value(lit)

    @deprecated("Use boolean_values() method instead.")
    def BooleanValues(self, variables: _IndexOrSeries) -> pd.Series:
        return self.boolean_values(variables)

    @deprecated("Use num_booleans property instead.")
    def NumBooleans(self) -> int:
        return self.num_booleans

    @deprecated("Use num_conflicts property instead.")
    def NumConflicts(self) -> int:
        return self.num_conflicts

    @deprecated("Use num_branches property instead.")
    def NumBranches(self) -> int:
        return self.num_branches

    @deprecated("Use objective_value property instead.")
    def ObjectiveValue(self) -> float:
        return self.objective_value

    @deprecated("Use response_proto property instead.")
    def ResponseProto(self) -> cmh.CpSolverResponse:
        return self.response_proto

    @deprecated("Use response_stats() method instead.")
    def ResponseStats(self) -> str:
        return self.response_stats()

    @deprecated("Use solve() method instead.")
    def Solve(
        self, model: CpModel, callback: "CpSolverSolutionCallback" = None
    ) -> cmh.CpSolverStatus:
        return self.solve(model, callback)

    @deprecated("Use solution_info() method instead.")
    def SolutionInfo(self) -> str:
        return self.solution_info()

    @deprecated("Use status_name() method instead.")
    def StatusName(self, status: Optional[Any] = None) -> str:
        return self.status_name(status)

    @deprecated("Use stop_search() method instead.")
    def StopSearch(self) -> None:
        self.stop_search()

    @deprecated("Use sufficient_assumptions_for_infeasibility() method instead.")
    def SufficientAssumptionsForInfeasibility(self) -> Sequence[int]:
        return self.sufficient_assumptions_for_infeasibility()

    @deprecated("Use user_time property instead.")
    def UserTime(self) -> float:
        return self.user_time

    @deprecated("Use value() method instead.")
    def Value(self, expression: LinearExprT) -> int:
        return self.value(expression)

    @deprecated("Use values() method instead.")
    def Values(self, expressions: _IndexOrSeries) -> pd.Series:
        return self.values(expressions)

    @deprecated("Use wall_time property instead.")
    def WallTime(self) -> float:
        return self.wall_time

    @deprecated("Use solve() with enumerate_all_solutions = True.")
    def SearchForAllSolutions(
        self, model: CpModel, callback: "CpSolverSolutionCallback"
    ) -> cmh.CpSolverStatus:
        """Search for all solutions of a satisfiability problem.

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
        if model.has_objective():
            raise TypeError(
                "Search for all solutions is only defined on satisfiability problems"
            )
        # Store old parameter.
        enumerate_all = self.parameters.enumerate_all_solutions
        self.parameters.enumerate_all_solutions = True

        status: cmh.CpSolverStatus = self.solve(model, callback)

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

    # pylint: disable=invalid-name
    def OnSolutionCallback(self) -> None:
        """Proxy for the same method in snake case."""
        self.on_solution_callback()

    # pylint: enable=invalid-name

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

    def float_value(self, expression: LinearExprT) -> float:
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
        return self.FloatValue(expression)

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
    def num_binary_propagations(self) -> int:
        """Returns the number of Boolean propagations done by the solver."""
        if not self.has_response():
            raise RuntimeError("solve() has not been called.")
        return self.NumBinaryPropagations()

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
    def response_proto(self) -> cmh.CpSolverResponse:
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
