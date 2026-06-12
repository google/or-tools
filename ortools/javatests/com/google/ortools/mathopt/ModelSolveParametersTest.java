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
import static com.google.ortools.mathopt.testing.MathOptProtoSubject.assertThat;
import static org.junit.jupiter.api.Assertions.assertThrows;

import com.google.common.collect.ImmutableMap;
import com.google.ortools.mathopt.Basis.BasisStatus;
import com.google.ortools.mathopt.BasisStatusProto;
import com.google.ortools.mathopt.ModelSolveParameters.SolutionHint;
import com.google.ortools.mathopt.ModelSolveParametersProto;
import com.google.ortools.mathopt.SolutionHintProto;
import com.google.ortools.mathopt.SolutionStatusProto;
import com.google.ortools.mathopt.SparseDoubleVectorProto;
import org.junit.jupiter.api.Test;

public class ModelSolveParametersTest {
  @Test
  public void newSolutionHint_primalAndDual_hasMembers() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    LinearConstraint c = model.addLinearConstraint();
    LinearConstraint d = model.addLinearConstraint();

    var hint =
        new SolutionHint(ImmutableMap.of(x, 10.0, y, 11.0), ImmutableMap.of(c, 12.0, d, 13.0));

    assertThat(hint.getVariableValues()).containsExactly(x, 10.0, y, 11.0);
    assertThat(hint.getDualValues()).containsExactly(c, 12.0, d, 13.0);
  }

  @Test
  public void newSolutionHint_primalOnly_hasMembers() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();

    var hint = new SolutionHint(ImmutableMap.of(x, 10.0, y, 11.0));

    assertThat(hint.getVariableValues()).containsExactly(x, 10.0, y, 11.0);
    assertThat(hint.getDualValues()).isEmpty();
  }

  @Test
  public void solutionHint_toProto() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    LinearConstraint c = model.addLinearConstraint();
    LinearConstraint d = model.addLinearConstraint();
    var hint =
        new SolutionHint(ImmutableMap.of(x, 10.0, y, 11.0), ImmutableMap.of(c, 12.0, d, 13.0));

    var expected =
        SolutionHintProto.newBuilder()
            .setVariableValues(
                SparseDoubleVectorProto.newBuilder().addIds(0).addIds(1).addValues(10.0).addValues(
                    11.0))
            .setDualValues(
                SparseDoubleVectorProto.newBuilder().addIds(0).addIds(1).addValues(12.0).addValues(
                    13.0))
            .build();

    assertThat(hint.toProto()).isEquivalentTo(expected);
  }

  @Test
  public void solutionHintFromProto_validInput_ok() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    LinearConstraint c = model.addLinearConstraint();
    LinearConstraint d = model.addLinearConstraint();
    var protoHint =
        SolutionHintProto.newBuilder()
            .setVariableValues(
                SparseDoubleVectorProto.newBuilder().addIds(0).addIds(1).addValues(10.0).addValues(
                    11.0))
            .setDualValues(
                SparseDoubleVectorProto.newBuilder().addIds(0).addIds(1).addValues(12.0).addValues(
                    13.0))
            .build();

    var hint = new SolutionHint(model, protoHint);

    assertThat(hint.getVariableValues()).containsExactly(x, 10.0, y, 11.0);
    assertThat(hint.getDualValues()).containsExactly(c, 12.0, d, 13.0);
  }

  @Test
  public void solutionHintFromProto_badVariableValues_throws() {
    var model = new Model();
    var unused = model.addVariable();
    var protoHint =
        SolutionHintProto.newBuilder()
            .setVariableValues(
                SparseDoubleVectorProto.newBuilder().addIds(0).addIds(1).addValues(10.0).addValues(
                    11.0))
            .build();

    assertThrows(IllegalArgumentException.class, () -> new SolutionHint(model, protoHint));
  }

  @Test
  public void solutionHintFromProto_badDualValues_throws() {
    var model = new Model();
    var unused = model.addLinearConstraint();
    var protoHint =
        SolutionHintProto.newBuilder()
            .setDualValues(
                SparseDoubleVectorProto.newBuilder().addIds(0).addIds(1).addValues(10.0).addValues(
                    11.0))
            .build();

    assertThrows(IllegalArgumentException.class, () -> new SolutionHint(model, protoHint));
  }

  @Test
  public void newModelSolveParameters_empty_isEmpty() {
    var modelParams = new ModelSolveParameters();

    assertThat(modelParams.getBasis()).isEmpty();
    assertThat(modelParams.getBranchingPriorities()).isEmpty();
    assertThat(modelParams.unmodifiableSolutionHints()).isEmpty();
    assertThat(modelParams.toProto())
        .isEquivalentTo(ModelSolveParametersProto.getDefaultInstance());
  }

  @Test
  public void setBasis_hasBasis() {
    var model = new Model();
    Variable x = model.addVariable();
    var basis = new Basis(ImmutableMap.of(), ImmutableMap.of(x, BasisStatus.FREE));
    var modelParams = new ModelSolveParameters();

    assertThat(modelParams.setBasis(basis)).isSameInstanceAs(modelParams);

    assertThat(modelParams.getBasis()).isPresent();
    assertThat(modelParams.getBasis().get().getVariableStatus())
        .containsExactly(x, BasisStatus.FREE);
    assertThat(modelParams.getBasis().get().getConstraintStatus()).isEmpty();
    assertThat(modelParams.getBasis().get().getBasicDualFeasibility()).isEmpty();
  }

  @Test
  public void clearBasis_isEmpty() {
    var model = new Model();
    Variable x = model.addVariable();
    var basis = new Basis(ImmutableMap.of(), ImmutableMap.of(x, BasisStatus.FREE));
    var modelParams = new ModelSolveParameters();
    modelParams.setBasis(basis);

    assertThat(modelParams.clearBasis()).isSameInstanceAs(modelParams);

    assertThat(modelParams.getBasis()).isEmpty();
  }

  @Test
  public void addHint_hasHints() {
    var model = new Model();
    Variable x = model.addVariable();
    var hint = new SolutionHint(ImmutableMap.of(x, 2.0));
    var modelParams = new ModelSolveParameters();

    assertThat(modelParams.addSolutionHint(hint)).isSameInstanceAs(modelParams);

    assertThat(modelParams.unmodifiableSolutionHints()).hasSize(1);
    assertThat(modelParams.unmodifiableSolutionHints().get(0).getVariableValues())
        .containsExactly(x, 2.0);
    assertThat(modelParams.unmodifiableSolutionHints().get(0).getDualValues()).isEmpty();
  }

  @Test
  public void clearHint_isEmpty() {
    var model = new Model();
    Variable x = model.addVariable();
    var hint = new SolutionHint(ImmutableMap.of(x, 2.0));
    var modelParams = new ModelSolveParameters();
    modelParams.addSolutionHint(hint);

    assertThat(modelParams.clearSolutionHints()).isSameInstanceAs(modelParams);

    assertThat(modelParams.unmodifiableSolutionHints()).isEmpty();
  }

  @Test
  public void setBranchingPriority_hasBranchingPriority() {
    var model = new Model();
    Variable x = model.addVariable();
    var modelParams = new ModelSolveParameters();

    assertThat(modelParams.setBranchingPriorities(ImmutableMap.of(x, 2)))
        .isSameInstanceAs(modelParams);

    assertThat(modelParams.getBranchingPriorities()).containsExactly(x, 2);
  }

  @Test
  public void protoRoundTrip_nonEmpty_roundTrips() {
    var model = new Model();
    var unused = model.addVariable();
    var unused2 = model.addLinearConstraint();

    var protoParams = ModelSolveParametersProto.newBuilder();
    var hint = protoParams.addSolutionHintsBuilder();
    hint.getVariableValuesBuilder().addIds(0).addValues(10.0);
    hint.getDualValuesBuilder().addIds(0).addValues(11.0);
    var basis = protoParams.getInitialBasisBuilder();
    basis.getVariableStatusBuilder().addIds(0).addValues(BasisStatusProto.BASIS_STATUS_BASIC);
    basis.getConstraintStatusBuilder().addIds(0).addValues(BasisStatusProto.BASIS_STATUS_FREE);
    basis.setBasicDualFeasibility(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE);
    protoParams.getBranchingPrioritiesBuilder().addIds(0).addValues(5);

    var modelParams = new ModelSolveParameters(model, protoParams.build());

    assertThat(modelParams.toProto()).isEquivalentTo(protoParams.build());
  }
}
