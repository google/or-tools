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

"""The solution to an optimization problem defined by Model in model.py."""
import dataclasses
import enum
from typing import Dict, Optional, TypeVar

from ortools.math_opt import solution_pb2
from ortools.math_opt.python import model
from ortools.math_opt.python import sparse_containers


@enum.unique
class BasisStatus(enum.Enum):
    """Status of a variable/constraint in a LP basis.

    Attributes:
      FREE: The variable/constraint is free (it has no finite bounds).
      AT_LOWER_BOUND: The variable/constraint is at its lower bound (which must be
        finite).
      AT_UPPER_BOUND: The variable/constraint is at its upper bound (which must be
        finite).
      FIXED_VALUE: The variable/constraint has identical finite lower and upper
        bounds.
      BASIC: The variable/constraint is basic.
    """

    FREE = solution_pb2.BASIS_STATUS_FREE
    AT_LOWER_BOUND = solution_pb2.BASIS_STATUS_AT_LOWER_BOUND
    AT_UPPER_BOUND = solution_pb2.BASIS_STATUS_AT_UPPER_BOUND
    FIXED_VALUE = solution_pb2.BASIS_STATUS_FIXED_VALUE
    BASIC = solution_pb2.BASIS_STATUS_BASIC


@enum.unique
class SolutionStatus(enum.Enum):
    """Feasibility of a primal or dual solution as claimed by the solver.

    Attributes:
      UNDETERMINED: Solver does not claim a feasibility status.
      FEASIBLE: Solver claims the solution is feasible.
      INFEASIBLE: Solver claims the solution is infeasible.
    """

    UNDETERMINED = solution_pb2.SOLUTION_STATUS_UNDETERMINED
    FEASIBLE = solution_pb2.SOLUTION_STATUS_FEASIBLE
    INFEASIBLE = solution_pb2.SOLUTION_STATUS_INFEASIBLE


def parse_optional_solution_status(
    proto: solution_pb2.SolutionStatusProto,
) -> Optional[SolutionStatus]:
    """Converts a proto SolutionStatus to an optional Python SolutionStatus."""
    return (
        None
        if proto == solution_pb2.SOLUTION_STATUS_UNSPECIFIED
        else SolutionStatus(proto)
    )


def optional_solution_status_to_proto(
    status: Optional[SolutionStatus],
) -> solution_pb2.SolutionStatusProto:
    """Converts an optional Python SolutionStatus to a proto SolutionStatus."""
    return solution_pb2.SOLUTION_STATUS_UNSPECIFIED if status is None else status.value


@dataclasses.dataclass
class PrimalSolution:
    """A solution to the optimization problem in a Model.

    E.g. consider a simple linear program:
      min c * x
      s.t. A * x >= b
      x >= 0.
    A primal solution is assignment values to x. It is feasible if it satisfies
    A * x >= b and x >= 0 from above. In the class PrimalSolution variable_values
    is x and objective_value is c * x.

    For the general case of a MathOpt optimization model, see go/mathopt-solutions
    for details.

    Attributes:
      variable_values: The value assigned for each Variable in the model.
      objective_value: The value of the objective value at this solution. This
        value may not be always populated.
      feasibility_status: The feasibility of the solution as claimed by the
        solver.
    """

    variable_values: Dict[model.Variable, float] = dataclasses.field(
        default_factory=dict
    )
    objective_value: float = 0.0
    feasibility_status: SolutionStatus = SolutionStatus.UNDETERMINED

    def to_proto(self) -> solution_pb2.PrimalSolutionProto:
        """Returns an equivalent proto for a primal solution."""
        return solution_pb2.PrimalSolutionProto(
            variable_values=sparse_containers.to_sparse_double_vector_proto(
                self.variable_values
            ),
            objective_value=self.objective_value,
            feasibility_status=self.feasibility_status.value,
        )


def parse_primal_solution(
    proto: solution_pb2.PrimalSolutionProto, mod: model.Model
) -> PrimalSolution:
    """Returns an equivalent PrimalSolution from the input proto."""
    result = PrimalSolution()
    result.objective_value = proto.objective_value
    result.variable_values = sparse_containers.parse_variable_map(
        proto.variable_values, mod
    )
    status_proto = proto.feasibility_status
    if status_proto == solution_pb2.SOLUTION_STATUS_UNSPECIFIED:
        raise ValueError("Primal solution feasibility status should not be UNSPECIFIED")
    result.feasibility_status = SolutionStatus(status_proto)
    return result


@dataclasses.dataclass
class PrimalRay:
    """A direction of unbounded objective improvement in an optimization Model.

    Equivalently, a certificate of infeasibility for the dual of the optimization
    problem.

    E.g. consider a simple linear program:
      min c * x
      s.t. A * x >= b
      x >= 0.
    A primal ray is an x that satisfies:
      c * x < 0
      A * x >= 0
      x >= 0.
    Observe that given a feasible solution, any positive multiple of the primal
    ray plus that solution is still feasible, and gives a better objective
    value. A primal ray also proves the dual optimization problem infeasible.

    In the class PrimalRay, variable_values is this x.

    For the general case of a MathOpt optimization model, see
    go/mathopt-solutions for details.

    Attributes:
      variable_values: The value assigned for each Variable in the model.
    """

    variable_values: Dict[model.Variable, float] = dataclasses.field(
        default_factory=dict
    )

    def to_proto(self) -> solution_pb2.PrimalRayProto:
        """Returns an equivalent proto to this PrimalRay."""
        return solution_pb2.PrimalRayProto(
            variable_values=sparse_containers.to_sparse_double_vector_proto(
                self.variable_values
            )
        )


def parse_primal_ray(proto: solution_pb2.PrimalRayProto, mod: model.Model) -> PrimalRay:
    """Returns an equivalent PrimalRay from the input proto."""
    result = PrimalRay()
    result.variable_values = sparse_containers.parse_variable_map(
        proto.variable_values, mod
    )
    return result


@dataclasses.dataclass
class DualSolution:
    """A solution to the dual of the optimization problem given by a Model.

    E.g. consider the primal dual pair linear program pair:
      (Primal)              (Dual)
      min c * x             max b * y
      s.t. A * x >= b       s.t. y * A + r = c
      x >= 0                y, r >= 0.
    The dual solution is the pair (y, r). It is feasible if it satisfies the
    constraints from (Dual) above.

    Below, y is dual_values, r is reduced_costs, and b * y is objective_value.

    For the general case, see go/mathopt-solutions and go/mathopt-dual (and note
    that the dual objective depends on r in the general case).

    Attributes:
      dual_values: The value assigned for each LinearConstraint in the model.
      reduced_costs: The value assigned for each Variable in the model.
      objective_value: The value of the dual objective value at this solution.
        This value may not be always populated.
      feasibility_status: The feasibility of the solution as claimed by the
        solver.
    """

    dual_values: Dict[model.LinearConstraint, float] = dataclasses.field(
        default_factory=dict
    )
    reduced_costs: Dict[model.Variable, float] = dataclasses.field(default_factory=dict)
    objective_value: Optional[float] = None
    feasibility_status: SolutionStatus = SolutionStatus.UNDETERMINED

    def to_proto(self) -> solution_pb2.DualSolutionProto:
        """Returns an equivalent proto for a dual solution."""
        return solution_pb2.DualSolutionProto(
            dual_values=sparse_containers.to_sparse_double_vector_proto(
                self.dual_values
            ),
            reduced_costs=sparse_containers.to_sparse_double_vector_proto(
                self.reduced_costs
            ),
            objective_value=self.objective_value,
            feasibility_status=self.feasibility_status.value,
        )


def parse_dual_solution(
    proto: solution_pb2.DualSolutionProto, mod: model.Model
) -> DualSolution:
    """Returns an equivalent DualSolution from the input proto."""
    result = DualSolution()
    result.objective_value = (
        proto.objective_value if proto.HasField("objective_value") else None
    )
    result.dual_values = sparse_containers.parse_linear_constraint_map(
        proto.dual_values, mod
    )
    result.reduced_costs = sparse_containers.parse_variable_map(
        proto.reduced_costs, mod
    )
    status_proto = proto.feasibility_status
    if status_proto == solution_pb2.SOLUTION_STATUS_UNSPECIFIED:
        raise ValueError("Dual solution feasibility status should not be UNSPECIFIED")
    result.feasibility_status = SolutionStatus(status_proto)
    return result


@dataclasses.dataclass
class DualRay:
    """A direction of unbounded objective improvement in an optimization Model.

    A direction of unbounded improvement to the dual of an optimization,
    problem; equivalently, a certificate of primal infeasibility.

    E.g. consider the primal dual pair linear program pair:
      (Primal)              (Dual)
      min c * x             max b * y
      s.t. A * x >= b       s.t. y * A + r = c
      x >= 0                y, r >= 0.

    The dual ray is the pair (y, r) satisfying:
      b * y > 0
      y * A + r = 0
      y, r >= 0.
    Observe that adding a positive multiple of (y, r) to dual feasible solution
    maintains dual feasibility and improves the objective (proving the dual is
    unbounded). The dual ray also proves the primal problem is infeasible.

    In the class DualRay below, y is dual_values and r is reduced_costs.

    For the general case, see go/mathopt-solutions and go/mathopt-dual (and note
    that the dual objective depends on r in the general case).

    Attributes:
      dual_values: The value assigned for each LinearConstraint in the model.
      reduced_costs: The value assigned for each Variable in the model.
    """

    dual_values: Dict[model.LinearConstraint, float] = dataclasses.field(
        default_factory=dict
    )
    reduced_costs: Dict[model.Variable, float] = dataclasses.field(default_factory=dict)

    def to_proto(self) -> solution_pb2.DualRayProto:
        """Returns an equivalent proto to this PrimalRay."""
        return solution_pb2.DualRayProto(
            dual_values=sparse_containers.to_sparse_double_vector_proto(
                self.dual_values
            ),
            reduced_costs=sparse_containers.to_sparse_double_vector_proto(
                self.reduced_costs
            ),
        )


def parse_dual_ray(proto: solution_pb2.DualRayProto, mod: model.Model) -> DualRay:
    """Returns an equivalent DualRay from the input proto."""
    result = DualRay()
    result.dual_values = sparse_containers.parse_linear_constraint_map(
        proto.dual_values, mod
    )
    result.reduced_costs = sparse_containers.parse_variable_map(
        proto.reduced_costs, mod
    )
    return result


@dataclasses.dataclass
class Basis:
    """A combinatorial characterization for a solution to a linear program.

    The simplex method for solving linear programs always returns a "basic
    feasible solution" which can be described combinatorially as a Basis. A basis
    assigns a BasisStatus for every variable and linear constraint.

    E.g. consider a standard form LP:
      min c * x
      s.t. A * x = b
      x >= 0
    that has more variables than constraints and with full row rank A.

    Let n be the number of variables and m the number of linear constraints. A
    valid basis for this problem can be constructed as follows:
     * All constraints will have basis status FIXED.
     * Pick m variables such that the columns of A are linearly independent and
       assign the status BASIC.
     * Assign the status AT_LOWER for the remaining n - m variables.

    The basic solution for this basis is the unique solution of A * x = b that has
    all variables with status AT_LOWER fixed to their lower bounds (all zero). The
    resulting solution is called a basic feasible solution if it also satisfies
    x >= 0.

    See go/mathopt-basis for treatment of the general case and an explanation of
    how a dual solution is determined for a basis.

    Attributes:
      variable_status: The basis status for each variable in the model.
      constraint_status: The basis status for each linear constraint in the model.
      basic_dual_feasibility: This is an advanced feature used by MathOpt to
        characterize feasibility of suboptimal LP solutions (optimal solutions
        will always have status SolutionStatus.FEASIBLE). For single-sided LPs it
        should be equal to the feasibility status of the associated dual solution.
        For two-sided LPs it may be different in some edge cases (e.g. incomplete
        solves with primal simplex). For more details see
        go/mathopt-basis-advanced#dualfeasibility. If you are providing a starting
        basis via ModelSolveParameters.initial_basis, this value is ignored and
        can be None. It is only relevant for the basis returned by Solution.basis,
        and it is never None when returned from solve(). This is an advanced
        status. For single-sided LPs it should be equal to the feasibility status
        of the associated dual solution. For two-sided LPs it may be different in
        some edge cases (e.g. incomplete solves with primal simplex). For more
        details see go/mathopt-basis-advanced#dualfeasibility.
    """

    variable_status: Dict[model.Variable, BasisStatus] = dataclasses.field(
        default_factory=dict
    )
    constraint_status: Dict[model.LinearConstraint, BasisStatus] = dataclasses.field(
        default_factory=dict
    )
    basic_dual_feasibility: Optional[SolutionStatus] = None

    def to_proto(self) -> solution_pb2.BasisProto:
        """Returns an equivalent proto for the basis."""
        return solution_pb2.BasisProto(
            variable_status=_to_sparse_basis_status_vector_proto(self.variable_status),
            constraint_status=_to_sparse_basis_status_vector_proto(
                self.constraint_status
            ),
            basic_dual_feasibility=optional_solution_status_to_proto(
                self.basic_dual_feasibility
            ),
        )


def parse_basis(proto: solution_pb2.BasisProto, mod: model.Model) -> Basis:
    """Returns an equivalent Basis to the input proto."""
    result = Basis()
    for index, vid in enumerate(proto.variable_status.ids):
        status_proto = proto.variable_status.values[index]
        if status_proto == solution_pb2.BASIS_STATUS_UNSPECIFIED:
            raise ValueError("Variable basis status should not be UNSPECIFIED")
        result.variable_status[mod.get_variable(vid)] = BasisStatus(status_proto)
    for index, cid in enumerate(proto.constraint_status.ids):
        status_proto = proto.constraint_status.values[index]
        if status_proto == solution_pb2.BASIS_STATUS_UNSPECIFIED:
            raise ValueError("Constraint basis status should not be UNSPECIFIED")
        result.constraint_status[mod.get_linear_constraint(cid)] = BasisStatus(
            status_proto
        )
    result.basic_dual_feasibility = parse_optional_solution_status(
        proto.basic_dual_feasibility
    )
    return result


T = TypeVar("T", model.Variable, model.LinearConstraint)


def _to_sparse_basis_status_vector_proto(
    terms: Dict[T, BasisStatus]
) -> solution_pb2.SparseBasisStatusVector:
    """Converts a basis vector from a python Dict to a protocol buffer."""
    result = solution_pb2.SparseBasisStatusVector()
    if terms:
        id_and_status = sorted(
            (key.id, status.value) for (key, status) in terms.items()
        )
        ids, values = zip(*id_and_status)
        result.ids[:] = ids
        result.values[:] = values
    return result


@dataclasses.dataclass
class Solution:
    """A solution to the optimization problem in a Model."""

    primal_solution: Optional[PrimalSolution] = None
    dual_solution: Optional[DualSolution] = None
    basis: Optional[Basis] = None

    def to_proto(self) -> solution_pb2.SolutionProto:
        """Returns an equivalent proto for a solution."""
        return solution_pb2.SolutionProto(
            primal_solution=(
                self.primal_solution.to_proto()
                if self.primal_solution is not None
                else None
            ),
            dual_solution=(
                self.dual_solution.to_proto()
                if self.dual_solution is not None
                else None
            ),
            basis=self.basis.to_proto() if self.basis is not None else None,
        )


def parse_solution(proto: solution_pb2.SolutionProto, mod: model.Model) -> Solution:
    """Returns a Solution equivalent to the input proto."""
    result = Solution()
    if proto.HasField("primal_solution"):
        result.primal_solution = parse_primal_solution(proto.primal_solution, mod)
    if proto.HasField("dual_solution"):
        result.dual_solution = parse_dual_solution(proto.dual_solution, mod)
    result.basis = parse_basis(proto.basis, mod) if proto.HasField("basis") else None
    return result
