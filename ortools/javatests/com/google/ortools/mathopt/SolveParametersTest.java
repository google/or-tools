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

import com.google.ortools.GScipParameters;
import com.google.ortools.glop.GlopParameters;
import com.google.ortools.pdlp.PrimalDualHybridGradientParams;
import com.google.ortools.sat.SatParameters;
import java.time.Duration;
import org.junit.jupiter.api.Test;

public final class SolveParametersTest {
  @Test
  public void lpAlgorithm_toProto() {
    for (LPAlgorithmProto proto : LPAlgorithmProto.values()) {
      if (proto.equals(LPAlgorithmProto.LP_ALGORITHM_UNSPECIFIED)
          || proto.equals(LPAlgorithmProto.UNRECOGNIZED)) {
        continue;
      }
      assertThat(SolveParameters.LPAlgorithm.fromProto(proto).get().toProto()).isEqualTo(proto);
    }
  }

  @Test
  public void lpAlgorithm_unspecified() {
    assertThat(SolveParameters.LPAlgorithm.fromProto(LPAlgorithmProto.LP_ALGORITHM_UNSPECIFIED))
        .isEmpty();
  }

  @Test
  public void emphasis_toProto() {
    for (EmphasisProto proto : EmphasisProto.values()) {
      if (proto.equals(EmphasisProto.EMPHASIS_UNSPECIFIED)
          || proto.equals(EmphasisProto.UNRECOGNIZED)) {
        continue;
      }
      assertThat(SolveParameters.Emphasis.fromProto(proto).get().toProto()).isEqualTo(proto);
    }
  }

  @Test
  public void emphasis_unspecified() {
    assertThat(SolveParameters.Emphasis.fromProto(EmphasisProto.EMPHASIS_UNSPECIFIED)).isEmpty();
  }

  @Test
  public void gurobiParameters_emptyProtoRoundTrip() {
    GurobiParametersProto proto = GurobiParametersProto.getDefaultInstance();
    SolveParameters.GurobiParameters params = new SolveParameters.GurobiParameters(proto);

    assertThat(params.toProto()).isEqualTo(proto);
  }

  @Test
  public void gurobiParameters_protoRoundTrip() {
    GurobiParametersProto proto = GurobiParametersProto.newBuilder()
                                      .addParameters(GurobiParametersProto.Parameter.newBuilder()
                                              .setName("name1")
                                              .setValue("value1")
                                              .build())
                                      .addParameters(GurobiParametersProto.Parameter.newBuilder()
                                              .setName("name2")
                                              .setValue("value2")
                                              .build())
                                      .build();
    SolveParameters.GurobiParameters params = new SolveParameters.GurobiParameters(proto);

    assertThat(params.toProto()).isEqualTo(proto);
  }

  @Test
  public void gurobiParameters_addParamValue() {
    SolveParameters.GurobiParameters params = new SolveParameters.GurobiParameters()
                                                  .addParamValue("name1", "value1")
                                                  .addParamValue("name2", "value2");

    assertThat(params.toProto())
        .isEqualTo(GurobiParametersProto.newBuilder()
                .addParameters(GurobiParametersProto.Parameter.newBuilder()
                        .setName("name1")
                        .setValue("value1")
                        .build())
                .addParameters(GurobiParametersProto.Parameter.newBuilder()
                        .setName("name2")
                        .setValue("value2")
                        .build())
                .build());
  }

  @Test
  public void glpkParameters_emptyProtoRoundTrip() {
    GlpkParametersProto proto = GlpkParametersProto.getDefaultInstance();
    SolveParameters.GlpkParameters params = new SolveParameters.GlpkParameters(proto);

    assertThat(params.toProto()).isEqualTo(proto);
  }

  @Test
  public void glpkParameters_protoRoundTrip() {
    GlpkParametersProto proto =
        GlpkParametersProto.newBuilder().setComputeUnboundRaysIfPossible(true).build();
    SolveParameters.GlpkParameters params = new SolveParameters.GlpkParameters(proto);

    assertThat(params.toProto()).isEqualTo(proto);
  }

  @Test
  public void glpkParameters_setComputeUnboundRaysIfPossible() {
    SolveParameters.GlpkParameters params =
        new SolveParameters.GlpkParameters().setComputeUnboundRaysIfPossible(true);

    assertThat(params.toProto())
        .isEqualTo(GlpkParametersProto.newBuilder().setComputeUnboundRaysIfPossible(true).build());
  }

  @Test
  public void solveParameters_emptyProtoRoundTrip() {
    SolveParametersProto proto = SolveParametersProto.getDefaultInstance();
    SolveParameters params = new SolveParameters(proto);

    assertThat(params.toProto()).isEqualTo(proto);
  }

  @Test
  public void solveParameters_commonParamsToProtoRoundTrip() {
    SolveParametersProto.Builder protoBuilder =
        SolveParametersProto.newBuilder()
            .setEnableOutput(true)
            .setIterationLimit(2)
            .setNodeLimit(3)
            .setCutoffLimit(4)
            .setObjectiveLimit(4)
            .setBestBoundLimit(5)
            .setSolutionLimit(6)
            .setThreads(7)
            .setRandomSeed(8)
            .setAbsoluteGapTolerance(10)
            .setRelativeGapTolerance(11)
            .setSolutionPoolSize(12)
            .setLpAlgorithm(LPAlgorithmProto.LP_ALGORITHM_DUAL_SIMPLEX)
            .setPresolve(EmphasisProto.EMPHASIS_OFF)
            .setCuts(EmphasisProto.EMPHASIS_LOW)
            .setHeuristics(EmphasisProto.EMPHASIS_MEDIUM)
            .setScaling(EmphasisProto.EMPHASIS_HIGH);
    protoBuilder.getTimeLimitBuilder().setSeconds(1);
    SolveParametersProto proto = protoBuilder.build();
    SolveParameters params = new SolveParameters(proto);

    assertThat(params.toProto()).isEqualTo(proto);
  }

  @Test
  public void solveParameters_commonParamsToProto() {
    SolveParameters params = new SolveParameters();
    params.setEnableOutput(true)
        .setTimeLimit(Duration.ofSeconds(1))
        .setIterationLimit(2)
        .setNodeLimit(3)
        .setCutoffLimit(4)
        .setObjectiveLimit(5)
        .setBestBoundLimit(6)
        .setSolutionLimit(7)
        .setThreads(8)
        .setRandomSeed(9)
        .setAbsoluteGapTolerance(10)
        .setRelativeGapTolerance(11)
        .setSolutionPoolSize(12)
        .setLPAlgorithm(SolveParameters.LPAlgorithm.DUAL_SIMPLEX)
        .setPresolve(SolveParameters.Emphasis.OFF)
        .setCuts(SolveParameters.Emphasis.LOW)
        .setHeuristics(SolveParameters.Emphasis.MEDIUM)
        .setScaling(SolveParameters.Emphasis.HIGH);

    SolveParametersProto.Builder protoBuilder =
        SolveParametersProto.newBuilder()
            .setEnableOutput(true)
            .setIterationLimit(2)
            .setNodeLimit(3)
            .setCutoffLimit(4)
            .setObjectiveLimit(5)
            .setBestBoundLimit(6)
            .setSolutionLimit(7)
            .setThreads(8)
            .setRandomSeed(9)
            .setAbsoluteGapTolerance(10)
            .setRelativeGapTolerance(11)
            .setSolutionPoolSize(12)
            .setLpAlgorithm(LPAlgorithmProto.LP_ALGORITHM_DUAL_SIMPLEX)
            .setPresolve(EmphasisProto.EMPHASIS_OFF)
            .setCuts(EmphasisProto.EMPHASIS_LOW)
            .setHeuristics(EmphasisProto.EMPHASIS_MEDIUM)
            .setScaling(EmphasisProto.EMPHASIS_HIGH);
    protoBuilder.getTimeLimitBuilder().setSeconds(1);

    assertThat(params.toProto()).isEqualTo(protoBuilder.build());
  }

  @Test
  public void solveParameters_solverSpecificParamsProtoSettersToProto() {
    SolveParameters params =
        new SolveParameters()
            .setGscip(GScipParameters.newBuilder().setNumSolutions(1).build())
            .setGurobiFromProto(GurobiParametersProto.newBuilder()
                    .addParameters(GurobiParametersProto.Parameter.newBuilder()
                            .setName("name")
                            .setValue("value")
                            .build())
                    .build())
            .setGlop(GlopParameters.newBuilder().setRandomSeed(2).build())
            .setCpSat(SatParameters.newBuilder().setNumWorkers(3).build())
            .setPdlp(
                PrimalDualHybridGradientParams.newBuilder().setInitialPrimalWeight(4.0).build())
            .setOsqp(OsqpSettingsProto.newBuilder().setAlpha(5.0).build())
            .setGlpkFromProto(
                GlpkParametersProto.newBuilder().setComputeUnboundRaysIfPossible(true).build())
            .setHighs(
                HighsOptionsProto.newBuilder().putBoolOptions("solve_relaxation", true).build())
            .setXpressFromProto(XpressParametersProto.newBuilder()
                    .addParameters(XpressParametersProto.Parameter.newBuilder()
                            .setName("name")
                            .setValue("value")
                            .build())
                    .build());

    SolveParametersProto.Builder protoBuilder = SolveParametersProto.newBuilder();
    protoBuilder.getGscipBuilder().setNumSolutions(1);
    protoBuilder.getGurobiBuilder().addParameters(
        GurobiParametersProto.Parameter.newBuilder().setName("name").setValue("value").build());
    protoBuilder.getGlopBuilder().setRandomSeed(2);
    protoBuilder.getCpSatBuilder().setNumWorkers(3);
    protoBuilder.getPdlpBuilder().setInitialPrimalWeight(4.0);
    protoBuilder.getOsqpBuilder().setAlpha(5.0);
    protoBuilder.getGlpkBuilder().setComputeUnboundRaysIfPossible(true);
    protoBuilder.getHighsBuilder().putBoolOptions("solve_relaxation", true);
    protoBuilder.getXpressBuilder().addParameters(
        XpressParametersProto.Parameter.newBuilder().setName("name").setValue("value").build());
    SolveParametersProto proto = protoBuilder.build();

    assertThat(params.toProto()).isEqualTo(proto);
  }

  @Test
  public void solveParameters_solverSpecificParamsToProto() {
    SolveParameters params =
        new SolveParameters()
            .setGurobi(new SolveParameters.GurobiParameters().addParamValue("name", "value"))
            .setGlpk(new SolveParameters.GlpkParameters().setComputeUnboundRaysIfPossible(true))
            .setXpress(new SolveParameters.XpressParameters().addParamValue("name", "value"));

    SolveParametersProto.Builder protoBuilder = SolveParametersProto.newBuilder();
    protoBuilder.getGurobiBuilder().addParameters(
        GurobiParametersProto.Parameter.newBuilder().setName("name").setValue("value").build());
    protoBuilder.getGlpkBuilder().setComputeUnboundRaysIfPossible(true);
    protoBuilder.getXpressBuilder().addParameters(
        XpressParametersProto.Parameter.newBuilder().setName("name").setValue("value").build());
    SolveParametersProto proto = protoBuilder.build();

    assertThat(params.toProto()).isEqualTo(proto);
  }

  @Test
  public void solveParameters_solverSpecificParamsProtoRoundTrip() {
    SolveParametersProto.Builder protoBuilder = SolveParametersProto.newBuilder();
    protoBuilder.getGscipBuilder().setNumSolutions(1);
    protoBuilder.getGurobiBuilder().addParameters(
        GurobiParametersProto.Parameter.newBuilder().setName("name").setValue("value").build());
    protoBuilder.getGlopBuilder().setRandomSeed(2);
    protoBuilder.getCpSatBuilder().setNumWorkers(3);
    protoBuilder.getPdlpBuilder().setInitialPrimalWeight(4.0);
    protoBuilder.getOsqpBuilder().setAlpha(5.0);
    protoBuilder.getGlpkBuilder().setComputeUnboundRaysIfPossible(true);
    protoBuilder.getHighsBuilder().putBoolOptions("solve_relaxation", true);
    protoBuilder.getXpressBuilder().addParameters(
        XpressParametersProto.Parameter.newBuilder().setName("name").setValue("value").build());
    SolveParameters params = new SolveParameters(protoBuilder.build());

    assertThat(params.toProto()).isEqualTo(protoBuilder.build());
  }
}
