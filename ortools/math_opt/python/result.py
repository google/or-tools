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

"""The output from solving a mathematical optimization problem from model.py."""
import dataclasses
import datetime
import enum
from typing import Dict, Iterable, List, Optional, overload

from ortools.gscip import gscip_pb2
from ortools.math_opt import result_pb2
from ortools.math_opt.python import model
from ortools.math_opt.python import solution
from ortools.math_opt.solvers import osqp_pb2

_NO_DUAL_SOLUTION_ERROR = (
    "Best solution does not have an associated dual feasible solution."
)
_NO_BASIS_ERROR = "Best solution does not have an associated basis."


@enum.unique
class FeasibilityStatus(enum.Enum):
    """Problem feasibility status as claimed by the solver.

      (solver is not required to return a certificate for the claim.)

    Attributes:
      UNDETERMINED: Solver does not claim a status.
      FEASIBLE: Solver claims the problem is feasible.
      INFEASIBLE: Solver claims the problem is infeasible.
    """

    UNDETERMINED = result_pb2.FEASIBILITY_STATUS_UNDETERMINED
    FEASIBLE = result_pb2.FEASIBILITY_STATUS_FEASIBLE
    INFEASIBLE = result_pb2.FEASIBILITY_STATUS_INFEASIBLE


@dataclasses.dataclass(frozen=True)
class ProblemStatus:
    """Feasibility status of the primal problem and its dual (or dual relaxation).

    Statuses are as claimed by the solver and a dual relaxation is the dual of a
    continuous relaxation for the original problem (e.g. the LP relaxation of a
    MIP). The solver is not required to return a certificate for the feasibility
    or infeasibility claims (e.g. the solver may claim primal feasibility without
    returning a primal feasible solutuion). This combined status gives a
    comprehensive description of a solver's claims about feasibility and
    unboundedness of the solved problem. For instance,
      * a feasible status for primal and dual problems indicates the primal is
        feasible and bounded and likely has an optimal solution (guaranteed for
        problems without non-linear constraints).
      * a primal feasible and a dual infeasible status indicates the primal
        problem is unbounded (i.e. has arbitrarily good solutions).
    Note that a dual infeasible status by itself (i.e. accompanied by an
    undetermined primal status) does not imply the primal problem is unbounded as
    we could have both problems be infeasible. Also, while a primal and dual
    feasible status may imply the existence of an optimal solution, it does not
    guarantee the solver has actually found such optimal solution.

    Attributes:
      primal_status: Status for the primal problem.
      dual_status: Status for the dual problem (or for the dual of a continuous
        relaxation).
      primal_or_dual_infeasible: If true, the solver claims the primal or dual
        problem is infeasible, but it does not know which (or if both are
        infeasible). Can be true only when primal_problem_status =
        dual_problem_status = kUndetermined. This extra information is often
        needed when preprocessing determines there is no optimal solution to the
        problem (but can't determine if it is due to infeasibility, unboundedness,
        or both).
    """

    primal_status: FeasibilityStatus = FeasibilityStatus.UNDETERMINED
    dual_status: FeasibilityStatus = FeasibilityStatus.UNDETERMINED
    primal_or_dual_infeasible: bool = False

    def to_proto(self) -> result_pb2.ProblemStatusProto:
        """Returns an equivalent proto for a problem status."""
        return result_pb2.ProblemStatusProto(
            primal_status=self.primal_status.value,
            dual_status=self.dual_status.value,
            primal_or_dual_infeasible=self.primal_or_dual_infeasible,
        )


def parse_problem_status(proto: result_pb2.ProblemStatusProto) -> ProblemStatus:
    """Returns an equivalent ProblemStatus from the input proto."""
    primal_status_proto = proto.primal_status
    if primal_status_proto == result_pb2.FEASIBILITY_STATUS_UNSPECIFIED:
        raise ValueError("Primal feasibility status should not be UNSPECIFIED")
    dual_status_proto = proto.dual_status
    if dual_status_proto == result_pb2.FEASIBILITY_STATUS_UNSPECIFIED:
        raise ValueError("Dual feasibility status should not be UNSPECIFIED")
    return ProblemStatus(
        primal_status=FeasibilityStatus(primal_status_proto),
        dual_status=FeasibilityStatus(dual_status_proto),
        primal_or_dual_infeasible=proto.primal_or_dual_infeasible,
    )


@dataclasses.dataclass(frozen=True)
class ObjectiveBounds:
    """Bounds on the optimal objective value.

  MOE:begin_intracomment_strip
  See go/mathopt-objective-bounds for more details.
  MOE:end_intracomment_strip

  Attributes:
    primal_bound: Solver claims there exists a primal solution that is
      numerically feasible (i.e. feasible up to the solvers tolerance), and
      whose objective value is primal_bound.

      The optimal value is equal or better (smaller for min objectives and
      larger for max objectives) than primal_bound, but only up to
      solver-tolerances.

      MOE:begin_intracomment_strip
      See go/mathopt-objective-bounds for more details.
      MOE:end_intracomment_strip
    dual_bound: Solver claims there exists a dual solution that is numerically
      feasible (i.e. feasible up to the solvers tolerance), and whose objective
      value is dual_bound.

      For MIP solvers, the associated dual problem may be some continuous
      relaxation (e.g. LP relaxation), but it is often an implicitly defined
      problem that is a complex consequence of the solvers execution. For both
      continuous and MIP solvers, the optimal value is equal or worse (larger
      for min objective and smaller for max objectives) than dual_bound, but
      only up to solver-tolerances. Some continuous solvers provide a
      numerically safer dual bound through solver's specific output (e.g. for
      PDLP, pdlp_output.convergence_information.corrected_dual_objective).

      MOE:begin_intracomment_strip
      See go/mathopt-objective-bounds for more details.
      MOE:end_intracomment_strip
  """  # fmt: skip

    primal_bound: float = 0.0
    dual_bound: float = 0.0

    def to_proto(self) -> result_pb2.ObjectiveBoundsProto:
        """Returns an equivalent proto for objective bounds."""
        return result_pb2.ObjectiveBoundsProto(
            primal_bound=self.primal_bound, dual_bound=self.dual_bound
        )


def parse_objective_bounds(
    proto: result_pb2.ObjectiveBoundsProto,
) -> ObjectiveBounds:
    """Returns an equivalent ObjectiveBounds from the input proto."""
    return ObjectiveBounds(primal_bound=proto.primal_bound, dual_bound=proto.dual_bound)


@dataclasses.dataclass
class SolveStats:
    """Problem statuses and solve statistics returned by the solver.

    Attributes:
      solve_time: Elapsed wall clock time as measured by math_opt, roughly the
        time inside solve(). Note: this does not include work done building the
        model.
      simplex_iterations: Simplex iterations.
      barrier_iterations: Barrier iterations.
      first_order_iterations: First order iterations.
      node_count: Node count.
    """

    solve_time: datetime.timedelta = datetime.timedelta()
    simplex_iterations: int = 0
    barrier_iterations: int = 0
    first_order_iterations: int = 0
    node_count: int = 0

    def to_proto(self) -> result_pb2.SolveStatsProto:
        """Returns an equivalent proto for a solve stats."""
        result = result_pb2.SolveStatsProto(
            simplex_iterations=self.simplex_iterations,
            barrier_iterations=self.barrier_iterations,
            first_order_iterations=self.first_order_iterations,
            node_count=self.node_count,
        )
        result.solve_time.FromTimedelta(self.solve_time)
        return result


def parse_solve_stats(proto: result_pb2.SolveStatsProto) -> SolveStats:
    """Returns an equivalent SolveStats from the input proto."""
    result = SolveStats()
    result.solve_time = proto.solve_time.ToTimedelta()
    result.simplex_iterations = proto.simplex_iterations
    result.barrier_iterations = proto.barrier_iterations
    result.first_order_iterations = proto.first_order_iterations
    result.node_count = proto.node_count
    return result


@enum.unique
class TerminationReason(enum.Enum):
    """The reason a solve of a model terminated.

    These reasons are typically as reported by the underlying solver, e.g. we do
    not attempt to verify the precision of the solution returned.

    The values are:
       * OPTIMAL: A provably optimal solution (up to numerical tolerances) has
           been found.
       * INFEASIBLE: The primal problem has no feasible solutions.
       * UNBOUNDED: The primal problem is feasible and arbitrarily good solutions
           can be found along a primal ray.
       * INFEASIBLE_OR_UNBOUNDED: The primal problem is either infeasible or
           unbounded. More details on the problem status may be available in
           solve_stats.problem_status. Note that Gurobi's unbounded status may be
           mapped here as explained in
           go/mathopt-solver-specific#gurobi-inf-or-unb.
       * IMPRECISE: The problem was solved to one of the criteria above (Optimal,
           Infeasible, Unbounded, or InfeasibleOrUnbounded), but one or more
           tolerances was not met. Some primal/dual solutions/rays may be present,
           but either they will be slightly infeasible, or (if the problem was
           nearly optimal) their may be a gap between the best solution objective
           and best objective bound.

           Users can still query primal/dual solutions/rays and solution stats,
           but they are responsible for dealing with the numerical imprecision.
       * FEASIBLE: The optimizer reached some kind of limit and a primal feasible
           solution is returned. See SolveResultProto.limit_detail for detailed
           description of the kind of limit that was reached.
       * NO_SOLUTION_FOUND: The optimizer reached some kind of limit and it did
           not find a primal feasible solution. See SolveResultProto.limit_detail
           for detailed description of the kind of limit that was reached.
       * NUMERICAL_ERROR: The algorithm stopped because it encountered
           unrecoverable numerical error. No solution information is present.
       * OTHER_ERROR: The algorithm stopped because of an error not covered by one
           of the statuses defined above. No solution information is present.
    """

    OPTIMAL = result_pb2.TERMINATION_REASON_OPTIMAL
    INFEASIBLE = result_pb2.TERMINATION_REASON_INFEASIBLE
    UNBOUNDED = result_pb2.TERMINATION_REASON_UNBOUNDED
    INFEASIBLE_OR_UNBOUNDED = result_pb2.TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED
    IMPRECISE = result_pb2.TERMINATION_REASON_IMPRECISE
    FEASIBLE = result_pb2.TERMINATION_REASON_FEASIBLE
    NO_SOLUTION_FOUND = result_pb2.TERMINATION_REASON_NO_SOLUTION_FOUND
    NUMERICAL_ERROR = result_pb2.TERMINATION_REASON_NUMERICAL_ERROR
    OTHER_ERROR = result_pb2.TERMINATION_REASON_OTHER_ERROR


@enum.unique
class Limit(enum.Enum):
    """The optimizer reached a limit, partial solution information may be present.

    Values are:
       * UNDETERMINED: The underlying solver does not expose which limit was
           reached.
       * ITERATION: An iterative algorithm stopped after conducting the
           maximum number of iterations (e.g. simplex or barrier iterations).
       * TIME: The algorithm stopped after a user-specified amount of
           computation time.
       * NODE: A branch-and-bound algorithm stopped because it explored a
           maximum number of nodes in the branch-and-bound tree.
       * SOLUTION: The algorithm stopped because it found the required
           number of solutions. This is often used in MIPs to get the solver to
           return the first feasible solution it encounters.
       * MEMORY: The algorithm stopped because it ran out of memory.
       * OBJECTIVE: The algorithm stopped because it found a solution better
           than a minimum limit set by the user.
       * NORM: The algorithm stopped because the norm of an iterate became
           too large.
       * INTERRUPTED: The algorithm stopped because of an interrupt signal or a
           user interrupt request.
       * SLOW_PROGRESS: The algorithm stopped because it was unable to continue
           making progress towards the solution.
       * OTHER: The algorithm stopped due to a limit not covered by one of the
           above. Note that UNDETERMINED is used when the reason cannot be
           determined, and OTHER is used when the reason is known but does not fit
           into any of the above alternatives.
    """

    UNDETERMINED = result_pb2.LIMIT_UNDETERMINED
    ITERATION = result_pb2.LIMIT_ITERATION
    TIME = result_pb2.LIMIT_TIME
    NODE = result_pb2.LIMIT_NODE
    SOLUTION = result_pb2.LIMIT_SOLUTION
    MEMORY = result_pb2.LIMIT_MEMORY
    OBJECTIVE = result_pb2.LIMIT_OBJECTIVE
    NORM = result_pb2.LIMIT_NORM
    INTERRUPTED = result_pb2.LIMIT_INTERRUPTED
    SLOW_PROGRESS = result_pb2.LIMIT_SLOW_PROGRESS
    OTHER = result_pb2.LIMIT_OTHER


@dataclasses.dataclass
class Termination:
    """An explanation of why the solver stopped.

    Attributes:
      reason: Why the solver stopped, e.g. it found a provably optimal solution.
        Additional information in `limit` when value is FEASIBLE or
        NO_SOLUTION_FOUND, see `limit` for details.
      limit: If the solver stopped early, what caused it to stop. Have value
        UNSPECIFIED when reason is not NO_SOLUTION_FOUND or FEASIBLE. May still be
        UNSPECIFIED when reason is NO_SOLUTION_FOUND or FEASIBLE, some solvers
        cannot fill this in.
      detail: Additional, information beyond reason about why the solver stopped,
        typically solver specific.
      problem_status: Feasibility statuses for primal and dual problems.
      objective_bounds: Bounds on the optimal objective value.
    """

    reason: TerminationReason = TerminationReason.OPTIMAL
    limit: Optional[Limit] = None
    detail: str = ""
    problem_status: ProblemStatus = ProblemStatus()
    objective_bounds: ObjectiveBounds = ObjectiveBounds()

    def to_proto(self) -> result_pb2.TerminationProto:
        """Returns an equivalent protocol buffer to this Termination."""
        return result_pb2.TerminationProto(
            reason=self.reason.value,
            limit=(
                result_pb2.LIMIT_UNSPECIFIED if self.limit is None else self.limit.value
            ),
            detail=self.detail,
            problem_status=self.problem_status.to_proto(),
            objective_bounds=self.objective_bounds.to_proto(),
        )


def parse_termination(
    termination_proto: result_pb2.TerminationProto,
) -> Termination:
    """Returns a Termination that is equivalent to termination_proto."""
    reason_proto = termination_proto.reason
    limit_proto = termination_proto.limit
    if reason_proto == result_pb2.TERMINATION_REASON_UNSPECIFIED:
        raise ValueError("Termination reason should not be UNSPECIFIED")
    reason_is_limit = (
        reason_proto == result_pb2.TERMINATION_REASON_NO_SOLUTION_FOUND
    ) or (reason_proto == result_pb2.TERMINATION_REASON_FEASIBLE)
    limit_set = limit_proto != result_pb2.LIMIT_UNSPECIFIED
    if reason_is_limit != limit_set:
        raise ValueError(
            f"Termination limit (={limit_proto})) should take value other than "
            f"UNSPECIFIED if and only if termination reason (={reason_proto}) is "
            "FEASIBLE or NO_SOLUTION_FOUND"
        )
    termination = Termination()
    termination.reason = TerminationReason(reason_proto)
    termination.limit = Limit(limit_proto) if limit_set else None
    termination.detail = termination_proto.detail
    termination.problem_status = parse_problem_status(termination_proto.problem_status)
    termination.objective_bounds = parse_objective_bounds(
        termination_proto.objective_bounds
    )
    return termination


@dataclasses.dataclass
class SolveResult:
    """The result of solving an optimization problem defined by a Model.

    We attempt to return as much solution information (primal_solutions,
    primal_rays, dual_solutions, dual_rays) as each underlying solver will provide
    given its return status. Differences in the underlying solvers result in a
    weak contract on what fields will be populated for a given termination
    reason. This is discussed in detail in termination_reasons.md, and the most
    important points are summarized below:
      * When the termination reason is optimal, there will be at least one primal
        solution provided that will be feasible up to the underlying solver's
        tolerances.
      * Dual solutions are only given for convex optimization problems (e.g.
        linear programs, not integer programs).
      * A basis is only given for linear programs when solved by the simplex
        method (e.g., not with PDLP).
      * Solvers have widely varying support for returning primal and dual rays.
        E.g. a termination_reason of unbounded does not ensure that a feasible
        solution or a primal ray is returned, check termination_reasons.md for
        solver specific guarantees if this is needed. Further, many solvers will
        provide the ray but not the feasible solution when returning an unbounded
        status.
      * When the termination reason is that a limit was reached or that the result
        is imprecise, a solution may or may not be present. Further, for some
        solvers (generally, convex optimization solvers, not MIP solvers), the
        primal or dual solution may not be feasible.

    Solver specific output is also returned for some solvers (and only information
    for the solver used will be populated).

    Attributes:
      termination: The reason the solver stopped.
      solve_stats: Statistics on the solve process, e.g. running time, iterations.
      solutions: Lexicographically by primal feasibility status, dual feasibility
        status, (basic dual feasibility for simplex solvers), primal objective
        value and dual objective value.
      primal_rays: Directions of unbounded primal improvement, or equivalently,
        dual infeasibility certificates. Typically provided for terminal reasons
        UNBOUNDED and DUAL_INFEASIBLE.
      dual_rays: Directions of unbounded dual improvement, or equivalently, primal
        infeasibility certificates. Typically provided for termination reason
        INFEASIBLE.
      gscip_specific_output: statistics returned by the gSCIP solver, if used.
      osqp_specific_output: statistics returned by the OSQP solver, if used.
      pdlp_specific_output: statistics returned by the PDLP solver, if used.
    """

    termination: Termination = dataclasses.field(default_factory=Termination)
    solve_stats: SolveStats = dataclasses.field(default_factory=SolveStats)
    solutions: List[solution.Solution] = dataclasses.field(default_factory=list)
    primal_rays: List[solution.PrimalRay] = dataclasses.field(default_factory=list)
    dual_rays: List[solution.DualRay] = dataclasses.field(default_factory=list)
    # At most one of the below will be set
    gscip_specific_output: Optional[gscip_pb2.GScipOutput] = None
    osqp_specific_output: Optional[osqp_pb2.OsqpOutput] = None
    pdlp_specific_output: Optional[result_pb2.SolveResultProto.PdlpOutput] = None

    def solve_time(self) -> datetime.timedelta:
        """Shortcut for SolveResult.solve_stats.solve_time."""
        return self.solve_stats.solve_time

    def primal_bound(self) -> float:
        """Returns a primal bound on the optimal objective value as described in ObjectiveBounds.

        Will return a valid (possibly infinite) bound even if no primal feasible
        solutions are available.
        """
        return self.termination.objective_bounds.primal_bound

    def dual_bound(self) -> float:
        """Returns a dual bound on the optimal objective value as described in ObjectiveBounds.

        Will return a valid (possibly infinite) bound even if no dual feasible
        solutions are available.
        """
        return self.termination.objective_bounds.dual_bound

    def has_primal_feasible_solution(self) -> bool:
        """Indicates if at least one primal feasible solution is available.

        When termination.reason is TerminationReason.OPTIMAL or
        TerminationReason.FEASIBLE, this is guaranteed to be true and need not be
        checked.

        Returns:
          True if there is at least one primal feasible solution is available,
          False, otherwise.
        """
        if not self.solutions:
            return False
        return (
            self.solutions[0].primal_solution is not None
            and self.solutions[0].primal_solution.feasibility_status
            == solution.SolutionStatus.FEASIBLE
        )

    def objective_value(self) -> float:
        """Returns the objective value of the best primal feasible solution.

        An error will be raised if there are no primal feasible solutions.
        primal_bound() above is guaranteed to be at least as good (larger or equal
        for max problems and smaller or equal for min problems) as objective_value()
        and will never raise an error, so it may be preferable in some cases. Note
        that primal_bound() could be better than objective_value() even for optimal
        terminations, but on such optimal termination, both should satisfy the
        optimality tolerances.

         Returns:
           The objective value of the best primal feasible solution.

         Raises:
           ValueError: There are no primal feasible solutions.
        """
        if not self.has_primal_feasible_solution():
            raise ValueError("No primal feasible solution available.")
        assert self.solutions[0].primal_solution is not None
        return self.solutions[0].primal_solution.objective_value

    def best_objective_bound(self) -> float:
        """Returns a bound on the best possible objective value.

        best_objective_bound() is always equal to dual_bound(), so they can be
        used interchangeably.
        """
        return self.termination.objective_bounds.dual_bound

    @overload
    def variable_values(self, variables: None = ...) -> Dict[model.Variable, float]: ...

    @overload
    def variable_values(self, variables: model.Variable) -> float: ...

    @overload
    def variable_values(self, variables: Iterable[model.Variable]) -> List[float]: ...

    def variable_values(self, variables=None):
        """The variable values from the best primal feasible solution.

        An error will be raised if there are no primal feasible solutions.

        Args:
          variables: an optional Variable or iterator of Variables indicating what
            variable values to return. If not provided, variable_values returns a
            dictionary with all the variable values for all variables.

        Returns:
          The variable values from the best primal feasible solution.

        Raises:
          ValueError: There are no primal feasible solutions.
          TypeError: Argument is not None, a Variable or an iterable of Variables.
          KeyError: Variable values requested for an invalid variable (e.g. is not a
            Variable or is a variable for another model).
        """
        if not self.has_primal_feasible_solution():
            raise ValueError("No primal feasible solution available.")
        assert self.solutions[0].primal_solution is not None
        if variables is None:
            return self.solutions[0].primal_solution.variable_values
        if isinstance(variables, model.Variable):
            return self.solutions[0].primal_solution.variable_values[variables]
        if isinstance(variables, Iterable):
            return [
                self.solutions[0].primal_solution.variable_values[v] for v in variables
            ]
        raise TypeError(
            "unsupported type in argument for "
            f"variable_values: {type(variables).__name__!r}"
        )

    def bounded(self) -> bool:
        """Returns true only if the problem has been shown to be feasible and bounded."""
        return (
            self.termination.problem_status.primal_status == FeasibilityStatus.FEASIBLE
            and self.termination.problem_status.dual_status
            == FeasibilityStatus.FEASIBLE
        )

    def has_ray(self) -> bool:
        """Indicates if at least one primal ray is available.

        This is NOT guaranteed to be true when termination.reason is
        TerminationReason.kUnbounded or TerminationReason.kInfeasibleOrUnbounded.

        Returns:
          True if at least one primal ray is available.
        """
        return bool(self.primal_rays)

    @overload
    def ray_variable_values(
        self, variables: None = ...
    ) -> Dict[model.Variable, float]: ...

    @overload
    def ray_variable_values(self, variables: model.Variable) -> float: ...

    @overload
    def ray_variable_values(
        self, variables: Iterable[model.Variable]
    ) -> List[float]: ...

    def ray_variable_values(self, variables=None):
        """The variable values from the first primal ray.

        An error will be raised if there are no primal rays.

        Args:
          variables: an optional Variable or iterator of Variables indicating what
            variable values to return. If not provided, variable_values() returns a
            dictionary with the variable values for all variables.

        Returns:
          The variable values from the first primal ray.

        Raises:
          ValueError: There are no primal rays.
          TypeError: Argument is not None, a Variable or an iterable of Variables.
          KeyError: Variable values requested for an invalid variable (e.g. is not a
            Variable or is a variable for another model).
        """
        if not self.has_ray():
            raise ValueError("No primal ray available.")
        if variables is None:
            return self.primal_rays[0].variable_values
        if isinstance(variables, model.Variable):
            return self.primal_rays[0].variable_values[variables]
        if isinstance(variables, Iterable):
            return [self.primal_rays[0].variable_values[v] for v in variables]
        raise TypeError(
            "unsupported type in argument for "
            f"ray_variable_values: {type(variables).__name__!r}"
        )

    def has_dual_feasible_solution(self) -> bool:
        """Indicates if the best solution has an associated dual feasible solution.

        This is NOT guaranteed to be true when termination.reason is
        TerminationReason.Optimal. It also may be true even when the best solution
        does not have an associated primal feasible solution.

        Returns:
          True if the best solution has an associated dual feasible solution.
        """
        if not self.solutions:
            return False
        return (
            self.solutions[0].dual_solution is not None
            and self.solutions[0].dual_solution.feasibility_status
            == solution.SolutionStatus.FEASIBLE
        )

    @overload
    def dual_values(
        self, linear_constraints: None = ...
    ) -> Dict[model.LinearConstraint, float]: ...

    @overload
    def dual_values(self, linear_constraints: model.LinearConstraint) -> float: ...

    @overload
    def dual_values(
        self, linear_constraints: Iterable[model.LinearConstraint]
    ) -> List[float]: ...

    def dual_values(self, linear_constraints=None):
        """The dual values associated to the best solution.

        If there is at least one primal feasible solution, this corresponds to the
        dual values associated to the best primal feasible solution. An error will
        be raised if the best solution does not have an associated dual feasible
        solution.

        Args:
          linear_constraints: an optional LinearConstraint or iterator of
            LinearConstraint indicating what dual values to return. If not provided,
            dual_values() returns a dictionary with the dual values for all linear
            constraints.

        Returns:
          The dual values associated to the best solution.

        Raises:
          ValueError: The best solution does not have an associated dual feasible
            solution.
          TypeError: Argument is not None, a LinearConstraint or an iterable of
            LinearConstraint.
          KeyError: LinearConstraint values requested for an invalid
            linear constraint (e.g. is not a LinearConstraint or is a linear
            constraint for another model).
        """
        if not self.has_dual_feasible_solution():
            raise ValueError(_NO_DUAL_SOLUTION_ERROR)
        assert self.solutions[0].dual_solution is not None
        if linear_constraints is None:
            return self.solutions[0].dual_solution.dual_values
        if isinstance(linear_constraints, model.LinearConstraint):
            return self.solutions[0].dual_solution.dual_values[linear_constraints]
        if isinstance(linear_constraints, Iterable):
            return [
                self.solutions[0].dual_solution.dual_values[c]
                for c in linear_constraints
            ]
        raise TypeError(
            "unsupported type in argument for "
            f"dual_values: {type(linear_constraints).__name__!r}"
        )

    @overload
    def reduced_costs(self, variables: None = ...) -> Dict[model.Variable, float]: ...

    @overload
    def reduced_costs(self, variables: model.Variable) -> float: ...

    @overload
    def reduced_costs(self, variables: Iterable[model.Variable]) -> List[float]: ...

    def reduced_costs(self, variables=None):
        """The reduced costs associated to the best solution.

        If there is at least one primal feasible solution, this corresponds to the
        reduced costs associated to the best primal feasible solution. An error will
        be raised if the best solution does not have an associated dual feasible
        solution.

        Args:
          variables: an optional Variable or iterator of Variables indicating what
            reduced costs to return. If not provided, reduced_costs() returns a
            dictionary with the reduced costs for all variables.

        Returns:
          The reduced costs associated to the best solution.

        Raises:
          ValueError: The best solution does not have an associated dual feasible
            solution.
          TypeError: Argument is not None, a Variable or an iterable of Variables.
          KeyError: Variable values requested for an invalid variable (e.g. is not a
            Variable or is a variable for another model).
        """
        if not self.has_dual_feasible_solution():
            raise ValueError(_NO_DUAL_SOLUTION_ERROR)
        assert self.solutions[0].dual_solution is not None
        if variables is None:
            return self.solutions[0].dual_solution.reduced_costs
        if isinstance(variables, model.Variable):
            return self.solutions[0].dual_solution.reduced_costs[variables]
        if isinstance(variables, Iterable):
            return [self.solutions[0].dual_solution.reduced_costs[v] for v in variables]
        raise TypeError(
            "unsupported type in argument for "
            f"reduced_costs: {type(variables).__name__!r}"
        )

    def has_dual_ray(self) -> bool:
        """Indicates if at least one dual ray is available.

        This is NOT guaranteed to be true when termination.reason is
        TerminationReason.Infeasible.

        Returns:
          True if at least one dual ray is available.
        """
        return bool(self.dual_rays)

    @overload
    def ray_dual_values(
        self, linear_constraints: None = ...
    ) -> Dict[model.LinearConstraint, float]: ...

    @overload
    def ray_dual_values(self, linear_constraints: model.LinearConstraint) -> float: ...

    @overload
    def ray_dual_values(
        self, linear_constraints: Iterable[model.LinearConstraint]
    ) -> List[float]: ...

    def ray_dual_values(self, linear_constraints=None):
        """The dual values from the first dual ray.

        An error will be raised if there are no dual rays.

        Args:
          linear_constraints: an optional LinearConstraint or iterator of
            LinearConstraint indicating what dual values to return. If not provided,
            ray_dual_values() returns a dictionary with the dual values for all
            linear constraints.

        Returns:
          The dual values from the first dual ray.

        Raises:
          ValueError: There are no dual rays.
          TypeError: Argument is not None, a LinearConstraint or an iterable of
            LinearConstraint.
          KeyError: LinearConstraint values requested for an invalid
            linear constraint (e.g. is not a LinearConstraint or is a linear
            constraint for another model).
        """
        if not self.has_dual_ray():
            raise ValueError("No dual ray available.")
        if linear_constraints is None:
            return self.dual_rays[0].dual_values
        if isinstance(linear_constraints, model.LinearConstraint):
            return self.dual_rays[0].dual_values[linear_constraints]
        if isinstance(linear_constraints, Iterable):
            return [self.dual_rays[0].dual_values[v] for v in linear_constraints]
        raise TypeError(
            "unsupported type in argument for "
            f"ray_dual_values: {type(linear_constraints).__name__!r}"
        )

    @overload
    def ray_reduced_costs(
        self, variables: None = ...
    ) -> Dict[model.Variable, float]: ...

    @overload
    def ray_reduced_costs(self, variables: model.Variable) -> float: ...

    @overload
    def ray_reduced_costs(self, variables: Iterable[model.Variable]) -> List[float]: ...

    def ray_reduced_costs(self, variables=None):
        """The reduced costs from the first dual ray.

        An error will be raised if there are no dual rays.

        Args:
          variables: an optional Variable or iterator of Variables indicating what
            reduced costs to return. If not provided, ray_reduced_costs() returns a
            dictionary with the reduced costs for all variables.

        Returns:
          The reduced costs from the first dual ray.

        Raises:
          ValueError: There are no dual rays.
          TypeError: Argument is not None, a Variable or an iterable of Variables.
          KeyError: Variable values requested for an invalid variable (e.g. is not a
            Variable or is a variable for another model).
        """
        if not self.has_dual_ray():
            raise ValueError("No dual ray available.")
        if variables is None:
            return self.dual_rays[0].reduced_costs
        if isinstance(variables, model.Variable):
            return self.dual_rays[0].reduced_costs[variables]
        if isinstance(variables, Iterable):
            return [self.dual_rays[0].reduced_costs[v] for v in variables]
        raise TypeError(
            "unsupported type in argument for "
            f"ray_reduced_costs: {type(variables).__name__!r}"
        )

    def has_basis(self) -> bool:
        """Indicates if the best solution has an associated basis.

        This is NOT guaranteed to be true when termination.reason is
        TerminationReason.Optimal. It also may be true even when the best solution
        does not have an associated primal feasible solution.

        Returns:
          True if the best solution has an associated basis.
        """
        if not self.solutions:
            return False
        return self.solutions[0].basis is not None

    @overload
    def constraint_status(
        self, linear_constraints: None = ...
    ) -> Dict[model.LinearConstraint, solution.BasisStatus]: ...

    @overload
    def constraint_status(
        self, linear_constraints: model.LinearConstraint
    ) -> solution.BasisStatus: ...

    @overload
    def constraint_status(
        self, linear_constraints: Iterable[model.LinearConstraint]
    ) -> List[solution.BasisStatus]: ...

    def constraint_status(self, linear_constraints=None):
        """The constraint basis status associated to the best solution.

        If there is at least one primal feasible solution, this corresponds to the
        basis associated to the best primal feasible solution. An error will
        be raised if the best solution does not have an associated basis.


        Args:
          linear_constraints: an optional LinearConstraint or iterator of
            LinearConstraint indicating what constraint statuses to return. If not
            provided, returns a dictionary with the constraint statuses for all
            linear constraints.

        Returns:
          The constraint basis status associated to the best solution.

        Raises:
          ValueError: The best solution does not have an associated basis.
          TypeError: Argument is not None, a LinearConstraint or an iterable of
            LinearConstraint.
          KeyError: LinearConstraint values requested for an invalid
            linear constraint (e.g. is not a LinearConstraint or is a linear
            constraint for another model).
        """
        if not self.has_basis():
            raise ValueError(_NO_BASIS_ERROR)
        assert self.solutions[0].basis is not None
        if linear_constraints is None:
            return self.solutions[0].basis.constraint_status
        if isinstance(linear_constraints, model.LinearConstraint):
            return self.solutions[0].basis.constraint_status[linear_constraints]
        if isinstance(linear_constraints, Iterable):
            return [
                self.solutions[0].basis.constraint_status[c] for c in linear_constraints
            ]
        raise TypeError(
            "unsupported type in argument for "
            f"constraint_status: {type(linear_constraints).__name__!r}"
        )

    @overload
    def variable_status(
        self, variables: None = ...
    ) -> Dict[model.Variable, solution.BasisStatus]: ...

    @overload
    def variable_status(self, variables: model.Variable) -> solution.BasisStatus: ...

    @overload
    def variable_status(
        self, variables: Iterable[model.Variable]
    ) -> List[solution.BasisStatus]: ...

    def variable_status(self, variables=None):
        """The variable basis status associated to the best solution.

        If there is at least one primal feasible solution, this corresponds to the
        basis associated to the best primal feasible solution. An error will
        be raised if the best solution does not have an associated basis.

        Args:
          variables: an optional Variable or iterator of Variables indicating what
            reduced costs to return. If not provided, variable_status() returns a
            dictionary with the reduced costs for all variables.

        Returns:
          The variable basis status associated to the best solution.

        Raises:
          ValueError: The best solution does not have an associated basis.
          TypeError: Argument is not None, a Variable or an iterable of Variables.
          KeyError: Variable values requested for an invalid variable (e.g. is not a
            Variable or is a variable for another model).
        """
        if not self.has_basis():
            raise ValueError(_NO_BASIS_ERROR)
        assert self.solutions[0].basis is not None
        if variables is None:
            return self.solutions[0].basis.variable_status
        if isinstance(variables, model.Variable):
            return self.solutions[0].basis.variable_status[variables]
        if isinstance(variables, Iterable):
            return [self.solutions[0].basis.variable_status[v] for v in variables]
        raise TypeError(
            "unsupported type in argument for "
            f"variable_status: {type(variables).__name__!r}"
        )

    def to_proto(self) -> result_pb2.SolveResultProto:
        """Returns an equivalent protocol buffer for a SolveResult."""
        proto = result_pb2.SolveResultProto(
            termination=self.termination.to_proto(),
            solutions=[s.to_proto() for s in self.solutions],
            primal_rays=[r.to_proto() for r in self.primal_rays],
            dual_rays=[r.to_proto() for r in self.dual_rays],
            solve_stats=self.solve_stats.to_proto(),
        )

        # Ensure that at most solver has solver specific output.
        existing_solver_specific_output = None

        def has_solver_specific_output(solver_name: str) -> None:
            nonlocal existing_solver_specific_output
            if existing_solver_specific_output is not None:
                raise ValueError(
                    "found solver specific output for both"
                    f" {existing_solver_specific_output} and {solver_name}"
                )
            existing_solver_specific_output = solver_name

        if self.gscip_specific_output is not None:
            has_solver_specific_output("gscip")
            proto.gscip_output.CopyFrom(self.gscip_specific_output)
        if self.osqp_specific_output is not None:
            has_solver_specific_output("osqp")
            proto.osqp_output.CopyFrom(self.osqp_specific_output)
        if self.pdlp_specific_output is not None:
            has_solver_specific_output("pdlp")
            proto.pdlp_output.CopyFrom(self.pdlp_specific_output)
        return proto


def _get_problem_status(
    result_proto: result_pb2.SolveResultProto,
) -> result_pb2.ProblemStatusProto:
    if result_proto.termination.HasField("problem_status"):
        return result_proto.termination.problem_status
    return result_proto.solve_stats.problem_status


def _get_objective_bounds(
    result_proto: result_pb2.SolveResultProto,
) -> result_pb2.ObjectiveBoundsProto:
    if result_proto.termination.HasField("objective_bounds"):
        return result_proto.termination.objective_bounds
    return result_pb2.ObjectiveBoundsProto(
        primal_bound=result_proto.solve_stats.best_primal_bound,
        dual_bound=result_proto.solve_stats.best_dual_bound,
    )


def _upgrade_termination(
    result_proto: result_pb2.SolveResultProto,
) -> result_pb2.TerminationProto:
    return result_pb2.TerminationProto(
        reason=result_proto.termination.reason,
        limit=result_proto.termination.limit,
        detail=result_proto.termination.detail,
        problem_status=_get_problem_status(result_proto),
        objective_bounds=_get_objective_bounds(result_proto),
    )


def parse_solve_result(
    proto: result_pb2.SolveResultProto, mod: model.Model
) -> SolveResult:
    """Returns a SolveResult equivalent to the input proto."""
    result = SolveResult()
    # TODO(b/290091715): change to parse_termination(proto.termination)
    # once solve_stats proto no longer has best_primal/dual_bound/problem_status
    # and problem_status/objective_bounds are guaranteed to be present in
    # termination proto.
    result.termination = parse_termination(_upgrade_termination(proto))
    result.solve_stats = parse_solve_stats(proto.solve_stats)
    for solution_proto in proto.solutions:
        result.solutions.append(solution.parse_solution(solution_proto, mod))
    for primal_ray_proto in proto.primal_rays:
        result.primal_rays.append(solution.parse_primal_ray(primal_ray_proto, mod))
    for dual_ray_proto in proto.dual_rays:
        result.dual_rays.append(solution.parse_dual_ray(dual_ray_proto, mod))
    if proto.HasField("gscip_output"):
        result.gscip_specific_output = proto.gscip_output
    elif proto.HasField("osqp_output"):
        result.osqp_specific_output = proto.osqp_output
    elif proto.HasField("pdlp_output"):
        result.pdlp_specific_output = proto.pdlp_output
    return result
