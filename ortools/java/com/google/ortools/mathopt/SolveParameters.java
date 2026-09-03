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
import static com.google.ortools.util.ProtoDurationConversion.toProtoDuration;

import com.google.ortools.GScipParameters;
import com.google.ortools.glop.GlopParameters;
import com.google.ortools.mathopt.EmphasisProto;
import com.google.ortools.mathopt.GlpkParametersProto;
import com.google.ortools.mathopt.GurobiParametersProto;
import com.google.ortools.mathopt.HighsOptionsProto;
import com.google.ortools.mathopt.LPAlgorithmProto;
import com.google.ortools.mathopt.OsqpSettingsProto;
import com.google.ortools.mathopt.SolveParametersProto;
import com.google.ortools.mathopt.XpressParametersProto;
import com.google.ortools.pdlp.PrimalDualHybridGradientParams;
import com.google.ortools.sat.SatParameters;
import java.time.Duration;
import java.util.EnumMap;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Optional;
import java.util.OptionalDouble;
import java.util.OptionalInt;
import java.util.OptionalLong;

/**
 * Parameters to control a single solve.
 *
 * <p>Contains both parameters common to all solver, e.g. {@link #setTimeLimit(Duration)}, and
 * parameters for a specific solver, e.g. {@link #setGscip(GScipParameters)}. If a value is set in
 * both common and solver specific fields, the solver specific setting is used.
 *
 * <p>The common parameters that are optional and unset indicate that the solver default is used.
 *
 * <p>Solver specific parameters for solvers other than the one in use are ignored.
 *
 * <p>See {@link ModelSolveParameters} for parameters that depend on the model (e.g. solution hint).
 */
public final class SolveParameters {
  private boolean enableOutput = false;
  private Optional<Duration> timeLimit = Optional.empty();
  private OptionalLong iterationLimit = OptionalLong.empty();
  private OptionalLong nodeLimit = OptionalLong.empty();
  private OptionalDouble cutoffLimit = OptionalDouble.empty();
  private OptionalDouble objectiveLimit = OptionalDouble.empty();
  private OptionalDouble bestBoundLimit = OptionalDouble.empty();
  private OptionalInt solutionLimit = OptionalInt.empty();
  private OptionalInt threads = OptionalInt.empty();
  private OptionalInt randomSeed = OptionalInt.empty();
  private OptionalDouble absoluteGapTolerance = OptionalDouble.empty();
  private OptionalDouble relativeGapTolerance = OptionalDouble.empty();
  private OptionalInt solutionPoolSize = OptionalInt.empty();
  private Optional<LPAlgorithm> lpAlgorithm = Optional.empty();
  private Optional<Emphasis> presolve = Optional.empty();
  private Optional<Emphasis> cuts = Optional.empty();
  private Optional<Emphasis> heuristics = Optional.empty();
  private Optional<Emphasis> scaling = Optional.empty();
  private Optional<GScipParameters> gscip = Optional.empty();
  private Optional<GurobiParameters> gurobi = Optional.empty();
  private Optional<GlopParameters> glop = Optional.empty();
  private Optional<SatParameters> cpSat = Optional.empty();
  private Optional<PrimalDualHybridGradientParams> pdlp = Optional.empty();
  private Optional<OsqpSettingsProto> osqp = Optional.empty();
  private Optional<GlpkParameters> glpk = Optional.empty();
  private Optional<HighsOptionsProto> highs = Optional.empty();
  private Optional<XpressParameters> xpress = Optional.empty();

  /** Selects an algorithm for solver linear programs. */
  public enum LPAlgorithm {
    /**
     * The (primal) simplex method. Typically can provide primal and dual solutions, primal/dual
     * rays on primal/dual unbounded problems, and a basis.
     */
    PRIMAL_SIMPLEX(LPAlgorithmProto.LP_ALGORITHM_PRIMAL_SIMPLEX),

    /**
     * The dual simplex method. Typically can provide primal and dual solutions, primal/dual rays on
     * primal/dual unbounded problems, and a basis.
     */
    DUAL_SIMPLEX(LPAlgorithmProto.LP_ALGORITHM_DUAL_SIMPLEX),

    /**
     * The barrier method, also commonly called an interior point method (IPM). Can typically give
     * both primal and dual solutions. Some implementations can also produce rays on
     * unbounded/infeasible problems. A basis is not given unless the underlying solver does
     * "crossover" and finishes with simplex.
     */
    BARRIER(LPAlgorithmProto.LP_ALGORITHM_BARRIER),

    /**
     * An algorithm based around a first-order method. These will typically produce both primal and
     * dual solutions, and potentially also certificates of primal and/or dual infeasibility.
     * First-order methods typically will provide solutions with lower accuracy, so users should
     * take care to set solution quality parameters (e.g., tolerances) and to validate solutions.
     */
    FIRST_ORDER(LPAlgorithmProto.LP_ALGORITHM_FIRST_ORDER);

    private static class LPAlgorithmProtoMap {
      private static final EnumMap<LPAlgorithmProto, LPAlgorithm> map =
          new EnumMap<>(LPAlgorithmProto.class);
    }

    private final LPAlgorithmProto proto;

    LPAlgorithm(LPAlgorithmProto proto) {
      LPAlgorithmProtoMap.map.put(proto, this);
      this.proto = proto;
    }

    /** Returns the corresponding {@link LPAlgorithmProto} for the enum. */
    public LPAlgorithmProto toProto() {
      return this.proto;
    }

    /** Returns a {@link LPAlgorithm} form the given {@code proto}, if specified. */
    public static Optional<LPAlgorithm> fromProto(LPAlgorithmProto proto) {
      return Optional.ofNullable(LPAlgorithmProtoMap.map.get(proto));
    }
  }

  /**
   * Effort level applied to an optional task while solving.
   *
   * <p>Typically used as a {@code Optional<Emphasis>}. It's used to configure a solver feature as
   * follows:
   *
   * <ul>
   *   <li>If a solver doesn't support the feature, only absent will always be valid. Any other
   *       setting will throw an illegal argument exception.
   *   <li>If the solver supports the feature:
   *       <ul>
   *         <li>When absent, the underlying default is used.
   *         <li>When the feature cannot be turned off, {@link #OFF} will return an error.
   *         <li>If the feature is enabled by default, the solver default is typically mapped to
   *             {@link #MEDIUM}.
   *         <li>If the feature is supported, {@link #LOW}, {@link #MEDIUM}, {@link #HIGH}, and
   *             {@link #VERY_HIGH} will never give an error and will map onto their best match.
   *       </ul>
   * </ul>
   */
  public enum Emphasis {
    OFF(EmphasisProto.EMPHASIS_OFF),
    LOW(EmphasisProto.EMPHASIS_LOW),
    MEDIUM(EmphasisProto.EMPHASIS_MEDIUM),
    HIGH(EmphasisProto.EMPHASIS_HIGH),
    VERY_HIGH(EmphasisProto.EMPHASIS_VERY_HIGH);

    private static class EmphasisProtoMap {
      private static final EnumMap<EmphasisProto, Emphasis> map =
          new EnumMap<>(EmphasisProto.class);
    }

    private final EmphasisProto proto;

    Emphasis(EmphasisProto proto) {
      EmphasisProtoMap.map.put(proto, this);
      this.proto = proto;
    }

    /** Returns the corresponding {@link EmphasisProto} for the enum. */
    public EmphasisProto toProto() {
      return this.proto;
    }

    /** Returns a {@link Emphasis} form the given {@code proto}, if specified. */
    public static Optional<Emphasis> fromProto(EmphasisProto proto) {
      return Optional.ofNullable(EmphasisProtoMap.map.get(proto));
    }
  }

  /**
   * Gurobi specific parameters for solver. See <a
   * href="https://www.gurobi.com/documentation/9.1/refman/parameters.html">Gurobi's
   * documentation</a> for a list of possible parameters.
   *
   * <p>Example use:
   *
   * <pre>{@code
   * GurobiParameters gurobi;
   * gurobi.addParamValue("BarIterLimit", "10");
   * }</pre>
   *
   * <p>With Gurobi, the order that parameters are applied can have an impact in rare situations.
   * Parameters are applied in the following order:
   *
   * <ul>
   *   <li>LogToConsole is set from {@link SolveParameters#setEnableOutput(boolean)}.
   *   <li>Any common parameters not overwritten by {@link GurobiParameters}.
   *   <li>{@link #addParamValue} in iteration order (insertion order).
   * </ul>
   *
   * <p>We set LogToConsole first because setting other parameters can generate output.
   */
  public static final class GurobiParameters {
    private final LinkedHashMap<String, String> paramValues = new LinkedHashMap<>();

    /** Adds the parameter name-value pairs to set in insertion order. */
    public GurobiParameters addParamValue(String name, String value) {
      paramValues.put(name, value);
      return this;
    }

    public Map<String, String> getParamValues() {
      return this.paramValues;
    }

    /** Returns the corresponding {@link GurobiParametersProto} that this class represents. */
    public GurobiParametersProto toProto() {
      GurobiParametersProto.Builder protoBuilder = GurobiParametersProto.newBuilder();
      for (Map.Entry<String, String> entry : paramValues.entrySet()) {
        protoBuilder.addParameters(GurobiParametersProto.Parameter.newBuilder()
                .setName(entry.getKey())
                .setValue(entry.getValue())
                .build());
      }
      return protoBuilder.build();
    }

    public GurobiParameters() {}

    public GurobiParameters(GurobiParametersProto proto) {
      for (GurobiParametersProto.Parameter parameter : proto.getParametersList()) {
        paramValues.put(parameter.getName(), parameter.getValue());
      }
    }
  }

  /**
   * GLPK specific parameters for solver.
   *
   * <p>Fields are optional to enable capturing user intention; if the user explicitly sets a value,
   * then no generic solve parameters will overwrite this parameter. User specified solver specific
   * parameters have priority over generic parameters.
   */
  public static final class GlpkParameters {
    private Optional<Boolean> computeUnboundRaysIfPossible = Optional.empty();

    /**
     * Compute the primal or dual unbound ray when the variable (structural or auxiliary) causing
     * the unboundedness is identified (see glp_get_unbnd_ray() in <a
     * href="https://www.gnu.org/software/glpk/#documentation">GLPK documentation</a>).
     *
     * <p>The absent value is equivalent to false.
     *
     * <p>Rays are only available when solving linear programs; they are not available for MIPs. On
     * top of that, they are only available when using a simplex algorithm with the presolve
     * disabled.
     *
     * <p>A primal ray can only be built if the chosen LP algorithm is {@link
     * LPAlgorithm#PRIMAL_SIMPLEX}. The same for a dual ray and {@link LPAlgorithm#DUAL_SIMPLEX}.
     *
     * <p>The computation involves the basis factorizaion to be available which may lead to extra
     * computation/errors.
     */
    GlpkParameters setComputeUnboundRaysIfPossible(boolean computeUnboundRaysIfPossible) {
      this.computeUnboundRaysIfPossible = Optional.of(computeUnboundRaysIfPossible);
      return this;
    }

    Optional<Boolean> getComputeUnboundRaysIfPossible() {
      return this.computeUnboundRaysIfPossible;
    }

    /** Returns the correspong {@link GlpkParametersProto} that this class represents. */
    GlpkParametersProto toProto() {
      GlpkParametersProto.Builder protoBuilder = GlpkParametersProto.newBuilder();
      if (computeUnboundRaysIfPossible.isPresent()) {
        protoBuilder.setComputeUnboundRaysIfPossible(computeUnboundRaysIfPossible.get());
      }
      return protoBuilder.build();
    }

    GlpkParameters() {}

    GlpkParameters(GlpkParametersProto proto) {
      if (proto.hasComputeUnboundRaysIfPossible()) {
        this.computeUnboundRaysIfPossible = Optional.of(proto.getComputeUnboundRaysIfPossible());
      }
    }
  }

  /**
   * Xpress specific parameters for solver. See <a
   * href="https://www.fico.com/fico-xpress-optimization/docs/latest/solver/optimizer/HTML/chapter7.html">Xpress's
   * documentation</a> for a list of possible parameters.
   *
   * <p>Example use:
   *
   * <pre>{@code
   * XpressParameters xpress;
   * xpress.addParamValue("BarIterLimit", "10");
   * }</pre>
   */
  public static final class XpressParameters {
    private final LinkedHashMap<String, String> paramValues = new LinkedHashMap<>();

    /** Adds the parameter name-value pairs to set in insertion order. */
    public XpressParameters addParamValue(String name, String value) {
      paramValues.put(name, value);
      return this;
    }

    public Map<String, String> getParamValues() {
      return this.paramValues;
    }

    /** Returns the corresponding {@link XpressParametersProto} that this class represents. */
    public XpressParametersProto toProto() {
      XpressParametersProto.Builder protoBuilder = XpressParametersProto.newBuilder();
      for (Map.Entry<String, String> entry : paramValues.entrySet()) {
        protoBuilder.addParameters(XpressParametersProto.Parameter.newBuilder()
                .setName(entry.getKey())
                .setValue(entry.getValue())
                .build());
      }
      return protoBuilder.build();
    }

    public XpressParameters() {}

    public XpressParameters(XpressParametersProto proto) {
      for (XpressParametersProto.Parameter parameter : proto.getParametersList()) {
        if (paramValues.put(parameter.getName(), parameter.getValue()) != null) {
          throw new IllegalArgumentException(
              String.format("duplicate Xpress parameter: '%s'", parameter.getName()));
        }
      }
    }
  }

  /**
   * Enables printing the solver implementation traces. These traces are sent to the standard output
   * stream. Except for PDLP, which uses VLOG instead.
   *
   * <p>Note that if the solver supports message callback and the user registers a callback for it,
   * then this parameter value is ignored and no traces are printed.
   */
  public SolveParameters setEnableOutput(boolean enableOutput) {
    this.enableOutput = enableOutput;
    return this;
  }

  /**
   * Maximum time a solver should spend on the problem.
   *
   * <p>This value is not a hard limit, solve time may slightly exceed this value. Always passed to
   * the underlying solver, the solver default is not used.
   */
  public SolveParameters setTimeLimit(Duration timeLimit) {
    this.timeLimit = Optional.of(timeLimit);
    return this;
  }

  /**
   * Limit on the iterations of the underlying algorithm (e.g. simplex pivots). The specific
   * behavior is dependent on the solver and algorithm used, but often can give a deterministic
   * solve limit (further configuration may be needed, e.g. one thread.
   *
   * <p>Typically supported by LP, QP, and MIP solvers, but for MIP solvers, see also {@link
   * #setNodeLimit(long)}
   */
  public SolveParameters setIterationLimit(long iterationLimit) {
    this.iterationLimit = OptionalLong.of(iterationLimit);
    return this;
  }

  /**
   * Limit on the number of subproblems solved in enumerative search (e.g. branch and bound). For
   * many solvers, this can be used to deterministically limit computation (further configuration
   * may be needed, e.g. one thread).
   *
   * <p>Typically for MIP solvers, see also {@link #setIterationLimit(long)}.
   */
  public SolveParameters setNodeLimit(long nodeLimit) {
    this.nodeLimit = OptionalLong.of(nodeLimit);
    return this;
  }

  /**
   * The solver stops early if it can prove there are no primal solutions at least as good as
   * cutoff.
   *
   * <p>On an early stop, the solver returns {@link SolveResult.TerminationReason#NO_SOLUTION_FOUND}
   * and with {@link SolveResult.Limit#CUTOFF} and is not required to give any extra solution
   * information. This has no effect on the return value if there is no early stop.
   *
   * <p>It is recommended that you use a tolerance if you want solutions with objective exactly
   * equal to cutoff to be returned.
   *
   * <p>See the user guide for more details and a comparison with {@link
   * #setBestBoundLimit(double)}.
   */
  public SolveParameters setCutoffLimit(double cutoffLimit) {
    this.cutoffLimit = OptionalDouble.of(cutoffLimit);
    return this;
  }

  /**
   * The solver stops early as soon as it finds a solution at least this good, with {@link
   * SolveResult.TerminationReason#FEASIBLE} and {@link SolveResult.Limit#OBJECTIVE}.
   */
  public SolveParameters setObjectiveLimit(double objectiveLimit) {
    this.objectiveLimit = OptionalDouble.of(objectiveLimit);
    return this;
  }

  /**
   * The solver stops early as soon as it finds a solution at least this good, with {@link
   * SolveResult.TerminationReason#FEASIBLE} or {@link
   * SolveResult.TerminationReason#NO_SOLUTION_FOUND} and {@link SolveResult.Limit#OBJECTIVE}.
   *
   * <p>See the user guide for comparison with {@link #setCutoffLimit(double)}.
   */
  public SolveParameters setBestBoundLimit(double bestBoundLimit) {
    this.bestBoundLimit = OptionalDouble.of(bestBoundLimit);
    return this;
  }

  /**
   * The solver stops early after finding this many feasible solutions, with {@link
   * SolveResult.TerminationReason#FEASIBLE} and {@link SolveResult.Limit#SOLUTION}. Must be greater
   * than zero if set. It is often used to get the solver to stop on the first feasible solution
   * found. Not that there is no guarantee on the objective value for any of the returned solutions.
   *
   * <p>Solvers will typically not return more solutions than the solution limit, but this is not
   * enforce by MathOpt, see also b/214041169.
   *
   * <p>Currently supported for Gurobi and SCIP, and for CP-SAT only with value 1.
   */
  public SolveParameters setSolutionLimit(int solutionLimit) {
    this.solutionLimit = OptionalInt.of(solutionLimit);
    return this;
  }

  /**
   * If unset, use the solver default. If set, it must be >= 1, or else an exception will be thrown
   * on {@link Solve#solve(Model, SolverType)}.
   */
  public SolveParameters setThreads(int threads) {
    this.threads = OptionalInt.of(threads);
    return this;
  }

  /**
   * Seed for the pseudo-random number generator in the underlying solver. Note that all solvers use
   * pseudo-random numbers to select things such as perturbation in the LP algorithm, for
   * tie-break-up rules, and for heuristic fixings. Varying this can have a noticeable impact on
   * solver behavior.
   *
   * <p>Although all solvers have a concept of seeds, note that valid values depend on the actual
   * solver. An exception will be thrown on {@link Solve#solve(Model, SolverType)} for invalid
   * values.
   *
   * <ul>
   *   <li>Gurobi: [0:GRB_MAXINT] (which as of Gurobi 9.0 is 2x10^9).
   *   <li>GSCIP: [0:2147483647] (which is MAX_INT or kint32max or 2^31-1).
   *   <li>GLOP: [0:2147483647] (same as above)
   * </ul>
   *
   * <p>In all cases, the solver will receive a value equal to: MAX(0,
   * MIN(MAX_VALID_VALUE_FOR_SOLVER, random_seed)).
   */
  public SolveParameters setRandomSeed(int randomSeed) {
    this.randomSeed = OptionalInt.of(randomSeed);
    return this;
  }

  /**
   * An absolute optimality tolerance (primarily) for MIP solvers.
   *
   * <p>The absolute GAP is the absolute value of the difference between:
   *
   * <ul>
   *   <li>the objective value of the best feasible solution found,
   *   <li>the dual bound produced by the search.
   * </ul>
   *
   * <p>The solver can stop once the absolute GAP is at most {@code absoluteGapTolerance} (when
   * set), and return {@link SolveResult.TerminationReason#OPTIMAL}.
   *
   * <p>Must be >= 0 if set, and an exception will be thrown on {@link Solve#solve(Model,
   * SolverType)}.
   *
   * <p>See also {@link #setRelativeGapTolerance(double)}.
   */
  public SolveParameters setAbsoluteGapTolerance(double absoluteGapTolerance) {
    this.absoluteGapTolerance = OptionalDouble.of(absoluteGapTolerance);
    return this;
  }

  /**
   * A relative optimality tolerance (primarily) for MIP solvers.
   *
   * <p>The relative GAP is a normalize version of the absolute GAP (defined on {@link
   * #setAbsoluteGapTolerance}, where the normalization is solver-dependent, e.g. the absolute GAP
   * divided by the objective value of the best feasible solution found.
   *
   * <p>The solver can stop once the relative GAP is at most {@code relativeGapTolerance} (when
   * set), and return {@link SolveResult.TerminationReason#OPTIMAL}.
   *
   * <p>Must be >= 0 if set, and an exception will be thrown on {@link Solve#solve(Model,
   * SolverType)}.
   *
   * <p>See also {@link #setAbsoluteGapTolerance(double)}.
   */
  public SolveParameters setRelativeGapTolerance(double relativeGapTolerance) {
    this.relativeGapTolerance = OptionalDouble.of(relativeGapTolerance);
    return this;
  }

  /**
   * Maintain up to {@code solutionPoolSize} while searching. The solution pool generally has two
   * functions:
   *
   * <ol>
   *   <li>For solvers that can return more than one solution, this limits how many solutions will
   *       be returned.
   *   <li>Some solvers may run heuristics using solutions from the solution pool, so changing this
   *       value may affect the algorithm's path.
   * </ol>
   *
   * <p>To force the solver to fill the solution pool, e.g. with the n best solutions, requires
   * further, solver specific configuration.
   */
  public SolveParameters setSolutionPoolSize(int solutionPoolSize) {
    this.solutionPoolSize = OptionalInt.of(solutionPoolSize);
    return this;
  }

  /**
   * The algorithm for solving a linear program. If absent, use the solver default algorithm.
   *
   * <p>For problems that are not linear programs but where linear programming is a subroutine,
   * solvers may use this value. E.g. MIP solvers will typically use this for the root LP solve only
   * (and use dual simplex otherwise).
   */
  public SolveParameters setLPAlgorithm(LPAlgorithm lpAlgorithm) {
    this.lpAlgorithm = Optional.of(lpAlgorithm);
    return this;
  }

  /**
   * Effort on simplifying the problem before starting the main algorithm, or the solver default
   * effort level if absent.
   */
  public SolveParameters setPresolve(Emphasis presolve) {
    this.presolve = Optional.of(presolve);
    return this;
  }

  /**
   * Effort on getting a strong LP relaxation (MIP only) or the solver default effort level is
   * absent.
   *
   * <p>Note: disabling cuts may prevent callbacks from having a chance to add cuts at MIP_NODE.
   * This behavior is solver specific.
   */
  public SolveParameters setCuts(Emphasis cuts) {
    this.cuts = Optional.of(cuts);
    return this;
  }

  /**
   * Effort in finding feasible solutions beyond those encountered in the complete search procedure
   * (MIP only), or the solver default effort level if absent.
   */
  public SolveParameters setHeuristics(Emphasis heuristics) {
    this.heuristics = Optional.of(heuristics);
    return this;
  }

  /**
   * Effort in rescaling the problem to improve numerical stability, or the solver default effort
   * level if absent.
   */
  public SolveParameters setScaling(Emphasis scaling) {
    this.scaling = Optional.of(scaling);
    return this;
  }

  /** Solver specific parameters for {@link SolverType#GSCIP}. */
  public SolveParameters setGscip(GScipParameters gscip) {
    this.gscip = Optional.of(gscip);
    return this;
  }

  /** Solver specific parameters for {@link SolverType#GUROBI}. */
  public SolveParameters setGurobi(GurobiParameters gurobi) {
    this.gurobi = Optional.of(gurobi);
    return this;
  }

  /**
   * Solver specific parameters for {@link SolverType#GUROBI}, set from the {@link
   * GurobiParametersProto} proto.
   */
  public SolveParameters setGurobiFromProto(GurobiParametersProto gurobi) {
    this.gurobi = Optional.of(new GurobiParameters(gurobi));
    return this;
  }

  /** Solver specific parameters for {@link SolverType#GLOP}. */
  public SolveParameters setGlop(GlopParameters glop) {
    this.glop = Optional.of(glop);
    return this;
  }

  /** Solver specific parameters for {@link SolverType#CP_SAT}. */
  public SolveParameters setCpSat(SatParameters cpSat) {
    this.cpSat = Optional.of(cpSat);
    return this;
  }

  /** Solver specific parameters for {@link SolverType#PDLP}. */
  public SolveParameters setPdlp(PrimalDualHybridGradientParams pdlp) {
    this.pdlp = Optional.of(pdlp);
    return this;
  }

  /**
   * Solver specific parameters for {@link SolverType#OSQP}.
   *
   * <p>Users should prefer the generic MathOpt parameters over OSQP-level parameters when
   * available:
   *
   * <ul>
   *   <li>Prefer {@link #setEnableOutput(boolean)} to {@link OsqpSettingsProto#getVerbose()}.
   *   <li>Prefer {@link #setTimeLimit(Duration)} to {@link OsqpSettingsProto#getTimeLimit()}.
   *   <li>Prefer {@link #setIterationLimit(long)} to {@link OsqpSettingsProto#getMaxIter()}.
   *   <li>If a less granular configuration is acceptable, prefer {@link #setScaling(Emphasis)} to
   *       {@link OsqpSettingsProto#getScaling()}.
   * </ul>
   */
  public SolveParameters setOsqp(OsqpSettingsProto osqp) {
    this.osqp = Optional.of(osqp);
    return this;
  }

  /** Solver specific parameters for {@link SolverType#GLPK}. */
  public SolveParameters setGlpk(GlpkParameters glpk) {
    this.glpk = Optional.of(glpk);
    return this;
  }

  /**
   * Solver specific parameters for {@link SolverType#GLPK}, set from the {@link
   * GlpkParametersProto} proto.
   */
  public SolveParameters setGlpkFromProto(GlpkParametersProto glpk) {
    this.glpk = Optional.of(new GlpkParameters(glpk));
    return this;
  }

  /** Solver specific parameters for {@link SolverType#HIGHS}. */
  public SolveParameters setHighs(HighsOptionsProto highs) {
    this.highs = Optional.of(highs);
    return this;
  }

  /** Solver specific parameters for {@link SolverType#XPRESS}. */
  public SolveParameters setXpress(XpressParameters xpress) {
    this.xpress = Optional.of(xpress);
    return this;
  }

  /**
   * Solver specific parameters for {@link SolverType#XPRESS}, set from the {@link
   * XpressParametersProto} proto.
   */
  public SolveParameters setXpressFromProto(XpressParametersProto xpress) {
    this.xpress = Optional.of(new XpressParameters(xpress));
    return this;
  }

  public boolean getEnableOutput() {
    return enableOutput;
  }

  public Optional<Duration> getTimeLimit() {
    return timeLimit;
  }

  public OptionalLong getIterationLimit() {
    return iterationLimit;
  }

  public OptionalLong getNodeLimit() {
    return nodeLimit;
  }

  public OptionalDouble getCutoffLimit() {
    return cutoffLimit;
  }

  public OptionalDouble getObjectiveLimit() {
    return objectiveLimit;
  }

  public OptionalDouble getBestBoundLimit() {
    return bestBoundLimit;
  }

  public OptionalInt getSolutionLimit() {
    return solutionLimit;
  }

  public OptionalInt getThreads() {
    return threads;
  }

  public OptionalInt getRandomSeed() {
    return randomSeed;
  }

  public OptionalDouble getAbsoluteGapTolerance() {
    return absoluteGapTolerance;
  }

  public OptionalDouble getRelativeGapTolerance() {
    return relativeGapTolerance;
  }

  public OptionalInt getSolutionPoolSize() {
    return solutionPoolSize;
  }

  public Optional<LPAlgorithm> getLPAlgorithm() {
    return lpAlgorithm;
  }

  public Optional<Emphasis> getPresolve() {
    return presolve;
  }

  public Optional<Emphasis> getCuts() {
    return cuts;
  }

  public Optional<Emphasis> getHeuristics() {
    return heuristics;
  }

  public Optional<Emphasis> getScaling() {
    return scaling;
  }

  public Optional<GScipParameters> getGscip() {
    return gscip;
  }

  public Optional<GurobiParameters> getGurobi() {
    return gurobi;
  }

  public Optional<GlopParameters> getGlop() {
    return glop;
  }

  public Optional<SatParameters> getCpSat() {
    return cpSat;
  }

  public Optional<PrimalDualHybridGradientParams> getPdlp() {
    return pdlp;
  }

  public Optional<OsqpSettingsProto> getOsqp() {
    return osqp;
  }

  Optional<GlpkParameters> getGlpk() {
    return glpk;
  }

  Optional<HighsOptionsProto> getHighs() {
    return highs;
  }

  Optional<XpressParameters> getXpress() {
    return xpress;
  }

  /** Returns the correspong {@link SolveParametersProto} that this class represents. */
  public SolveParametersProto toProto() {
    SolveParametersProto.Builder protoBuilder = SolveParametersProto.newBuilder();
    protoBuilder.setEnableOutput(this.enableOutput);
    if (this.timeLimit.isPresent()) {
      protoBuilder.setTimeLimit(toProtoDuration(this.timeLimit.get()));
    }
    if (this.iterationLimit.isPresent()) {
      protoBuilder.setIterationLimit(this.iterationLimit.getAsLong());
    }
    if (this.nodeLimit.isPresent()) {
      protoBuilder.setNodeLimit(this.nodeLimit.getAsLong());
    }
    if (this.cutoffLimit.isPresent()) {
      protoBuilder.setCutoffLimit(this.cutoffLimit.getAsDouble());
    }
    if (this.objectiveLimit.isPresent()) {
      protoBuilder.setObjectiveLimit(this.objectiveLimit.getAsDouble());
    }
    if (this.bestBoundLimit.isPresent()) {
      protoBuilder.setBestBoundLimit(this.bestBoundLimit.getAsDouble());
    }
    if (this.solutionLimit.isPresent()) {
      protoBuilder.setSolutionLimit(this.solutionLimit.getAsInt());
    }
    if (this.threads.isPresent()) {
      protoBuilder.setThreads(this.threads.getAsInt());
    }
    if (this.randomSeed.isPresent()) {
      protoBuilder.setRandomSeed(this.randomSeed.getAsInt());
    }
    if (this.absoluteGapTolerance.isPresent()) {
      protoBuilder.setAbsoluteGapTolerance(this.absoluteGapTolerance.getAsDouble());
    }
    if (this.relativeGapTolerance.isPresent()) {
      protoBuilder.setRelativeGapTolerance(this.relativeGapTolerance.getAsDouble());
    }
    if (this.solutionPoolSize.isPresent()) {
      protoBuilder.setSolutionPoolSize(this.solutionPoolSize.getAsInt());
    }
    if (this.lpAlgorithm.isPresent()) {
      protoBuilder.setLpAlgorithm(this.lpAlgorithm.get().toProto());
    }
    if (this.presolve.isPresent()) {
      protoBuilder.setPresolve(this.presolve.get().toProto());
    }
    if (this.cuts.isPresent()) {
      protoBuilder.setCuts(this.cuts.get().toProto());
    }
    if (this.heuristics.isPresent()) {
      protoBuilder.setHeuristics(this.heuristics.get().toProto());
    }
    if (this.scaling.isPresent()) {
      protoBuilder.setScaling(this.scaling.get().toProto());
    }
    if (this.gscip.isPresent()) {
      protoBuilder.setGscip(this.gscip.get());
    }
    if (this.gurobi.isPresent()) {
      protoBuilder.setGurobi(this.gurobi.get().toProto());
    }
    if (this.glop.isPresent()) {
      protoBuilder.setGlop(this.glop.get());
    }
    if (this.cpSat.isPresent()) {
      protoBuilder.setCpSat(this.cpSat.get());
    }
    if (this.pdlp.isPresent()) {
      protoBuilder.setPdlp(this.pdlp.get());
    }
    if (this.osqp.isPresent()) {
      protoBuilder.setOsqp(this.osqp.get());
    }
    if (this.glpk.isPresent()) {
      protoBuilder.setGlpk(this.glpk.get().toProto());
    }
    if (this.highs.isPresent()) {
      protoBuilder.setHighs(highs.get());
    }
    if (this.xpress.isPresent()) {
      protoBuilder.setXpress(this.xpress.get().toProto());
    }
    return protoBuilder.build();
  }

  public SolveParameters() {}

  /**
   * Creates a new SolveParameters class from the give {@code proto}
   *
   * @throws IllegalArgumentException if the duration proto in {@link
   *     SolveParametersProto#getTimeLimit()} is invalid.
   */
  public SolveParameters(SolveParametersProto proto) {
    this.enableOutput = proto.getEnableOutput();
    if (proto.hasTimeLimit()) {
      this.timeLimit = Optional.of(toJavaDuration(proto.getTimeLimit()));
    }
    if (proto.hasIterationLimit()) {
      this.iterationLimit = OptionalLong.of(proto.getIterationLimit());
    }
    if (proto.hasNodeLimit()) {
      this.nodeLimit = OptionalLong.of(proto.getNodeLimit());
    }
    if (proto.hasCutoffLimit()) {
      this.cutoffLimit = OptionalDouble.of(proto.getCutoffLimit());
    }
    if (proto.hasObjectiveLimit()) {
      this.objectiveLimit = OptionalDouble.of(proto.getObjectiveLimit());
    }
    if (proto.hasBestBoundLimit()) {
      this.bestBoundLimit = OptionalDouble.of(proto.getBestBoundLimit());
    }
    if (proto.hasSolutionLimit()) {
      this.solutionLimit = OptionalInt.of(proto.getSolutionLimit());
    }
    if (proto.hasThreads()) {
      this.threads = OptionalInt.of(proto.getThreads());
    }
    if (proto.hasRandomSeed()) {
      this.randomSeed = OptionalInt.of(proto.getRandomSeed());
    }
    if (proto.hasAbsoluteGapTolerance()) {
      this.absoluteGapTolerance = OptionalDouble.of(proto.getAbsoluteGapTolerance());
    }
    if (proto.hasRelativeGapTolerance()) {
      this.relativeGapTolerance = OptionalDouble.of(proto.getRelativeGapTolerance());
    }
    if (proto.hasSolutionPoolSize()) {
      this.solutionPoolSize = OptionalInt.of(proto.getSolutionPoolSize());
    }
    this.lpAlgorithm = LPAlgorithm.fromProto(proto.getLpAlgorithm());
    this.presolve = Emphasis.fromProto(proto.getPresolve());
    this.cuts = Emphasis.fromProto(proto.getCuts());
    this.heuristics = Emphasis.fromProto(proto.getHeuristics());
    this.scaling = Emphasis.fromProto(proto.getScaling());
    if (proto.hasGscip()) {
      this.gscip = Optional.of(proto.getGscip());
    }
    if (proto.hasGurobi()) {
      this.gurobi = Optional.of(new GurobiParameters(proto.getGurobi()));
    }
    if (proto.hasGlop()) {
      this.glop = Optional.of(proto.getGlop());
    }
    if (proto.hasCpSat()) {
      this.cpSat = Optional.of(proto.getCpSat());
    }
    if (proto.hasPdlp()) {
      this.pdlp = Optional.of(proto.getPdlp());
    }
    if (proto.hasOsqp()) {
      this.osqp = Optional.of(proto.getOsqp());
    }
    if (proto.hasGlpk()) {
      this.glpk = Optional.of(new GlpkParameters(proto.getGlpk()));
    }
    if (proto.hasHighs()) {
      this.highs = Optional.of(proto.getHighs());
    }
    if (proto.hasXpress()) {
      this.xpress = Optional.of(new XpressParameters(proto.getXpress()));
    }
  }
}
