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

import static com.google.common.truth.Truth.assertThat;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.google.ortools.GScipOutput;
import com.google.ortools.mathopt.BasisProto;
import com.google.ortools.mathopt.BasisStatusProto;
import com.google.ortools.mathopt.DualRayProto;
import com.google.ortools.mathopt.DualSolutionProto;
import com.google.ortools.mathopt.FeasibilityStatusProto;
import com.google.ortools.mathopt.LimitProto;
import com.google.ortools.mathopt.ObjectiveBoundsProto;
import com.google.ortools.mathopt.OsqpOutput;
import com.google.ortools.mathopt.PrimalRayProto;
import com.google.ortools.mathopt.PrimalSolutionProto;
import com.google.ortools.mathopt.ProblemStatusProto;
import com.google.ortools.mathopt.SolutionProto;
import com.google.ortools.mathopt.SolutionStatusProto;
import com.google.ortools.mathopt.SolveResult.FeasibilityStatus;
import com.google.ortools.mathopt.SolveResultProto;
import com.google.ortools.mathopt.SolveStatsProto;
import com.google.ortools.mathopt.TerminationProto;
import com.google.ortools.mathopt.TerminationReasonProto;
import com.google.ortools.pdlp.ConvergenceInformation;
import java.time.Duration;
import org.junit.jupiter.api.Test;

public final class SolveResultTest {
  @Test
  public void feasibilityStatus_toProto() {
    assertThat(SolveResult.FeasibilityStatus.UNDETERMINED.toProto())
        .isEqualTo(FeasibilityStatusProto.FEASIBILITY_STATUS_UNDETERMINED);
    assertThat(SolveResult.FeasibilityStatus.FEASIBLE.toProto())
        .isEqualTo(FeasibilityStatusProto.FEASIBILITY_STATUS_FEASIBLE);
    assertThat(SolveResult.FeasibilityStatus.INFEASIBLE.toProto())
        .isEqualTo(FeasibilityStatusProto.FEASIBILITY_STATUS_INFEASIBLE);
  }

  @Test
  public void terminationReason_toProto() {
    assertThat(SolveResult.TerminationReason.OPTIMAL.toProto())
        .isEqualTo(TerminationReasonProto.TERMINATION_REASON_OPTIMAL);
    assertThat(SolveResult.TerminationReason.INFEASIBLE.toProto())
        .isEqualTo(TerminationReasonProto.TERMINATION_REASON_INFEASIBLE);
    assertThat(SolveResult.TerminationReason.UNBOUNDED.toProto())
        .isEqualTo(TerminationReasonProto.TERMINATION_REASON_UNBOUNDED);
    assertThat(SolveResult.TerminationReason.INFEASIBLE_OR_UNBOUNDED.toProto())
        .isEqualTo(TerminationReasonProto.TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED);
    assertThat(SolveResult.TerminationReason.IMPRECISE.toProto())
        .isEqualTo(TerminationReasonProto.TERMINATION_REASON_IMPRECISE);
    assertThat(SolveResult.TerminationReason.FEASIBLE.toProto())
        .isEqualTo(TerminationReasonProto.TERMINATION_REASON_FEASIBLE);
    assertThat(SolveResult.TerminationReason.NO_SOLUTION_FOUND.toProto())
        .isEqualTo(TerminationReasonProto.TERMINATION_REASON_NO_SOLUTION_FOUND);
    assertThat(SolveResult.TerminationReason.NUMERICAL_ERROR.toProto())
        .isEqualTo(TerminationReasonProto.TERMINATION_REASON_NUMERICAL_ERROR);
    assertThat(SolveResult.TerminationReason.OTHER_ERROR.toProto())
        .isEqualTo(TerminationReasonProto.TERMINATION_REASON_OTHER_ERROR);
  }

  @Test
  public void limit_toProto() {
    assertThat(SolveResult.Limit.UNDETERMINED.toProto()).isEqualTo(LimitProto.LIMIT_UNDETERMINED);
    assertThat(SolveResult.Limit.ITERATION.toProto()).isEqualTo(LimitProto.LIMIT_ITERATION);
    assertThat(SolveResult.Limit.TIME.toProto()).isEqualTo(LimitProto.LIMIT_TIME);
    assertThat(SolveResult.Limit.NODE.toProto()).isEqualTo(LimitProto.LIMIT_NODE);
    assertThat(SolveResult.Limit.SOLUTION.toProto()).isEqualTo(LimitProto.LIMIT_SOLUTION);
    assertThat(SolveResult.Limit.MEMORY.toProto()).isEqualTo(LimitProto.LIMIT_MEMORY);
    assertThat(SolveResult.Limit.CUTOFF.toProto()).isEqualTo(LimitProto.LIMIT_CUTOFF);
    assertThat(SolveResult.Limit.OBJECTIVE.toProto()).isEqualTo(LimitProto.LIMIT_OBJECTIVE);
    assertThat(SolveResult.Limit.NORM.toProto()).isEqualTo(LimitProto.LIMIT_NORM);
    assertThat(SolveResult.Limit.INTERRUPTED.toProto()).isEqualTo(LimitProto.LIMIT_INTERRUPTED);
    assertThat(SolveResult.Limit.SLOW_PROGRESS.toProto()).isEqualTo(LimitProto.LIMIT_SLOW_PROGRESS);
    assertThat(SolveResult.Limit.OTHER.toProto()).isEqualTo(LimitProto.LIMIT_OTHER);
  }

  @Test
  public void limit_fromProto_unspecified_throws() {
    var error = assertThrows(IllegalArgumentException.class,
        () -> SolveResult.Limit.fromProto(LimitProto.LIMIT_UNSPECIFIED));
    assertThat(error).hasMessageThat().contains("must be set");
  }

  @Test
  public void problemStatus_validoConstruction() {
    SolveResult.ProblemStatus problemStatus = new SolveResult.ProblemStatus(
        SolveResult.FeasibilityStatus.FEASIBLE, SolveResult.FeasibilityStatus.UNDETERMINED, false);

    assertThat(problemStatus.getPrimalStatus()).isEqualTo(SolveResult.FeasibilityStatus.FEASIBLE);
    assertThat(problemStatus.getDualStatus()).isEqualTo(SolveResult.FeasibilityStatus.UNDETERMINED);
    assertFalse(problemStatus.getPrimalOrDualInfeasible());
  }

  @Test
  public void problemStatus_validProtoConstruction() {
    ProblemStatusProto proto =
        ProblemStatusProto.newBuilder()
            .setPrimalStatus(FeasibilityStatusProto.FEASIBILITY_STATUS_FEASIBLE)
            .setDualStatus(FeasibilityStatusProto.FEASIBILITY_STATUS_UNDETERMINED)
            .setPrimalOrDualInfeasible(false)
            .build();

    SolveResult.ProblemStatus problemStatus = new SolveResult.ProblemStatus(proto);

    assertThat(problemStatus.getPrimalStatus()).isEqualTo(SolveResult.FeasibilityStatus.FEASIBLE);
    assertThat(problemStatus.getDualStatus()).isEqualTo(SolveResult.FeasibilityStatus.UNDETERMINED);
    assertFalse(problemStatus.getPrimalOrDualInfeasible());
  }

  @Test
  public void problemStatus_unspecifiedPrimalStatus_throws() {
    ProblemStatusProto proto =
        ProblemStatusProto.newBuilder()
            .setDualStatus(FeasibilityStatusProto.FEASIBILITY_STATUS_UNDETERMINED)
            .setPrimalOrDualInfeasible(false)
            .build();

    var error =
        assertThrows(IllegalArgumentException.class, () -> new SolveResult.ProblemStatus(proto));
    assertThat(error).hasMessageThat().contains("must be set");
  }

  @Test
  public void problemStatus_unspecifiedDualStatus_throws() {
    ProblemStatusProto proto =
        ProblemStatusProto.newBuilder()
            .setPrimalStatus(FeasibilityStatusProto.FEASIBILITY_STATUS_FEASIBLE)
            .setPrimalOrDualInfeasible(false)
            .build();

    var error =
        assertThrows(IllegalArgumentException.class, () -> new SolveResult.ProblemStatus(proto));
    assertThat(error).hasMessageThat().contains("must be set");
  }

  @Test
  public void problemStatus_toString() {
    var problemStatus = new SolveResult.ProblemStatus(
        FeasibilityStatus.FEASIBLE, FeasibilityStatus.UNDETERMINED, false);
    assertThat(problemStatus.toString())
        .isEqualTo(
            "primal_status: FEASIBLE dual_status: UNDETERMINED primal_or_dual_infeasible: false");
  }

  @Test
  public void objectiveBounds_validConstruction() {
    SolveResult.ObjectiveBounds objectiveBounds = new SolveResult.ObjectiveBounds(1.0, 2.0);

    assertThat(objectiveBounds.getPrimalBound()).isEqualTo(1.0);
    assertThat(objectiveBounds.getDualBound()).isEqualTo(2.0);
  }

  @Test
  public void objectiveBounds_validProtoConstruction() {
    ObjectiveBoundsProto proto =
        ObjectiveBoundsProto.newBuilder().setPrimalBound(1).setDualBound(2).build();

    SolveResult.ObjectiveBounds objectiveBounds = new SolveResult.ObjectiveBounds(proto);

    assertThat(objectiveBounds.getPrimalBound()).isEqualTo(1);
    assertThat(objectiveBounds.getDualBound()).isEqualTo(2);
  }

  @Test
  public void objectiveBounds_toString() {
    SolveResult.ObjectiveBounds objectiveBounds = new SolveResult.ObjectiveBounds(1.0, 2.0);

    assertThat(objectiveBounds.toString()).isEqualTo("primal_bound: 1.0 dual_bound: 2.0");
  }

  @Test
  public void solveStats_validConstruction() {
    SolveStatsProto.Builder proto = SolveStatsProto.newBuilder()
                                        .setSimplexIterations(3)
                                        .setBarrierIterations(4)
                                        .setFirstOrderIterations(5)
                                        .setNodeCount(6);
    proto.getSolveTimeBuilder().setSeconds(7);

    SolveResult.SolveStats solveStats = new SolveResult.SolveStats(proto.build());

    assertThat(solveStats.getSolveTime()).isEqualTo(Duration.ofSeconds(7));
    assertThat(solveStats.getSimplexIterations()).isEqualTo(3);
    assertThat(solveStats.getBarrierIterations()).isEqualTo(4);
    assertThat(solveStats.getFirstOrderIterations()).isEqualTo(5);
    assertThat(solveStats.getNodeCount()).isEqualTo(6);
  }

  @Test
  public void solveStats_toString() {
    SolveStatsProto.Builder proto = SolveStatsProto.newBuilder()
                                        .setSimplexIterations(3)
                                        .setBarrierIterations(4)
                                        .setFirstOrderIterations(5)
                                        .setNodeCount(6);
    proto.getSolveTimeBuilder().setSeconds(7);
    SolveResult.SolveStats solveStats = new SolveResult.SolveStats(proto.build());

    assertThat(solveStats.toString())
        .isEqualTo(
            "solve_time: PT7S simplex_iterations: 3 barrier_iterations: 4 first_order_iterations: 5"
            + " node_count: 6");

    assertThat(solveStats.getSolveTime()).isEqualTo(Duration.ofSeconds(7));
    assertThat(solveStats.getSimplexIterations()).isEqualTo(3);
    assertThat(solveStats.getBarrierIterations()).isEqualTo(4);
    assertThat(solveStats.getFirstOrderIterations()).isEqualTo(5);
    assertThat(solveStats.getNodeCount()).isEqualTo(6);
  }

  @Test
  public void termination_validConstruction() {
    SolveResult.ProblemStatus problemStatus = new SolveResult.ProblemStatus(
        SolveResult.FeasibilityStatus.FEASIBLE, SolveResult.FeasibilityStatus.FEASIBLE, false);
    SolveResult.ObjectiveBounds objectiveBounds = new SolveResult.ObjectiveBounds(2.0, 2.0);
    SolveResult.Termination termination =
        new SolveResult.Termination(SolveResult.TerminationReason.OPTIMAL,
            SolveResult.Limit.SOLUTION, "detail", problemStatus, objectiveBounds);

    assertThat(termination.getReason()).isEqualTo(SolveResult.TerminationReason.OPTIMAL);
    assertThat(termination.getLimit()).hasValue(SolveResult.Limit.SOLUTION);
    assertThat(termination.getDetail()).isEqualTo("detail");
    assertFalse(termination.limitReached());
    assertThat(termination.getProblemStatus().getPrimalStatus())
        .isEqualTo(SolveResult.FeasibilityStatus.FEASIBLE);
    assertThat(termination.getProblemStatus().getDualStatus())
        .isEqualTo(SolveResult.FeasibilityStatus.FEASIBLE);
    assertFalse(termination.getProblemStatus().getPrimalOrDualInfeasible());
    assertThat(termination.getObjectiveBounds().getPrimalBound()).isEqualTo(2.0);
    assertThat(termination.getObjectiveBounds().getDualBound()).isEqualTo(2.0);
  }

  @Test
  public void termination_toString() {
    SolveResult.ProblemStatus problemStatus = new SolveResult.ProblemStatus(
        SolveResult.FeasibilityStatus.FEASIBLE, SolveResult.FeasibilityStatus.FEASIBLE, false);
    SolveResult.ObjectiveBounds objectiveBounds = new SolveResult.ObjectiveBounds(2.0, 2.0);
    SolveResult.Termination termination =
        new SolveResult.Termination(SolveResult.TerminationReason.OPTIMAL,
            SolveResult.Limit.SOLUTION, "detail", problemStatus, objectiveBounds);

    String expected =
        "reason: OPTIMAL limit: Optional[SOLUTION] detail: \"detail\" problem_status: "
        + "{primal_status: FEASIBLE dual_status: FEASIBLE primal_or_dual_infeasible: false} "
        + "objective_bounds: {primal_bound: 2.0 dual_bound: 2.0}";
    assertThat(termination.toString()).isEqualTo(expected);
  }

  @Test
  public void termination_validFromProtoConstruction() {
    TerminationProto.Builder proto =
        TerminationProto.newBuilder()
            .setReason(TerminationReasonProto.TERMINATION_REASON_OPTIMAL)
            .setLimit(LimitProto.LIMIT_SOLUTION)
            .setDetail("detail");
    proto.getProblemStatusBuilder()
        .setPrimalStatus(FeasibilityStatusProto.FEASIBILITY_STATUS_FEASIBLE)
        .setDualStatus(FeasibilityStatusProto.FEASIBILITY_STATUS_INFEASIBLE);
    proto.getObjectiveBoundsBuilder().setPrimalBound(1.0).setDualBound(2.0);

    SolveResult.Termination termination = new SolveResult.Termination(proto.build());

    assertThat(termination.getReason()).isEqualTo(SolveResult.TerminationReason.OPTIMAL);
    assertThat(termination.getLimit()).hasValue(SolveResult.Limit.SOLUTION);
    assertThat(termination.getDetail()).isEqualTo("detail");
    assertFalse(termination.limitReached());
    assertThat(termination.getProblemStatus().getPrimalStatus())
        .isEqualTo(SolveResult.FeasibilityStatus.FEASIBLE);
    assertThat(termination.getProblemStatus().getDualStatus())
        .isEqualTo(SolveResult.FeasibilityStatus.INFEASIBLE);
    assertFalse(termination.getProblemStatus().getPrimalOrDualInfeasible());
    assertThat(termination.getObjectiveBounds().getPrimalBound()).isEqualTo(1.0);
    assertThat(termination.getObjectiveBounds().getDualBound()).isEqualTo(2.0);
  }

  @Test
  public void solveResult_validConstruction() {
    Model model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    LinearConstraint lc = model.addLinearConstraint("lc_1");
    TerminationProto.Builder termination =
        TerminationProto.newBuilder()
            .setReason(TerminationReasonProto.TERMINATION_REASON_OPTIMAL)
            .setLimit(LimitProto.LIMIT_SOLUTION)
            .setDetail("detail");
    termination.getProblemStatusBuilder()
        .setPrimalStatus(FeasibilityStatusProto.FEASIBILITY_STATUS_FEASIBLE)
        .setDualStatus(FeasibilityStatusProto.FEASIBILITY_STATUS_FEASIBLE);
    termination.getObjectiveBoundsBuilder().setPrimalBound(10.0).setDualBound(10.0);
    PrimalSolutionProto.Builder primalSolution =
        PrimalSolutionProto.newBuilder().setObjectiveValue(1.0).setFeasibilityStatus(
            SolutionStatusProto.SOLUTION_STATUS_FEASIBLE);
    primalSolution.getVariableValuesBuilder().addIds(x.getId()).addValues(2.0);
    DualSolutionProto.Builder dualSolution =
        DualSolutionProto.newBuilder().setObjectiveValue(3.0).setFeasibilityStatus(
            SolutionStatusProto.SOLUTION_STATUS_FEASIBLE);
    dualSolution.getDualValuesBuilder().addIds(lc.getId()).addValues(4.0);
    dualSolution.getReducedCostsBuilder().addIds(x.getId()).addValues(5.0);
    BasisProto.Builder basis = BasisProto.newBuilder().setBasicDualFeasibility(
        SolutionStatusProto.SOLUTION_STATUS_FEASIBLE);
    basis.getVariableStatusBuilder().addIds(x.getId()).addValues(
        BasisStatusProto.BASIS_STATUS_AT_LOWER_BOUND);
    basis.getConstraintStatusBuilder()
        .addIds(lc.getId())
        .addValues(BasisStatusProto.BASIS_STATUS_FREE);
    SolutionProto solution = SolutionProto.newBuilder()
                                 .setPrimalSolution(primalSolution.build())
                                 .setDualSolution(dualSolution.build())
                                 .setBasis(basis.build())
                                 .build();
    PrimalRayProto.Builder primalRay = PrimalRayProto.newBuilder();
    primalRay.getVariableValuesBuilder().addIds(x.getId()).addValues(6.0);
    DualRayProto.Builder dualRay = DualRayProto.newBuilder();
    dualRay.getDualValuesBuilder().addIds(lc.getId()).addValues(7.0);
    dualRay.getReducedCostsBuilder().addIds(x.getId()).addValues(8.0);
    SolveStatsProto.Builder solveStats = SolveStatsProto.newBuilder();
    solveStats.getSolveTimeBuilder().setSeconds(11);
    GScipOutput gscipOutput =
        GScipOutput.newBuilder().setStatus(GScipOutput.Status.NODE_LIMIT).build();

    SolveResultProto proto = SolveResultProto.newBuilder()
                                 .setTermination(termination)
                                 .addSolutions(solution)
                                 .addPrimalRays(primalRay.build())
                                 .addDualRays(dualRay.build())
                                 .setSolveStats(solveStats.build())
                                 .setGscipOutput(gscipOutput)
                                 .build();

    SolveResult solveResult = new SolveResult(model, proto);

    assertThat(solveResult.getTermination().getReason())
        .isEqualTo(SolveResult.TerminationReason.OPTIMAL);
    assertThat(solveResult.getTermination().getLimit()).hasValue(SolveResult.Limit.SOLUTION);
    assertThat(solveResult.getTermination().getDetail()).isEqualTo("detail");
    assertThat(solveResult.getTermination().getObjectiveBounds().getPrimalBound()).isEqualTo(10.0);
    assertThat(solveResult.getTermination().getObjectiveBounds().getDualBound()).isEqualTo(10.0);
    assertThat(solveResult.getSolveTime()).isEqualTo(Duration.ofSeconds(11));
    assertThat(solveResult.getSolutions()).hasSize(1);
    assertThat(solveResult.getPrimalRays()).hasSize(1);
    assertThat(solveResult.getDualRays()).hasSize(1);
    assertThat(solveResult.getGscipSolverSpecificOutput().getStatus())
        .isEqualTo(GScipOutput.Status.NODE_LIMIT);
    assertThat(solveResult.getOsqpSolverSpecificOutput()).isNull();
    assertThat(solveResult.getPdlpSolverSpecificOutput()).isNull();
    assertTrue(solveResult.hasPrimalFeasibleSolution());
    assertThat(solveResult.getObjectiveValue()).isEqualTo(1.0);
    assertThat(solveResult.getBestObjectiveBound()).isEqualTo(10.0);
    assertThat(solveResult.getPrimalBound()).isEqualTo(10.0);
    assertThat(solveResult.getDualBound()).isEqualTo(10.0);
    assertThat(solveResult.getVariableValues()).containsExactly(x, 2.0);
    assertTrue(solveResult.isBounded());
    assertTrue(solveResult.hasRay());
    assertThat(solveResult.getRayVariableValues()).containsExactly(x, 6.0);
    assertTrue(solveResult.hasDualFeasibleSolution());
    assertThat(solveResult.getDualValues()).containsExactly(lc, 4.0);
    assertThat(solveResult.getReducedCosts()).containsExactly(x, 5.0);
    assertTrue(solveResult.hasDualRay());
    assertThat(solveResult.getRayDualValues()).containsExactly(lc, 7.0);
    assertThat(solveResult.getRayReducedCosts()).containsExactly(x, 8.0);
    assertThat(solveResult.getConstraintStatus()).containsExactly(lc, Basis.BasisStatus.FREE);
    assertThat(solveResult.getVariableStatus())
        .containsExactly(x, Basis.BasisStatus.AT_LOWER_BOUND);
  }

  private SolveResultProto.Builder minimalValidProto() {
    TerminationProto.Builder termination = TerminationProto.newBuilder();
    termination.setReason(TerminationReasonProto.TERMINATION_REASON_OPTIMAL);
    termination.getProblemStatusBuilder()
        .setPrimalStatus(FeasibilityStatusProto.FEASIBILITY_STATUS_FEASIBLE)
        .setDualStatus(FeasibilityStatusProto.FEASIBILITY_STATUS_FEASIBLE);
    PrimalSolutionProto primalSolution =
        PrimalSolutionProto.newBuilder()
            .setFeasibilityStatus(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE)
            .build();
    DualSolutionProto dualSolution =
        DualSolutionProto.newBuilder()
            .setFeasibilityStatus(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE)
            .build();
    BasisProto basis = BasisProto.newBuilder()
                           .setBasicDualFeasibility(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE)
                           .build();
    SolutionProto solution = SolutionProto.newBuilder()
                                 .setPrimalSolution(primalSolution)
                                 .setDualSolution(dualSolution)
                                 .setBasis(basis)
                                 .build();

    return SolveResultProto.newBuilder()
        .setTermination(termination.build())
        .addSolutions(solution)
        .addPrimalRays(PrimalRayProto.getDefaultInstance())
        .addDualRays(DualRayProto.getDefaultInstance())
        .setGscipOutput(GScipOutput.getDefaultInstance());
  }

  @Test
  public void solveResult_osqpSolverSpecificOutput() {
    Model model = new Model("test_model");
    SolveResultProto.Builder proto = minimalValidProto();
    proto.setOsqpOutput(OsqpOutput.newBuilder().setInitializedUnderlyingSolver(true).build());

    SolveResult solveResult = new SolveResult(model, proto.build());

    assertThat(solveResult.getGscipSolverSpecificOutput()).isNull();
    assertThat(solveResult.getOsqpSolverSpecificOutput()).isNotNull();
    assertTrue(solveResult.getOsqpSolverSpecificOutput().getInitializedUnderlyingSolver());
    assertThat(solveResult.getPdlpSolverSpecificOutput()).isNull();
  }

  @Test
  public void solveResult_pdlpSolverSpecificOutput() {
    Model model = new Model("test_model");
    SolveResultProto.Builder proto = minimalValidProto();
    proto.setPdlpOutput(SolveResultProto.PdlpOutput.newBuilder()
            .setConvergenceInformation(
                ConvergenceInformation.newBuilder().setCorrectedDualObjective(2.0).build())
            .build());

    SolveResult solveResult = new SolveResult(model, proto.build());

    assertThat(solveResult.getGscipSolverSpecificOutput()).isNull();
    assertThat(solveResult.getOsqpSolverSpecificOutput()).isNull();
    assertThat(solveResult.getPdlpSolverSpecificOutput()
                   .getConvergenceInformation()
                   .getCorrectedDualObjective())
        .isEqualTo(2.0);
  }

  @Test
  public void solveResult_hasPrimalFeasibleSolution() {
    Model model = new Model("test_model");
    SolveResultProto emptySolutionProto = minimalValidProto().clearSolutions().build();
    SolveResultProto primalFeasibleProto = minimalValidProto().build();
    SolveResultProto.Builder primalInfeasibleProto = minimalValidProto();
    primalInfeasibleProto.getSolutionsBuilder(0).getPrimalSolutionBuilder().setFeasibilityStatus(
        SolutionStatusProto.SOLUTION_STATUS_INFEASIBLE);

    SolveResult emptySolutionSolveResult = new SolveResult(model, emptySolutionProto);
    SolveResult primalFeasibleSolveResult = new SolveResult(model, primalFeasibleProto);
    SolveResult primalInfeasibleSolveResult = new SolveResult(model, primalInfeasibleProto.build());

    assertFalse(emptySolutionSolveResult.hasPrimalFeasibleSolution());
    assertTrue(primalFeasibleSolveResult.hasPrimalFeasibleSolution());
    assertFalse(primalInfeasibleSolveResult.hasPrimalFeasibleSolution());
  }

  @Test
  public void solveResult_hasDualFeasibleSolution() {
    Model model = new Model("test_model");
    SolveResultProto emptySolutionProto = minimalValidProto().clearSolutions().build();
    SolveResultProto dualFeasibleProto = minimalValidProto().build();
    SolveResultProto.Builder dualInfeasibleProto = minimalValidProto();
    dualInfeasibleProto.getSolutionsBuilder(0).getDualSolutionBuilder().setFeasibilityStatus(
        SolutionStatusProto.SOLUTION_STATUS_INFEASIBLE);

    SolveResult emptySolutionSolveResult = new SolveResult(model, emptySolutionProto);
    SolveResult dualFeasibleSolveResult = new SolveResult(model, dualFeasibleProto);
    SolveResult dualInfeasibleSolveResult = new SolveResult(model, dualInfeasibleProto.build());

    assertFalse(emptySolutionSolveResult.hasDualFeasibleSolution());
    assertTrue(dualFeasibleSolveResult.hasDualFeasibleSolution());
    assertFalse(dualInfeasibleSolveResult.hasDualFeasibleSolution());
  }

  @Test
  public void solveResult_hasBasis() {
    Model model = new Model("test_model");
    SolveResultProto emptySolutionProto = minimalValidProto().clearSolutions().build();
    SolveResultProto hasBasisProto = minimalValidProto().build();

    SolveResult emptySolutionSolveResult = new SolveResult(model, emptySolutionProto);
    SolveResult hasBasisSolveResult = new SolveResult(model, hasBasisProto);

    assertFalse(emptySolutionSolveResult.hasBasis());
    assertTrue(hasBasisSolveResult.hasBasis());
  }

  @Test
  public void solveResult_invalidSolution_throws() {
    Model model = new Model("test_model");
    SolveResultProto.Builder proto = minimalValidProto();
    proto.getSolutionsBuilder(0).getPrimalSolutionBuilder().clearFeasibilityStatus();

    assertThrows(IllegalArgumentException.class, () -> new SolveResult(model, proto.build()));
  }

  @Test
  public void solveResult_invalidPrimalRay_throws() {
    Model model = new Model("test_model");
    SolveResultProto.Builder proto = minimalValidProto();
    proto.getPrimalRaysBuilder(0).getVariableValuesBuilder().addValues(1.0);

    assertThrows(IllegalArgumentException.class, () -> new SolveResult(model, proto.build()));
  }

  @Test
  public void solveResult_invalidDualRay_throws() {
    Model model = new Model("test_model");
    SolveResultProto.Builder proto = minimalValidProto();
    proto.getDualRaysBuilder(0).getDualValuesBuilder().addValues(1.0);

    assertThrows(IllegalArgumentException.class, () -> new SolveResult(model, proto.build()));
  }

  @Test
  public void solveResult_invalidTermination_throws() {
    Model model = new Model("test_model");
    SolveResultProto.Builder proto = minimalValidProto();
    proto.getTerminationBuilder().clearReason();

    assertThrows(IllegalArgumentException.class, () -> new SolveResult(model, proto.build()));
  }

  @Test
  public void solveResult_invalidTerminationProblemStatus_throws() {
    Model model = new Model("test_model");
    SolveResultProto.Builder proto = minimalValidProto();
    proto.getTerminationBuilder().clearProblemStatus();

    assertThrows(IllegalArgumentException.class, () -> new SolveResult(model, proto.build()));
  }

  @Test
  public void solveResult_missingTerminationProblemStatus_recoveredFromSolveStats() {
    Model model = new Model("test_model");
    SolveResultProto.Builder proto = minimalValidProto();
    proto.getSolveStatsBuilder().setProblemStatus(proto.getTermination().getProblemStatus());
    proto.getTerminationBuilder().clearProblemStatus();

    SolveResult solveResult = new SolveResult(model, proto.build());
    assertThat(solveResult.getTermination().getProblemStatus().getPrimalStatus())
        .isEqualTo(SolveResult.FeasibilityStatus.FEASIBLE);
    assertThat(solveResult.getTermination().getProblemStatus().getDualStatus())
        .isEqualTo(SolveResult.FeasibilityStatus.FEASIBLE);
    assertFalse(solveResult.getTermination().getProblemStatus().getPrimalOrDualInfeasible());
  }

  @Test
  public void solveResult_missingTerminationObjectiveBounds_recoveredFromSolveStats() {
    Model model = new Model("test_model");
    SolveResultProto.Builder proto = minimalValidProto();
    proto.getSolveStatsBuilder().setBestPrimalBound(10.0).setBestDualBound(20.0);
    proto.getTerminationBuilder().clearObjectiveBounds();

    SolveResult solveResult = new SolveResult(model, proto.build());
    assertThat(solveResult.getTermination().getObjectiveBounds().getPrimalBound()).isEqualTo(10.0);
    assertThat(solveResult.getTermination().getObjectiveBounds().getDualBound()).isEqualTo(20.0);
  }

  @Test
  public void solveResult_emptySolverSpecificOutput_ok() {
    Model model = new Model("test_model");
    SolveResultProto.Builder proto = minimalValidProto();
    proto.clearGscipOutput();

    var result = new SolveResult(model, proto.build());

    assertThat(result.getGscipSolverSpecificOutput()).isNull();
    assertThat(result.getOsqpSolverSpecificOutput()).isNull();
    assertThat(result.getPdlpSolverSpecificOutput()).isNull();
  }
}
