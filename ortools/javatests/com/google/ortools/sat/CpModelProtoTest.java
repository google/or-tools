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

package com.google.ortools.sat;

import static com.google.common.truth.Truth.assertThat;

import com.google.ortools.Loader;
import com.google.ortools.sat.ConstraintProto;
import com.google.ortools.sat.CpModelProto;
import com.google.ortools.sat.CpObjectiveProto;
import com.google.ortools.sat.CpSolverResponse;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.DecisionStrategyProto;
import com.google.ortools.sat.IntegerVariableProto;
import com.google.ortools.sat.IntervalConstraintProto;
import com.google.ortools.sat.LinearConstraintProto;
import com.google.ortools.sat.PartialVariableAssignment;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

/** Tests for the Java CpModelProto. */
public final class CpModelProtoTest {
  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void testCpModelProtoCreation() {
    CpModelProto.Builder model = CpModelProto.newBuilder();
    model.setName("test_model");

    IntegerVariableProto.Builder var = model.addVariablesBuilder();
    var.setName("x");
    var.addDomain(0);
    var.addDomain(10);

    assertThat(model.getName()).isEqualTo("test_model");
    assertThat(model.getVariablesCount()).isEqualTo(1);
    assertThat(model.getVariables(0).getName()).isEqualTo("x");
    assertThat(model.getVariables(0).getDomainList()).containsExactly(0L, 10L).inOrder();
  }

  @Test
  public void testConstraintProtoModification() {
    ConstraintProto.Builder constraint = ConstraintProto.newBuilder();
    constraint.setName("bool_or_constraint");
    constraint.getBoolOrBuilder().addLiterals(1).addLiterals(-2);

    assertThat(constraint.getName()).isEqualTo("bool_or_constraint");
    assertThat(constraint.hasBoolOr()).isTrue();
    assertThat(constraint.getBoolOr().getLiteralsList()).containsExactly(1, -2).inOrder();

    // Modification
    constraint.getBoolOrBuilder().setLiterals(0, 5);
    assertThat(constraint.getBoolOr().getLiterals(0)).isEqualTo(5);

    // Enforcement literal
    constraint.addEnforcementLiteral(10);
    assertThat(constraint.getEnforcementLiteralCount()).isEqualTo(1);
    assertThat(constraint.getEnforcementLiteral(0)).isEqualTo(10);
  }

  @Test
  public void testLinearConstraint() {
    LinearConstraintProto.Builder linear = LinearConstraintProto.newBuilder();
    linear.addVars(1).addVars(2);
    linear.addCoeffs(10).addCoeffs(20);
    linear.addDomain(0).addDomain(100);

    assertThat(linear.getVarsCount()).isEqualTo(2);
    assertThat(linear.getCoeffsList()).containsExactly(10L, 20L).inOrder();
    assertThat(linear.getDomainList()).containsExactly(0L, 100L).inOrder();
  }

  @Test
  public void testEnums() {
    // VariableSelectionStrategy enum
    DecisionStrategyProto.VariableSelectionStrategy strategy =
        DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_LOWEST_MIN;
    assertThat(strategy.getNumber()).isEqualTo(1);
    assertThat(DecisionStrategyProto.VariableSelectionStrategy.forNumber(1)).isEqualTo(strategy);
    assertThat(DecisionStrategyProto.VariableSelectionStrategy.valueOf("CHOOSE_LOWEST_MIN"))
        .isEqualTo(strategy);

    // DomainReductionStrategy enum
    DecisionStrategyProto.DomainReductionStrategy domainStrategy =
        DecisionStrategyProto.DomainReductionStrategy.SELECT_MAX_VALUE;
    assertThat(domainStrategy.getNumber()).isEqualTo(1);

    // CpSolverStatus enum
    CpSolverStatus status = CpSolverStatus.OPTIMAL;
    assertThat(status.getNumber()).isEqualTo(4);
    assertThat(CpSolverStatus.forNumber(4)).isEqualTo(status);
  }

  @Test
  public void testNestedMessages() {
    CpModelProto.Builder model = CpModelProto.newBuilder();
    ConstraintProto.Builder ct = model.addConstraintsBuilder();
    ct.getLinearBuilder().addVars(0).addCoeffs(1).addDomain(0).addDomain(1);

    assertThat(model.getConstraintsCount()).isEqualTo(1);
    assertThat(model.getConstraints(0).hasLinear()).isTrue();
    assertThat(model.getConstraints(0).getLinear().getVars(0)).isEqualTo(0);
  }

  @Test
  public void testObjectiveModification() {
    CpObjectiveProto.Builder objective = CpObjectiveProto.newBuilder();
    objective.addVars(1).addCoeffs(5L);
    objective.setOffset(10.0);
    objective.setScalingFactor(-1.0);

    assertThat(objective.getVars(0)).isEqualTo(1);
    assertThat(objective.getCoeffs(0)).isEqualTo(5L);
    assertThat(objective.getOffset()).isEqualTo(10.0);
    assertThat(objective.getScalingFactor()).isEqualTo(-1.0);
  }

  @Test
  public void testIntervalConstraint() {
    IntervalConstraintProto.Builder interval = IntervalConstraintProto.newBuilder();
    interval.getStartBuilder().addVars(1).addCoeffs(1).setOffset(0);
    interval.getSizeBuilder().setOffset(10);
    interval.getEndBuilder().addVars(1).addCoeffs(1).setOffset(10);

    assertThat(interval.hasStart()).isTrue();
    assertThat(interval.getStart().getVars(0)).isEqualTo(1);
    assertThat(interval.getSize().getOffset()).isEqualTo(10L);
    assertThat(interval.getEnd().getOffset()).isEqualTo(10L);
  }

  @Test
  public void testPartialVariableAssignment() {
    PartialVariableAssignment.Builder assignment = PartialVariableAssignment.newBuilder();
    assignment.addVars(1).addValues(10L);
    assignment.addVars(2).addValues(20L);

    assertThat(assignment.getVarsCount()).isEqualTo(2);
    assertThat(assignment.getValuesList()).containsExactly(10L, 20L).inOrder();
  }

  @Test
  public void testCpSolverResponse() {
    CpSolverResponse.Builder response = CpSolverResponse.newBuilder();
    response.setStatus(CpSolverStatus.FEASIBLE);
    response.addSolution(1).addSolution(2).addSolution(3);
    response.setObjectiveValue(100.5);

    assertThat(response.getStatus()).isEqualTo(CpSolverStatus.FEASIBLE);
    assertThat(response.getSolutionCount()).isEqualTo(3);
    assertThat(response.getObjectiveValue()).isEqualTo(100.5);
  }
}
