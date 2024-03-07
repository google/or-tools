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

"""Configures the solving of an optimization model."""

import dataclasses
import datetime
import enum
from typing import Dict, Optional

from ortools.pdlp import solvers_pb2 as pdlp_solvers_pb2
from ortools.glop import parameters_pb2 as glop_parameters_pb2
from ortools.gscip import gscip_pb2
from ortools.math_opt import parameters_pb2 as math_opt_parameters_pb2
from ortools.math_opt.solvers import glpk_pb2
from ortools.math_opt.solvers import gurobi_pb2
from ortools.math_opt.solvers import highs_pb2
from ortools.math_opt.solvers import osqp_pb2
from ortools.sat import sat_parameters_pb2


@enum.unique
class SolverType(enum.Enum):
    """The underlying solver to use.

    This must stay synchronized with math_opt_parameters_pb2.SolverTypeProto.

    Attributes:
      GSCIP: Solving Constraint Integer Programs (SCIP) solver (third party).
        Supports LP, MIP, and nonconvex integer quadratic problems. No dual data
        for LPs is returned though. Prefer GLOP for LPs.
      GUROBI: Gurobi solver (third party). Supports LP, MIP, and nonconvex integer
        quadratic problems. Generally the fastest option, but has special
        licensing, see go/gurobi-google for details.
      GLOP: Google's Glop linear solver. Supports LP with primal and dual simplex
        methods.
      CP_SAT: Google's CP-SAT solver. Supports problems where all variables are
        integer and bounded (or implied to be after presolve). Experimental
        support to rescale and discretize problems with continuous variables.
      MOE:begin_intracomment_strip
      PDLP: Google's PDLP solver. Supports LP and convex diagonal quadratic
        objectives. Uses first order methods rather than simplex. Can solve very
        large problems.
      MOE:end_intracomment_strip
      GLPK: GNU Linear Programming Kit (GLPK) (third party). Supports MIP and LP.
        Thread-safety: GLPK use thread-local storage for memory allocations. As a
        consequence when using IncrementalSolver, the user must make sure that
        instances are closed on the same thread as they are created or GLPK will
        crash. To do so, use `with` or call IncrementalSolver#close(). It seems OK
        to call IncrementalSolver#Solve() from another thread than the one used to
        create the Solver but it is not documented by GLPK and should be avoided.
        Of course these limitations do not apply to the solve() function that
        recreates a new GLPK problem in the calling thread and destroys before
        returning.  When solving a LP with the presolver, a solution (and the
        unbound rays) are only returned if an optimal solution has been found.
        Else nothing is returned. See glpk-5.0/doc/glpk.pdf page #40 available
        from glpk-5.0.tar.gz for details.
      OSQP: The Operator Splitting Quadratic Program (OSQP) solver (third party).
        Supports continuous problems with linear constraints and linear or convex
        quadratic objectives. Uses a first-order method.
      ECOS: The Embedded Conic Solver (ECOS) (third party). Supports LP and SOCP
        problems. Uses interior point methods (barrier).
      SCS: The Splitting Conic Solver (SCS) (third party). Supports LP and SOCP
        problems. Uses a first-order method.
      HIGHS: The HiGHS Solver (third party). Supports LP and MIP problems (convex
        QPs are unimplemented).
      SANTORINI: The Santorini Solver (first party). Supports MIP. Experimental,
        do not use in production.
    """

    GSCIP = math_opt_parameters_pb2.SOLVER_TYPE_GSCIP
    GUROBI = math_opt_parameters_pb2.SOLVER_TYPE_GUROBI
    GLOP = math_opt_parameters_pb2.SOLVER_TYPE_GLOP
    CP_SAT = math_opt_parameters_pb2.SOLVER_TYPE_CP_SAT
    PDLP = math_opt_parameters_pb2.SOLVER_TYPE_PDLP
    GLPK = math_opt_parameters_pb2.SOLVER_TYPE_GLPK
    OSQP = math_opt_parameters_pb2.SOLVER_TYPE_OSQP
    ECOS = math_opt_parameters_pb2.SOLVER_TYPE_ECOS
    SCS = math_opt_parameters_pb2.SOLVER_TYPE_SCS
    HIGHS = math_opt_parameters_pb2.SOLVER_TYPE_HIGHS
    SANTORINI = math_opt_parameters_pb2.SOLVER_TYPE_SANTORINI


def solver_type_from_proto(
    proto_value: math_opt_parameters_pb2.SolverTypeProto,
) -> Optional[SolverType]:
    if proto_value == math_opt_parameters_pb2.SOLVER_TYPE_UNSPECIFIED:
        return None
    return SolverType(proto_value)


def solver_type_to_proto(
    solver_type: Optional[SolverType],
) -> math_opt_parameters_pb2.SolverTypeProto:
    if solver_type is None:
        return math_opt_parameters_pb2.SOLVER_TYPE_UNSPECIFIED
    return solver_type.value


@enum.unique
class LPAlgorithm(enum.Enum):
    """Selects an algorithm for solving linear programs.

    Attributes:
      * UNPSECIFIED: No algorithm is selected.
      * PRIMAL_SIMPLEX: The (primal) simplex method. Typically can provide primal
        and dual solutions, primal/dual rays on primal/dual unbounded problems,
        and a basis.
      * DUAL_SIMPLEX: The dual simplex method. Typically can provide primal and
        dual solutions, primal/dual rays on primal/dual unbounded problems, and a
        basis.
      * BARRIER: The barrier method, also commonly called an interior point method
        (IPM). Can typically give both primal and dual solutions. Some
        implementations can also produce rays on unbounded/infeasible problems. A
        basis is not given unless the underlying solver does "crossover" and
        finishes with simplex.
      * FIRST_ORDER: An algorithm based around a first-order method. These will
        typically produce both primal and dual solutions, and potentially also
        certificates of primal and/or dual infeasibility. First-order methods
        typically will provide solutions with lower accuracy, so users should take
        care to set solution quality parameters (e.g., tolerances) and to validate
        solutions.

    This must stay synchronized with math_opt_parameters_pb2.LPAlgorithmProto.
    """

    PRIMAL_SIMPLEX = math_opt_parameters_pb2.LP_ALGORITHM_PRIMAL_SIMPLEX
    DUAL_SIMPLEX = math_opt_parameters_pb2.LP_ALGORITHM_DUAL_SIMPLEX
    BARRIER = math_opt_parameters_pb2.LP_ALGORITHM_BARRIER
    FIRST_ORDER = math_opt_parameters_pb2.LP_ALGORITHM_FIRST_ORDER


def lp_algorithm_from_proto(
    proto_value: math_opt_parameters_pb2.LPAlgorithmProto,
) -> Optional[LPAlgorithm]:
    if proto_value == math_opt_parameters_pb2.LP_ALGORITHM_UNSPECIFIED:
        return None
    return LPAlgorithm(proto_value)


def lp_algorithm_to_proto(
    lp_algorithm: Optional[LPAlgorithm],
) -> math_opt_parameters_pb2.LPAlgorithmProto:
    if lp_algorithm is None:
        return math_opt_parameters_pb2.LP_ALGORITHM_UNSPECIFIED
    return lp_algorithm.value


@enum.unique
class Emphasis(enum.Enum):
    """Effort level applied to an optional task while solving (see SolveParameters for use).

    - OFF: disable this task.
    - LOW: apply reduced effort.
    - MEDIUM: typically the default setting (unless the default is off).
    - HIGH: apply extra effort beyond MEDIUM.
    - VERY_HIGH: apply the maximum effort.

    Typically used as Optional[Emphasis]. It used to configure a solver feature as
    follows:
      * If a solver doesn't support the feature, only None will always be valid,
        any other setting will give an invalid argument error (some solvers may
        also accept OFF).
      * If the solver supports the feature:
        - When set to None, the underlying default is used.
        - When the feature cannot be turned off, OFF will produce an error.
        - If the feature is enabled by default, the solver default is typically
          mapped to MEDIUM.
        - If the feature is supported, LOW, MEDIUM, HIGH, and VERY HIGH will never
          give an error, and will map onto their best match.

    This must stay synchronized with math_opt_parameters_pb2.EmphasisProto.
    """

    OFF = math_opt_parameters_pb2.EMPHASIS_OFF
    LOW = math_opt_parameters_pb2.EMPHASIS_LOW
    MEDIUM = math_opt_parameters_pb2.EMPHASIS_MEDIUM
    HIGH = math_opt_parameters_pb2.EMPHASIS_HIGH
    VERY_HIGH = math_opt_parameters_pb2.EMPHASIS_VERY_HIGH


def emphasis_from_proto(
    proto_value: math_opt_parameters_pb2.EmphasisProto,
) -> Optional[Emphasis]:
    if proto_value == math_opt_parameters_pb2.EMPHASIS_UNSPECIFIED:
        return None
    return Emphasis(proto_value)


def emphasis_to_proto(
    emphasis: Optional[Emphasis],
) -> math_opt_parameters_pb2.EmphasisProto:
    if emphasis is None:
        return math_opt_parameters_pb2.EMPHASIS_UNSPECIFIED
    return emphasis.value


@dataclasses.dataclass
class GurobiParameters:
    """Gurobi specific parameters for solving.

    See https://www.gurobi.com/documentation/9.1/refman/parameters.html for a list
    of possible parameters.

    Example use:
      gurobi=GurobiParameters();
      gurobi.param_values["BarIterLimit"] = "10";

    With Gurobi, the order that parameters are applied can have an impact in rare
    situations. Parameters are applied in the following order:
     * LogToConsole is set from SolveParameters.enable_output.
     * Any common parameters not overwritten by GurobiParameters.
     * param_values in iteration order (insertion order).
    We set LogToConsole first because setting other parameters can generate
    output.
    """

    param_values: Dict[str, str] = dataclasses.field(default_factory=dict)

    def to_proto(self) -> gurobi_pb2.GurobiParametersProto:
        return gurobi_pb2.GurobiParametersProto(
            parameters=[
                gurobi_pb2.GurobiParametersProto.Parameter(name=key, value=val)
                for key, val in self.param_values.items()
            ]
        )


@dataclasses.dataclass
class GlpkParameters:
    """GLPK specific parameters for solving.

    Fields are optional to enable to capture user intention; if they set
    explicitly a value to then no generic solve parameters will overwrite this
    parameter. User specified solver specific parameters have priority on generic
    parameters.

    Attributes:
      compute_unbound_rays_if_possible: Compute the primal or dual unbound ray
        when the variable (structural or auxiliary) causing the unboundness is
        identified (see glp_get_unbnd_ray()). The unset value is equivalent to
        false. Rays are only available when solving linear programs, they are not
        available for MIPs. On top of that they are only available when using a
        simplex algorithm with the presolve disabled. A primal ray can only be
        built if the chosen LP algorithm is LPAlgorithm.PRIMAL_SIMPLEX. Same for a
        dual ray and LPAlgorithm.DUAL_SIMPLEX. The computation involves the basis
        factorization to be available which may lead to extra computations/errors.
    """

    compute_unbound_rays_if_possible: Optional[bool] = None

    def to_proto(self) -> glpk_pb2.GlpkParametersProto:
        return glpk_pb2.GlpkParametersProto(
            compute_unbound_rays_if_possible=self.compute_unbound_rays_if_possible
        )


@dataclasses.dataclass
class SolveParameters:
    """Parameters to control a single solve.

  If a value is set in both common and solver specific field (e.g. gscip), the
  solver specific setting is used.

  Solver specific parameters for solvers other than the one in use are ignored.

  Parameters that depends on the model (e.g. branching priority is set for each
  variable) are passed in ModelSolveParameters.

  See solve() and IncrementalSolver.solve() in solve.py for more details.

  Attributes:
    time_limit: The maximum time a solver should spend on the problem, or if
      None, then the time limit is infinite. This value is not a hard limit,
      solve time may slightly exceed this value. This parameter is always passed
      to the underlying solver, the solver default is not used.
    iteration_limit: Limit on the iterations of the underlying algorithm (e.g.
      simplex pivots). The specific behavior is dependent on the solver and
      algorithm used, but often can give a deterministic solve limit (further
      configuration may be needed, e.g. one thread). Typically supported by LP,
      QP, and MIP solvers, but for MIP solvers see also node_limit.
    node_limit: Limit on the number of subproblems solved in enumerative search
      (e.g. branch and bound). For many solvers this can be used to
      deterministically limit computation (further configuration may be needed,
      e.g. one thread). Typically for MIP solvers, see also iteration_limit.
    cutoff_limit: The solver stops early if it can prove there are no primal
      solutions at least as good as cutoff. On an early stop, the solver returns
      TerminationReason.NO_SOLUTION_FOUND and with Limit.CUTOFF and is not
      required to give any extra solution information. Has no effect on the
      return value if there is no early stop. It is recommended that you use a
      tolerance if you want solutions with objective exactly equal to cutoff to
      be returned. See the user guide for more details and a comparison with
      best_bound_limit.
    objective_limit: The solver stops early as soon as it finds a solution at
      least this good, with TerminationReason.FEASIBLE and Limit.OBJECTIVE.
    best_bound_limit: The solver stops early as soon as it proves the best bound
      is at least this good, with TerminationReason of FEASIBLE or
      NO_SOLUTION_FOUND and Limit.OBJECTIVE. See the user guide for more details
      and a comparison with cutoff_limit.
    solution_limit: The solver stops early after finding this many feasible
      solutions, with TerminationReason.FEASIBLE and Limit.SOLUTION. Must be
      greater than zero if set. It is often used get the solver to stop on the
      first feasible solution found. Note that there is no guarantee on the
      objective value for any of the returned solutions. Solvers will typically
      not return more solutions than the solution limit, but this is not
      enforced by MathOpt, see also b/214041169. Currently supported for Gurobi
      and SCIP, and for CP-SAT only with value 1.
    enable_output: If the solver should print out its log messages.
    threads: An integer >= 1, how many threads to use when solving.
    random_seed: Seed for the pseudo-random number generator in the underlying
      solver. Note that valid values depend on the actual solver:
        * Gurobi: [0:GRB_MAXINT] (which as of Gurobi 9.0 is 2x10^9).
        * GSCIP: [0:2147483647] (which is MAX_INT or kint32max or 2^31-1).
        * GLOP: [0:2147483647] (same as above).
      In all cases, the solver will receive a value equal to:
      MAX(0, MIN(MAX_VALID_VALUE_FOR_SOLVER, random_seed)).
    absolute_gap_tolerance: An absolute optimality tolerance (primarily) for MIP
      solvers. The absolute GAP is the absolute value of the difference between:
        * the objective value of the best feasible solution found,
        * the dual bound produced by the search.
      The solver can stop once the absolute GAP is at most
      absolute_gap_tolerance (when set), and return TerminationReason.OPTIMAL.
      Must be >= 0 if set. See also relative_gap_tolerance.
    relative_gap_tolerance: A relative optimality tolerance (primarily) for MIP
      solvers. The relative GAP is a normalized version of the absolute GAP
      (defined on absolute_gap_tolerance), where the normalization is
      solver-dependent, e.g. the absolute GAP divided by the objective value of
      the best feasible solution found. The solver can stop once the relative
      GAP is at most relative_gap_tolerance (when set), and return
      TerminationReason.OPTIMAL. Must be >= 0 if set. See also
      absolute_gap_tolerance.
    solution_pool_size: Maintain up to `solution_pool_size` solutions while
      searching. The solution pool generally has two functions:
        * For solvers that can return more than one solution, this limits how
          many solutions will be returned.
        * Some solvers may run heuristics using solutions from the solution
          pool, so changing this value may affect the algorithm's path.
      To force the solver to fill the solution pool, e.g. with the n best
      solutions, requires further, solver specific configuration.
    lp_algorithm: The algorithm for solving a linear program. If UNSPECIFIED,
      use the solver default algorithm. For problems that are not linear
      programs but where linear programming is a subroutine, solvers may use
      this value. E.g. MIP solvers will typically use this for the root LP solve
      only (and use dual simplex otherwise).
    presolve: Effort on simplifying the problem before starting the main
      algorithm (e.g. simplex).
    cuts: Effort on getting a stronger LP relaxation (MIP only). Note that in
      some solvers, disabling cuts may prevent callbacks from having a chance to
      add cuts at MIP_NODE.
    heuristics: Effort in finding feasible solutions beyond those encountered in
      the complete search procedure.
    scaling: Effort in rescaling the problem to improve numerical stability.
    gscip: GSCIP specific solve parameters.
    gurobi: Gurobi specific solve parameters.
    glop: Glop specific solve parameters.
    cp_sat: CP-SAT specific solve parameters.
    pdlp: PDLP specific solve parameters.
    osqp: OSQP specific solve parameters. Users should prefer the generic
      MathOpt parameters over OSQP-level parameters, when available: - Prefer
      SolveParameters.enable_output to OsqpSettingsProto.verbose. - Prefer
      SolveParameters.time_limit to OsqpSettingsProto.time_limit. - Prefer
      SolveParameters.iteration_limit to OsqpSettingsProto.iteration_limit. - If
      a less granular configuration is acceptable, prefer
      SolveParameters.scaling to OsqpSettingsProto.
    glpk: GLPK specific solve parameters.
    highs: HiGHS specific solve parameters.
  """  # fmt: skip

    time_limit: Optional[datetime.timedelta] = None
    iteration_limit: Optional[int] = None
    node_limit: Optional[int] = None
    cutoff_limit: Optional[float] = None
    objective_limit: Optional[float] = None
    best_bound_limit: Optional[float] = None
    solution_limit: Optional[int] = None
    enable_output: bool = False
    threads: Optional[int] = None
    random_seed: Optional[int] = None
    absolute_gap_tolerance: Optional[float] = None
    relative_gap_tolerance: Optional[float] = None
    solution_pool_size: Optional[int] = None
    lp_algorithm: Optional[LPAlgorithm] = None
    presolve: Optional[Emphasis] = None
    cuts: Optional[Emphasis] = None
    heuristics: Optional[Emphasis] = None
    scaling: Optional[Emphasis] = None
    gscip: gscip_pb2.GScipParameters = dataclasses.field(
        default_factory=gscip_pb2.GScipParameters
    )
    gurobi: GurobiParameters = dataclasses.field(default_factory=GurobiParameters)
    glop: glop_parameters_pb2.GlopParameters = dataclasses.field(
        default_factory=glop_parameters_pb2.GlopParameters
    )
    cp_sat: sat_parameters_pb2.SatParameters = dataclasses.field(
        default_factory=sat_parameters_pb2.SatParameters
    )
    pdlp: pdlp_solvers_pb2.PrimalDualHybridGradientParams = dataclasses.field(
        default_factory=pdlp_solvers_pb2.PrimalDualHybridGradientParams
    )
    osqp: osqp_pb2.OsqpSettingsProto = dataclasses.field(
        default_factory=osqp_pb2.OsqpSettingsProto
    )
    glpk: GlpkParameters = dataclasses.field(default_factory=GlpkParameters)
    highs: highs_pb2.HighsOptionsProto = dataclasses.field(
        default_factory=highs_pb2.HighsOptionsProto
    )

    def to_proto(self) -> math_opt_parameters_pb2.SolveParametersProto:
        """Returns a protocol buffer equivalent to this."""
        result = math_opt_parameters_pb2.SolveParametersProto(
            enable_output=self.enable_output,
            lp_algorithm=lp_algorithm_to_proto(self.lp_algorithm),
            presolve=emphasis_to_proto(self.presolve),
            cuts=emphasis_to_proto(self.cuts),
            heuristics=emphasis_to_proto(self.heuristics),
            scaling=emphasis_to_proto(self.scaling),
            gscip=self.gscip,
            gurobi=self.gurobi.to_proto(),
            glop=self.glop,
            cp_sat=self.cp_sat,
            pdlp=self.pdlp,
            osqp=self.osqp,
            glpk=self.glpk.to_proto(),
            highs=self.highs,
        )
        if self.time_limit is not None:
            result.time_limit.FromTimedelta(self.time_limit)
        if self.iteration_limit is not None:
            result.iteration_limit = self.iteration_limit
        if self.node_limit is not None:
            result.node_limit = self.node_limit
        if self.cutoff_limit is not None:
            result.cutoff_limit = self.cutoff_limit
        if self.objective_limit is not None:
            result.objective_limit = self.objective_limit
        if self.best_bound_limit is not None:
            result.best_bound_limit = self.best_bound_limit
        if self.solution_limit is not None:
            result.solution_limit = self.solution_limit
        if self.threads is not None:
            result.threads = self.threads
        if self.random_seed is not None:
            result.random_seed = self.random_seed
        if self.absolute_gap_tolerance is not None:
            result.absolute_gap_tolerance = self.absolute_gap_tolerance
        if self.relative_gap_tolerance is not None:
            result.relative_gap_tolerance = self.relative_gap_tolerance
        if self.solution_pool_size is not None:
            result.solution_pool_size = self.solution_pool_size
        return result
