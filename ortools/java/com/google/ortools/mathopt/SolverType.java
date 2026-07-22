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

import com.google.common.base.Verify;
import java.util.EnumMap;
import java.util.Optional;

/** The solvers supported by MathOpt. */
public enum SolverType {
  /**
   * Solving Constraint Integer Programs (SCIP) solver (third party).
   *
   * <p>Supports LP, MIP, and nonconvex integer quadratic problems. No dual data for LPs is returned
   * though. Prefer GLOP for LPs.
   */
  GSCIP(SolverTypeProto.SOLVER_TYPE_GSCIP),

  /**
   * Gurobi solver (third party).
   *
   * <p>Supports LP, MIP, and nonconvex integer quadratic problems. Generally the fastest option,
   * but has special licensing, see go/gurobi-google for details.
   */
  GUROBI(SolverTypeProto.SOLVER_TYPE_GUROBI),

  /**
   * Google's GLOP solver.
   *
   * <p>Supports LP with primal and dual simplex methods.
   */
  GLOP(SolverTypeProto.SOLVER_TYPE_GLOP),

  /**
   * Google's CP-SAT solver.
   *
   * <p>Supports problems where all variables are integer and bounded (or implied to be after
   * presolve). Experimental support to rescale and discretize problems with continuous variables.
   */
  CP_SAT(SolverTypeProto.SOLVER_TYPE_CP_SAT),

  /**
   * Google's PDLP solver.
   *
   * <p>Supports LP and convex diagonal quadratic objectives. Uses first order methods rather than
   * simplex. Can solve very large problems.
   */
  PDLP(SolverTypeProto.SOLVER_TYPE_PDLP),

  /**
   * GNU Linear Programming Kit (GLPK) solver (third party)
   *
   * <p>Supports MIP and LP.
   *
   * <p>Thread-safety: GLPK use thread-local storage for memory allocations. As a consequence,
   * native Solver instances must be destroyed on the same thread as they are created or GLPK will
   * crash. Using GLPK via {@link Solve#solve(Model, SolverType)} will maintain this invariant, but
   * incremental solving from Java does not ensure this as of January 2023 and should be avoided.
   *
   * <p>When solving a LP with the presolver, a solution (and the unbound rays) are only returned if
   * an optimal solution has been found. Else nothing is returned. See glpk-5.0/doc/glpk.pdf page
   * #40 available from glpk-5.0.tar.gz for details.
   */
  GLPK(SolverTypeProto.SOLVER_TYPE_GLPK),

  /**
   * The Operator Splitting Quadratic Program (OSQP) solver (third party).
   *
   * <p>Supports continuous problems with linear constraints and linear or convex quadratic
   * objectives. Uses a first-order method.
   */
  OSQP(SolverTypeProto.SOLVER_TYPE_OSQP),

  /**
   * The Embedded Conic Solver ECOS (third party).
   *
   * <p>Supports LP and SOCP problems. Uses interior point methods (barrier).
   */
  ECOS(SolverTypeProto.SOLVER_TYPE_ECOS),

  /**
   * The Splitting Conic Solver SCS (third party).
   *
   * <p>Supports LP and SOCP problems. Uses a first-order method.
   */
  SCS(SolverTypeProto.SOLVER_TYPE_SCS),

  /**
   * The HiGHS Solver (third party).
   *
   * <p>Supports LP and MIP problems (convex QP support is not implemented).
   */
  HIGHS(SolverTypeProto.SOLVER_TYPE_HIGHS),

  /**
   * The Santorini Solver (first party).
   *
   * <p>Supports MIPs. Experimental, do not use in production.
   */
  SANTORINI(SolverTypeProto.SOLVER_TYPE_SANTORINI),

  /**
   * The Xpress Solver (third party).
   *
   * <p>Supports LP, MIP, and nonconvex integer quadratic problems. Need a special license.
   */
  XPRESS(SolverTypeProto.SOLVER_TYPE_XPRESS),

  /**
   * Google's Min-Cost Flow solver.
   *
   * <p>Uses a specialized solver for Min-Cost Flow problems (see
   * https://developers.google.com/optimization/flow/mincostflow). Supports LP problems that match
   * the structure of a Min-Cost Flow problem (see go/mathopt-min-cost-flow).
   *
   * <p>Requirements:
   *
   * <ul>
   *   <li>The constraint matrix must be the node-arc incidence matrix of a digraph, that is, each
   *       variable appears in exactly two constraints, with coefficients +1 and -1.
   *   <li>Only linear constraints are allowed.
   *   <li>All linear constraints must be equality constraints.
   *   <li>All variable lower bounds must be 0.
   *   <li>All variables and constraints must have integer bounds and costs.
   *   <li>The objective must be linear.
   * </ul>
   */
  MIN_COST_FLOW(SolverTypeProto.SOLVER_TYPE_MIN_COST_FLOW),

  /**
   * IBM ILOG CPLEX solver (third party).
   *
   * <p>Supports LP and MIP problems (other types are unimplemented). A fast option, but has special
   * licensing.
   */
  CPLEX(SolverTypeProto.SOLVER_TYPE_CPLEX);

  private static class ProtoMap {
    private static final EnumMap<SolverTypeProto, SolverType> map =
        new EnumMap<>(SolverTypeProto.class);
  }

  private final SolverTypeProto proto;

  SolverType(SolverTypeProto proto) {
    ProtoMap.map.put(proto, this);
    this.proto = proto;
  }

  public SolverTypeProto toProto() {
    return this.proto;
  }

  /**
   * Returns a {@link SolverType} from the given {@code proto}, or an empty optional if {@code
   * proto} is unspecified.
   *
   * @throws UnsupportedOperationException if {@code proto} is unrecognized.
   * @see #fromProto(SolverTypeProto)
   */
  public static Optional<SolverType> optionalFromProto(SolverTypeProto proto) {
    switch (proto) {
      case SOLVER_TYPE_UNSPECIFIED -> {
        return Optional.empty();
      }
      case UNRECOGNIZED ->
        throw new UnsupportedOperationException(
            "SolverTypeProto was UNRECOGNIZED, cannot convert to SolverType. Typically caused by"
            + " proto data newer than the compiled proto file.");
      default -> {
        SolverType solverType = ProtoMap.map.get(proto);
        // Unit tests ensure this is not reachable.
        Verify.verifyNotNull(
            solverType, "SolverTypeProto %s is not associated with any SolverType", proto);
        return Optional.of(solverType);
      }
    }
  }

  /**
   * Returns a {@link SolverType} from the given {@code proto}.
   *
   * @throws IllegalArgumentException if {@code proto} is unspecified.
   * @throws UnsupportedOperationException if {@code proto} is unrecognized.
   * @see #optionalFromProto(SolverTypeProto)
   */
  public static SolverType fromProto(SolverTypeProto proto) {
    return optionalFromProto(proto).orElseThrow(
        ()
            -> new IllegalArgumentException(
                "SolverTypeProto was SOLVER_TYPE_UNSPECIFIED, this value is not convertible to"
                + " SolverType"));
  }
}
