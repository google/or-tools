// Copyright 2010-2025 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.ortools.mathopt;

import static com.google.ortools.util.ProtoDurationConversion.toJavaDuration;

import com.google.common.base.Preconditions;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.ortools.GScipOutput;
import com.google.ortools.mathopt.FeasibilityStatusProto;
import com.google.ortools.mathopt.LimitProto;
import com.google.ortools.mathopt.ObjectiveBoundsProto;
import com.google.ortools.mathopt.OsqpOutput;
import com.google.ortools.mathopt.ProblemStatusProto;
import com.google.ortools.mathopt.SolveResultProto;
import com.google.ortools.mathopt.SolveStatsProto;
import com.google.ortools.mathopt.TerminationProto;
import com.google.ortools.mathopt.TerminationReasonProto;
import java.time.Duration;
import java.util.EnumMap;
import java.util.Optional;
import org.jspecify.annotations.Nullable;

/**
 * The result of solving an optimization problem with {@link Solve#solve(Model, SolverType)}. This
 * class along with its nested classes are immutable.
 */
public final class SolveResult {
  private final Termination termination;
  private final SolveStats solveStats;
  private final ImmutableList<Solution> solutions;
  private final ImmutableList<Solution.PrimalRay> primalRays;
  private final ImmutableList<Solution.DualRay> dualRays;
  private final @Nullable GScipOutput gscipSolverSpecificOutput;
  private final @Nullable OsqpOutput osqpSolverSpecificOutput;
  private final SolveResultProto.@Nullable PdlpOutput pdlpSolverSpecificOutput;

  /**
   * Problem feasibility status as claimed by the solver (solver is not required to return a
   * certificate for the claim).
   */
  public enum FeasibilityStatus {
    /** Solver does not claim a status. */
    UNDETERMINED(FeasibilityStatusProto.FEASIBILITY_STATUS_UNDETERMINED),

    /** Solver claims the problem is feasible. */
    FEASIBLE(FeasibilityStatusProto.FEASIBILITY_STATUS_FEASIBLE),

    /** Solver claims the problem is infeasible. */
    INFEASIBLE(FeasibilityStatusProto.FEASIBILITY_STATUS_INFEASIBLE);

    private static class FeasibilityStatusProtoMap {
      private static final EnumMap<FeasibilityStatusProto, FeasibilityStatus> map =
          new EnumMap<>(FeasibilityStatusProto.class);
    }

    private final FeasibilityStatusProto proto;

    FeasibilityStatus(FeasibilityStatusProto proto) {
      FeasibilityStatusProtoMap.map.put(proto, this);
      this.proto = proto;
    }

    /** Returns the corresponding {@link FeasibilityStatusProto} for the enum. */
    public FeasibilityStatusProto toProto() {
      return this.proto;
    }

    /**
     * Returns a {@link FeasibilityStatus} form the given {@code proto}.
     *
     * @throws IllegalArgumentException if {@code proto} is unspecified.
     */
    public static FeasibilityStatus fromProto(FeasibilityStatusProto proto) {
      FeasibilityStatus status = FeasibilityStatusProtoMap.map.get(proto);
      if (status == null) {
        throw new IllegalArgumentException("feasibility status must be set");
      }
      return status;
    }
  }

  /**
   * Feasibility status of the primal problem and its dual (or the dual of a continuous relaxation)
   * as claimed by the solver. The solver is not required to return a certificate for the claim
   * (e.g. the solver may claim primal feasibility without returning a primal feasible solution).
   * This combined status gives a comprehensive description of a solver's claims about feasibility
   * and unboundedness of the solved problem.
   *
   * <p>For instance, a feasible status for primal and dual problems indicates the primal is
   * feasible and bounded and likely has an optimal solution (guaranteed for problems without
   * nonlinear constraints). On the other hand, a primal feasible and a dual infeasible status
   * indicates the primal problem is unbounded (i.e. has arbitrarily good solutions).
   *
   * <p>Note that a dual infeasible status by itself (i.e. accompanied by an undetermined primal
   * status) does not imply the primal problem is unbounded as we could have both problems be
   * infeasible. Also, while a primal and dual feasible status may imply the existence of an optimal
   * solution, it does not guarantee the solver has actually found such optimal solution.
   */
  public static final class ProblemStatus {
    private final FeasibilityStatus primalStatus;
    private final FeasibilityStatus dualStatus;
    private final boolean primalOrDualInfeasible;

    /** Status for the primal problem. */
    public FeasibilityStatus getPrimalStatus() {
      return primalStatus;
    }

    /** Status for the dual problem (or the dual of a continuous relaxation). */
    public FeasibilityStatus getDualStatus() {
      return dualStatus;
    }

    /**
     * If true, the solver claims the primal or dual problem is infeasible, but it does not know
     * which (or if both are infeasible). Can be true only when {@link #primalStatus} = {@link
     * #dualStatus} = {@link FeasibilityStatus#UNDETERMINED}. This extra information is often needed
     * when preprocessing determines there is no optimal solution to the problem (but can't
     * determine if it is due to infeasibility, unboundedness, or both).
     */
    public boolean getPrimalOrDualInfeasible() {
      return primalOrDualInfeasible;
    }

    /** Creates a new ProblemStatus with the provided statuses. */
    public ProblemStatus(FeasibilityStatus primalStatus, FeasibilityStatus dualStatus,
        boolean primalOrDualInfeasible) {
      this.primalStatus = primalStatus;
      this.dualStatus = dualStatus;
      this.primalOrDualInfeasible = primalOrDualInfeasible;
    }

    /**
     * Creates a new ProblemStatus from a ProblemStatusProto.
     *
     * @throws IllegalArgumentException if the {@link #primalStatus} or {@link #dualStatus} cannot
     *     be converted from the {@code proto}.
     */
    ProblemStatus(ProblemStatusProto problemStatusProto) {
      FeasibilityStatus primalStatus =
          FeasibilityStatus.fromProto(problemStatusProto.getPrimalStatus());
      this.primalStatus = primalStatus;

      FeasibilityStatus dualStatus =
          FeasibilityStatus.fromProto(problemStatusProto.getDualStatus());
      this.dualStatus = dualStatus;

      this.primalOrDualInfeasible = problemStatusProto.getPrimalOrDualInfeasible();
    }

    @Override
    public String toString() {
      return "primal_status: " + primalStatus + " dual_status: " + dualStatus
          + " primal_or_dual_infeasible: " + primalOrDualInfeasible;
    }
  }

  /** Bounds on the optimal objective value. */
  public static final class ObjectiveBounds {
    private final double primalBound;
    private final double dualBound;

    /**
     * Solver claims there exists a primal solution that is numerically feasible (i.e. feasible up
     * to the solvers tolerance), and whose objective value is {@link #primalBound}.
     *
     * <p>The optimal value is equal or better (smaller for min objectives and larger for max
     * objectives) than {@link #primalBound}, but only up to solver-tolerances.
     *
     * <p>See go/mathopt-objective-bounds for mode details.
     */
    public double getPrimalBound() {
      return primalBound;
    }

    /**
     * Solver claims there exists a dual solution that is numerically feasible (i.e. feasible up to
     * the solvers tolerance), and whose objective value is {@link #dualBound}.
     *
     * <p>For MIP solvers, the associated dual problem may be some continuous relaxation (e.g. LP
     * relaxation), but it is often an implicitly defined problem that is a complex consequence of
     * the solvers execution. For both continuous and MIP solvers, the optimal value is equal or
     * worse (larger for min objective and smaller for max objectives) than {@link #dualBound}, but
     * only up to solver-tolerances. Some continuous solvers provide a numerically safer dual bound
     * through solver's specific output (e.g. for PDLP,
     * pdlp_output.convergence_information.corrected_dual_objective).
     *
     * <p>See go/mathopt-objective-bounds for mode details.
     */
    public double getDualBound() {
      return dualBound;
    }

    /** Creates a new ObjectiveBounds from provided bounds. */
    public ObjectiveBounds(double primalBound, double dualBound) {
      this.primalBound = primalBound;
      this.dualBound = dualBound;
    }

    /** Creates a new ObjectiveBounds from a ObjectiveBoundsProto. */
    public ObjectiveBounds(ObjectiveBoundsProto proto) {
      this.primalBound = proto.getPrimalBound();
      this.dualBound = proto.getDualBound();
    }

    @Override
    public String toString() {
      return "primal_bound: " + primalBound + " dual_bound: " + dualBound;
    }
  }

  /** Stats of the solver. */
  public static final class SolveStats {
    private final Duration solveTime;
    private final long simplexIterations;
    private final long barrierIterations;
    private final long firstOrderIterations;
    private final long nodeCount;

    /**
     * Elapsed wall clock time as measured by MathOpt, roughly the time inside {@link
     * Solve#solve(Model, SolverType)}. Note: this does not include work done building the model or
     * crossing the language boundary.
     */
    public Duration getSolveTime() {
      return solveTime;
    }

    public long getSimplexIterations() {
      return simplexIterations;
    }

    public long getBarrierIterations() {
      return barrierIterations;
    }

    public long getFirstOrderIterations() {
      return firstOrderIterations;
    }

    public long getNodeCount() {
      return nodeCount;
    }

    /** Creates a new SolveStats from a SolveStatsProto. */
    SolveStats(SolveStatsProto proto) {
      this.solveTime = toJavaDuration(proto.getSolveTime());
      this.simplexIterations = proto.getSimplexIterations();
      this.barrierIterations = proto.getBarrierIterations();
      this.firstOrderIterations = proto.getFirstOrderIterations();
      this.nodeCount = proto.getNodeCount();
    }

    @Override
    public String toString() {
      return "solve_time: " + solveTime + " simplex_iterations: " + simplexIterations
          + " barrier_iterations: " + barrierIterations
          + " first_order_iterations: " + firstOrderIterations + " node_count: " + nodeCount;
    }
  }

  /** The reason a call to {@link Solve#solve(Model, SolverType)} terminates. */
  public enum TerminationReason {
    /** A provably optimal solution (up to numerical tolerances) has been found. */
    OPTIMAL(TerminationReasonProto.TERMINATION_REASON_OPTIMAL),

    /** The primal problem has no feasible solutions. */
    INFEASIBLE(TerminationReasonProto.TERMINATION_REASON_INFEASIBLE),

    /**
     * The primal problem is feasible and arbitrarily good solutions can be found along a primal
     * ray.
     */
    UNBOUNDED(TerminationReasonProto.TERMINATION_REASON_UNBOUNDED),

    /**
     * The primal problem is either infeasible or unbounded. More details on the problem status may
     * be available in {@link Termination#getProblemStatus()}. Note that Gurobi's unbounded status
     * may be mapped here as explained in go/mathop-solver-specific#gurobi-inf-or-unb.
     */
    INFEASIBLE_OR_UNBOUNDED(TerminationReasonProto.TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED),

    /**
     * The problem was solved to one of the criteria above ({@link #OPTIMAL}, {@link #INFEASIBLE},
     * {@link #UNBOUNDED}, or {@link #INFEASIBLE_OR_UNBOUNDED}), but one or more tolerances was not
     * met. Some primal/dual solutions/rays may be present, but either they will be slightly
     * infeasible, or (if the problem was nearly optimal) their may be a gap between the best
     * solution and best objective bound.
     *
     * <p>Users can still query primal/dual solutions/rays and solution stats, but they are
     * responsible for dealing with the numerical imprecision.
     */
    IMPRECISE(TerminationReasonProto.TERMINATION_REASON_IMPRECISE),

    /**
     * The optimizer reached some kind of limit and a primal feasible solution is returned. See
     * {@link Termination#getLimit()} for detailed description of the kind of limit that was
     * reached.
     */
    FEASIBLE(TerminationReasonProto.TERMINATION_REASON_FEASIBLE),

    /**
     * The optimizer reached some kind of limit and it did not find a primal feasible solution. See
     * {@link Termination#getLimit()} for detailed description of the kind of limit that was
     * reached.
     */
    NO_SOLUTION_FOUND(TerminationReasonProto.TERMINATION_REASON_NO_SOLUTION_FOUND),

    /**
     * The algorithm stopped because it encountered unrecoverable numerical error. No solution
     * information is available.
     */
    NUMERICAL_ERROR(TerminationReasonProto.TERMINATION_REASON_NUMERICAL_ERROR),

    /**
     * The algorithm stopped because of an error not covered by one of the statuses defined above.
     * No solution information is available.
     */
    OTHER_ERROR(TerminationReasonProto.TERMINATION_REASON_OTHER_ERROR);

    private static class TerminationReasonProtoMap {
      private static final EnumMap<TerminationReasonProto, TerminationReason> map =
          new EnumMap<>(TerminationReasonProto.class);
    }

    private final TerminationReasonProto proto;

    TerminationReason(TerminationReasonProto proto) {
      TerminationReasonProtoMap.map.put(proto, this);
      this.proto = proto;
    }

    /** Returns the corresponding {@link TerminationReasonProto} for the enum. */
    public TerminationReasonProto toProto() {
      return this.proto;
    }

    /**
     * Returns a {@link TerminationReason} form the given {@code proto}.
     *
     * @throws IllegalArgumentException if {@code proto} is unspecified.
     */
    public static TerminationReason fromProto(TerminationReasonProto proto) {
      TerminationReason reason = TerminationReasonProtoMap.map.get(proto);
      if (reason == null) {
        throw new IllegalArgumentException("termination reason must be set");
      }
      return reason;
    }
  }

  /**
   * The specific limit that was hit when a {@link Solve#solve(Model, SolverType)} stops early with
   * {@link TerminationReason#FEASIBLE} or {@link TerminationReason#NO_SOLUTION_FOUND}.
   */
  public enum Limit {
    /**
     * Used if the underlying solver cannot determine which limit was reached, or as a null value
     * when we terminated not from a limit (e.g. {@link TerminationReason#OPTIMAL}).
     */
    UNDETERMINED(LimitProto.LIMIT_UNDETERMINED),

    /**
     * An iterative algorithm stopped after conducting the maximum number of iterations (e.g.
     * simplex or barrier iterations).
     */
    ITERATION(LimitProto.LIMIT_ITERATION),

    /** The algorithm stopped after a user-specified computation time. */
    TIME(LimitProto.LIMIT_TIME),

    /**
     * A branch-and-bound algorithm stopped because it explored a maximum number of nodes in the
     * branch-and-bound tree.
     */
    NODE(LimitProto.LIMIT_NODE),

    /**
     * The algorithm stopped because it found the required number of solutions. This is often used
     * in MIPs to get the solver to return the first feasible solution it encounters.
     */
    SOLUTION(LimitProto.LIMIT_SOLUTION),

    /** The algorithm stopped because it ran out of memory. */
    MEMORY(LimitProto.LIMIT_MEMORY),

    /**
     * The solver was run with a cutoff (e.g. {@link SolverParametersProto#cutoffLimit} was set) on
     * the objective, indicating that the user did not want any solution worse than the cutoff, and
     * the solver concluded there were no solutions at least as good as the cutoff. Typically no
     * further solution information is provided.
     */
    CUTOFF(LimitProto.LIMIT_CUTOFF),

    /**
     * The algorithm stopped because it found a solution better than a minimum limit set by the
     * user.
     */
    OBJECTIVE(LimitProto.LIMIT_OBJECTIVE),

    /** The algorithm stopped because the norm of an iterate became too large. */
    NORM(LimitProto.LIMIT_NORM),

    /** The algorithm stopped because of an interrupt signal or a user interrupt request. */
    INTERRUPTED(LimitProto.LIMIT_INTERRUPTED),

    /**
     * The algorithm stopped because it was unable to continue making progress towards the solution.
     */
    SLOW_PROGRESS(LimitProto.LIMIT_SLOW_PROGRESS),

    /**
     * The algorithm stopped due to a limit not covered by one of the above. Note that {@link
     * #UNDETERMINED} is used when the reason cannot be determined, and {@link #OTHER} is used when
     * the reason is known but does not fit into any of the above alternatives.
     */
    OTHER(LimitProto.LIMIT_OTHER);

    private static class LimitProtoMap {
      private static final EnumMap<LimitProto, Limit> map = new EnumMap<>(LimitProto.class);
    }

    private final LimitProto proto;

    Limit(LimitProto proto) {
      LimitProtoMap.map.put(proto, this);
      this.proto = proto;
    }

    /** Returns the corresponding {@link LimitProto} for the enum. */
    public LimitProto toProto() {
      return this.proto;
    }

    /** Returns a {@link Limit} form the given {@code proto} if present. */
    public static Optional<Limit> optionalFromProto(LimitProto proto) {
      return Optional.ofNullable(LimitProtoMap.map.get(proto));
    }

    /**
     * Returns a {@link Limit} form the given {@code proto}.
     *
     * @throws IllegalArgumentException if {@code proto} is unspecified.
     */
    public static Limit fromProto(LimitProto proto) {
      Limit limit = LimitProtoMap.map.get(proto);
      if (limit == null) {
        throw new IllegalArgumentException("limit must be set");
      }
      return limit;
    }
  }

  /** All information regarding why a call to {@link Solve#solve(Model, SolverType)} terminated. */
  public static class Termination {
    /**
     * Additional information in {@link #limit} when value is {@link TerminationReason#FEASIBLE} or
     * {@link TerminationReason#NO_SOLUTION_FOUND}, see {@link #limit} for details.
     */
    private final TerminationReason reason;

    /**
     * A Termination within a SolveResult returned by {@link Solve#solve(Model, SolverType)}
     * satisfies some additional invariants:
     *
     * <ul>
     *   <li>limit is set iff {@link #reason} is {@link TerminationReason#FEASIBLE} or {@link
     *       TerminationReason#NO_SOLUTION_FOUND}.
     *   <li>if the limit is {@link Limit#CUTOFF}, the termination reason will be {@link
     *       TerminationReason#NO_SOLUTION_FOUND}.
     * </ul>
     */
    private final Optional<Limit> limit;

    /**
     * Additional typically solver specific information about termination. Not all solvers can
     * always determine the limit which caused termination. {@link Limit#UNDETERMINED} is used when
     * the cause cannot be determined.
     */
    private final String detail;

    /** Feasibility statuses for primal and dual problems. */
    private final ProblemStatus problemStatus;

    /** Bounds on the optimal objective value. */
    private final ObjectiveBounds objectiveBounds;

    public TerminationReason getReason() {
      return reason;
    }

    public Optional<Limit> getLimit() {
      return limit;
    }

    public String getDetail() {
      return detail;
    }

    public ProblemStatus getProblemStatus() {
      return problemStatus;
    }

    public ObjectiveBounds getObjectiveBounds() {
      return objectiveBounds;
    }

    /**
     * Returns true if a limit was reached (i.e. if {@link #reason} is {@link
     * TerminationReason#FEASIBLE} or {@link TerminationReason#NO_SOLUTION_FOUND}, and limit is not
     * empty).
     */
    public boolean limitReached() {
      return reason.equals(TerminationReason.FEASIBLE)
          || reason.equals(TerminationReason.NO_SOLUTION_FOUND);
    }

    /** Creates a new Termination from provided arguments. */
    public Termination(TerminationReason reason, Limit limit, String detail,
        ProblemStatus problemStatus, ObjectiveBounds objectiveBounds) {
      this.reason = reason;
      this.limit = Optional.of(limit);
      this.detail = detail;
      this.problemStatus = problemStatus;
      this.objectiveBounds = objectiveBounds;
    }

    /**
     * Creates a new Termination from the Termination proto.
     *
     * @throws IllegalArgumentException is {@link #reason} or {@link #problemStatus} cannot be
     *     converted from {@code proto}.
     */
    public Termination(TerminationProto proto) {
      this.reason = TerminationReason.fromProto(proto.getReason());
      this.limit = Limit.optionalFromProto(proto.getLimit());
      this.detail = proto.getDetail();
      this.problemStatus = new ProblemStatus(proto.getProblemStatus());
      this.objectiveBounds = new ObjectiveBounds(proto.getObjectiveBounds());
    }

    @Override
    public String toString() {
      return "reason: " + reason + " limit: " + limit + " detail: \""
          + detail // TODO: b/332607722 - escape this string.
          + "\" problem_status: {" + problemStatus + "} objective_bounds: {" + objectiveBounds
          + "}";
    }
  }

  /** The reason the solver stopped. */
  public Termination getTermination() {
    return termination;
  }

  /** Statistics on the solve process, e.g. running time, iterations. */
  public SolveStats getSolveStats() {
    return solveStats;
  }

  /**
   * Basic solutions use, as of Nov 2021:
   *
   * <ul>
   *   <li>All convex optimization solvers (LP, convex QP) return only one solution as a primal dual
   *       pair.
   *   <li>Only MI(Q)P solvers return more than one solution. MIP solvers do not return any dual
   *       information, or primal infeasible solutions. Solutions are returned in order of best
   *       primal objective first. Gurobi solves nonconvex QP (integer or continuous) as MIQP.
   * </ul>
   *
   * <p>The general contract for the order of solutions that future solvers should implement is to
   * order by:
   *
   * <ul>
   *   <li>1. The solutions with a primal feasible solution, ordered by best primal objective first.
   *   <li>2. The solutions with a dual feasible solution, ordered by best dual objective (unknown
   *       dual objective is worst).
   *   <li>3. All remaining solutions can be returned in any order.
   * </ul>
   */
  public ImmutableList<Solution> getSolutions() {
    return solutions;
  }

  /**
   * Directions of unbounded primal improvement, or equivalently, dual infeasibility certificates.
   * Typically provided for {@link TerminationReason#UNBOUNDED} and {@link
   * TerminationReason#INFEASIBLE_OR_UNBOUNDED}.
   */
  public ImmutableList<Solution.PrimalRay> getPrimalRays() {
    return primalRays;
  }

  /**
   * Directions of unbounded dual improvement, or equivalently, primal infeasibility certificates.
   * Typically provided for {@link TerminationReason#INFEASIBLE}.
   */
  public ImmutableList<Solution.DualRay> getDualRays() {
    return dualRays;
  }

  /** Solver specific output from Gscip. Only populated if Gscip is used. */
  public @Nullable GScipOutput getGscipSolverSpecificOutput() {
    return gscipSolverSpecificOutput;
  }

  /** Solver specific output form OSQP. Only populated if OSQP is used. */
  public @Nullable OsqpOutput getOsqpSolverSpecificOutput() {
    return osqpSolverSpecificOutput;
  }

  /** Solver specific output form PDLP. Only populated if PDLP is used. */
  public SolveResultProto.@Nullable PdlpOutput getPdlpSolverSpecificOutput() {
    return pdlpSolverSpecificOutput;
  }

  public Duration getSolveTime() {
    return solveStats.getSolveTime();
  }

  /**
   * A primal bound on the optimal objective value as described in {@link ObjectiveBounds}. Will
   * return a valid (possibly infinite) bound even if no primal feasible solutions are available.
   */
  public double getPrimalBound() {
    return termination.objectiveBounds.getPrimalBound();
  }

  /**
   * A dual bound on the optimal objective value as described in {@link ObjectiveBounds}. Will
   * return a valid (possibly infinite) bound even if no dual feasible solutions are available.
   */
  public double getDualBound() {
    return termination.objectiveBounds.getDualBound();
  }

  /**
   * Indicates if at least one primal feasible solution is available.
   *
   * <p>For SolveResults generated by calling {@link Solve#solve(Model, SolverType)}, when {@link
   * Termination#reason} is {@link TerminationReason#OPTIMAL} or {@link TerminationReason#FEASIBLE},
   * this is guaranteed to be true and need not be checked. SolveResult objects generated directly
   * from a proto need not have this property.
   */
  public boolean hasPrimalFeasibleSolution() {
    return !solutions.isEmpty() && solutions.get(0).getPrimalSolution().isPresent()
        && (solutions.get(0).getPrimalSolution().get().getFeasibilityStatus()
            == SolutionStatus.FEASIBLE);
  }

  /**
   * The objective value of the best primal feasible solution.
   *
   * <p>{@link #getPrimalBound()} above is guaranteed to be at least as good (larger or equal for
   * max problems and smaller or equal for min problems) as {@link #getObjectiveValue()} and will
   * never throw an exception, so it may be preferable in some cases. Note that {@link
   * #getPrimalBound()} could be better than {@link #getObjectiveValue()} even for optimal
   * terminations, but on such optimal termination, both should satisfy the optimality tolerances.
   *
   * @throws IllegalStateException if there are no primal feasible solutions.
   */
  public double getObjectiveValue() {
    Preconditions.checkState(hasPrimalFeasibleSolution());
    return solutions.get(0).getPrimalSolution().get().getObjectiveValue();
  }

  /**
   * A bound on the best possible objective.
   *
   * <p>{@link #getBestObjectiveBound()} is always equal to {@link #getDualBound()} , so they can be
   * used interchangeably.
   */
  public double getBestObjectiveBound() {
    return termination.objectiveBounds.getDualBound();
  }

  /**
   * The variable values from the best primal feasible solution.
   *
   * @throws IllegalStateException if there are no primal feasible solutions.
   */
  public ImmutableMap<Variable, Double> getVariableValues() {
    Preconditions.checkState(hasPrimalFeasibleSolution());
    return solutions.get(0).getPrimalSolution().get().getVariableValues();
  }

  /** Returns true only if the problem has been shown to be feasible and bounded. */
  public boolean isBounded() {
    return termination.problemStatus.primalStatus.equals(FeasibilityStatus.FEASIBLE)
        && termination.problemStatus.dualStatus.equals(FeasibilityStatus.FEASIBLE);
  }

  /**
   * Indicates if at least one primal ray is available.
   *
   * <p>This is NOT guaranteed to be true when {@link Termination#reason} is {@link
   * TerminationReason#UNBOUNDED} or {@link TerminationReason#INFEASIBLE_OR_UNBOUNDED}.
   */
  public boolean hasRay() {
    return !primalRays.isEmpty();
  }

  /**
   * The variable values form the first primal ray.
   *
   * @throws IllegalStateException if there are no primal rays.
   */
  public ImmutableMap<Variable, Double> getRayVariableValues() {
    Preconditions.checkState(hasRay());
    return primalRays.get(0).getVariableValues();
  }

  /**
   * Indicates if the best solution has an associated dual feasible solution.
   *
   * <p>This is NOT guaranteed to be true when {@link Termination#reason} is {@link
   * TerminationReason#OPTIMAL}. It also may be true even when the best solution does not have an
   * associated primal feasible solution.
   */
  public boolean hasDualFeasibleSolution() {
    return !solutions.isEmpty() && solutions.get(0).getDualSolution().isPresent()
        && (solutions.get(0).getDualSolution().get().getFeasibilityStatus()
            == SolutionStatus.FEASIBLE);
  }

  /**
   * The dual values associated to the best solution.
   *
   * <p>If there is at least one primal feasible solution, this corresponds to the dual values
   * associated to the best primal feasible solution.
   *
   * @throws IllegalStateException if the best solution does not have an associated dual feasible
   *     solution.
   */
  public ImmutableMap<LinearConstraint, Double> getDualValues() {
    Preconditions.checkState(hasDualFeasibleSolution());
    return solutions.get(0).getDualSolution().get().getDualValues();
  }

  /**
   * The reduced costs associated to the best solution.
   *
   * <p>If there is at least one primal feasible solution, this corresponds to the reduced costs
   * associated to the best primal fesaible solution.
   *
   * @throws IllegalStateException if the best solution does not ahve an associated dual feasible
   *     solution.
   */
  public ImmutableMap<Variable, Double> getReducedCosts() {
    Preconditions.checkState(hasDualFeasibleSolution());
    return solutions.get(0).getDualSolution().get().getReducedCosts();
  }

  /**
   * Indicates if at least one dual ray is available.
   *
   * <p>This is NOT guaranteed to be true when {@link Termination#reason} is {@link
   * TerminationReason#INFEASIBLE}.
   */
  public boolean hasDualRay() {
    return !dualRays.isEmpty();
  }

  /**
   * The dual values from the first dual ray.
   *
   * @throws IllegalStateException if there are no dual rays.
   */
  public ImmutableMap<LinearConstraint, Double> getRayDualValues() {
    Preconditions.checkState(hasDualRay());
    return dualRays.get(0).getDualValues();
  }

  /**
   * The reduced costs from the first dual ray.
   *
   * @throws IllegalStateException if there are no dual rays.
   */
  public ImmutableMap<Variable, Double> getRayReducedCosts() {
    Preconditions.checkState(hasDualRay());
    return dualRays.get(0).getReducedCosts();
  }

  /** Indicates if the best solution has an associated basis. */
  public boolean hasBasis() {
    return !solutions.isEmpty() && solutions.get(0).getBasis().isPresent();
  }

  /**
   * The constraint basis status for the best solution.
   *
   * @throws IllegalStateException if the best solution does not have an associated basis.
   */
  public ImmutableMap<LinearConstraint, Basis.BasisStatus> getConstraintStatus() {
    Preconditions.checkState(hasBasis());
    return solutions.get(0).getBasis().get().getConstraintStatus();
  }

  /**
   * The variable basis status for the best solution.
   *
   * @throws IllegalStateException if the best solution does not have an associated basis.
   */
  public ImmutableMap<Variable, Basis.BasisStatus> getVariableStatus() {
    Preconditions.checkState(hasBasis());
    return solutions.get(0).getBasis().get().getVariableStatus();
  }

  private ProblemStatusProto getProblemStatusProto(SolveResultProto proto) {
    if (proto.getTermination().hasProblemStatus()) {
      return proto.getTermination().getProblemStatus();
    }
    return proto.getSolveStats().getProblemStatus();
  }

  private ObjectiveBoundsProto getObjectiveBoundsProto(SolveResultProto proto) {
    if (proto.getTermination().hasObjectiveBounds()) {
      return proto.getTermination().getObjectiveBounds();
    }
    return ObjectiveBoundsProto.newBuilder()
        .setPrimalBound(proto.getSolveStats().getBestPrimalBound())
        .setDualBound(proto.getSolveStats().getBestDualBound())
        .build();
  }

  private TerminationProto upgradeTermination(SolveResultProto proto) {
    return TerminationProto.newBuilder()
        .setReason(proto.getTermination().getReason())
        .setLimit(proto.getTermination().getLimit())
        .setDetail(proto.getTermination().getDetail())
        .setProblemStatus(getProblemStatusProto(proto))
        .setObjectiveBounds(getObjectiveBoundsProto(proto))
        .build();
  }

  /**
   * Creates a new SolveResult from a SolveResult.
   *
   * <p>Note: The invariants of this class specified in the comments are not validated during this
   * conversion. The C++ function ValidateResult() is called automatically if the model is being
   * solved locally. Users who are reading a solution from disk, solving remotely, or getting their
   * SolveResultProto (or SolveResult) by any other means are encouraged to validate the result
   * themselves or not rely on the strong guarantees of the ValidateResult().
   *
   * @param proto is a SolveResultProto that is converted to a new SolveResult.
   * @param model is the optimization problem that was solved to generate {@code proto} which
   *     defines the {@link Variable} and {@link LinearConstraint} objects that this SolveResult
   *     will be expressed in terms.
   * @throws IllegalArgumentException if any solutions, rays, {@link #termination}, {@link
   *     #solveStats}, or solver specific outputs cannot be read from the {@code proto}. See the
   *     FromProto() methods for each of these classes for more details.
   */
  public SolveResult(Model model, SolveResultProto proto) {
    // TODO(b/290091715): Remove upgradeTermination once solve_stats proto no longer has
    // best_primal/dual_bound/problem_status and problem_status/objective_bounds are guaranteed to
    // be present in termination proto.
    this.termination = new Termination(upgradeTermination(proto));
    this.solveStats = new SolveStats(proto.getSolveStats());

    ImmutableList.Builder<Solution> solutionsBuilder = ImmutableList.builder();
    for (int i = 0; i < proto.getSolutionsCount(); i++) {
      solutionsBuilder.add(new Solution(model, proto.getSolutions(i)));
    }
    this.solutions = solutionsBuilder.build();

    ImmutableList.Builder<Solution.PrimalRay> primalRaysBuilder = ImmutableList.builder();
    for (int i = 0; i < proto.getPrimalRaysCount(); i++) {
      primalRaysBuilder.add(new Solution.PrimalRay(model, proto.getPrimalRays(i)));
    }
    this.primalRays = primalRaysBuilder.build();

    ImmutableList.Builder<Solution.DualRay> dualRaysBuilder = ImmutableList.builder();
    for (int i = 0; i < proto.getDualRaysCount(); i++) {
      dualRaysBuilder.add(new Solution.DualRay(model, proto.getDualRays(i)));
    }
    this.dualRays = dualRaysBuilder.build();

    GScipOutput gscipOutput = null;
    OsqpOutput osqpOutput = null;
    SolveResultProto.PdlpOutput pdlpOutput = null;
    switch (proto.getSolverSpecificOutputCase()) {
      case GSCIP_OUTPUT -> gscipOutput = proto.getGscipOutput();
      case OSQP_OUTPUT -> osqpOutput = proto.getOsqpOutput();
      case PDLP_OUTPUT -> pdlpOutput = proto.getPdlpOutput();
      case SOLVERSPECIFICOUTPUT_NOT_SET -> {
      }
      default ->
        throw new IllegalArgumentException("unexpected value of solver_specific_output_case "
            + proto.getSolverSpecificOutputCase());
    }
    this.gscipSolverSpecificOutput = gscipOutput;
    this.osqpSolverSpecificOutput = osqpOutput;
    this.pdlpSolverSpecificOutput = pdlpOutput;
  }
}
