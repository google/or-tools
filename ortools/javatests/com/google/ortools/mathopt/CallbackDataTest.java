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
import static com.google.common.truth.extensions.proto.ProtoTruth.assertThat;
import static com.google.ortools.util.ProtoDurationConversion.toProtoDuration;
import static org.junit.jupiter.api.Assertions.assertThrows;

import com.google.common.collect.ImmutableMap;
import com.google.ortools.mathopt.CallbackDataProto.BarrierStats;
import com.google.ortools.mathopt.CallbackDataProto.MipStats;
import com.google.ortools.mathopt.CallbackDataProto.PresolveStats;
import com.google.ortools.mathopt.CallbackDataProto.SimplexStats;
import java.time.Duration;
import org.junit.jupiter.api.Test;

public final class CallbackDataTest {
  @Test
  public void new_withSolution_hasData() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    var cbDataProto = CallbackDataProto.newBuilder()
                          .setEvent(CallbackEventProto.CALLBACK_EVENT_MIP_NODE)
                          .setRuntime(toProtoDuration(Duration.ofSeconds(5)))
                          .setPrimalSolutionVector(SparseDoubleVectorProto.newBuilder()
                                  .addIds(0)
                                  .addValues(5.0)
                                  .addIds(1)
                                  .addValues(10.0)
                                  .build())
                          .setMipStats(MipStats.newBuilder().setOpenNodes(10).build())
                          .build();

    var cbData = new CallbackData(model, cbDataProto);

    assertThat(cbData.getEvent()).isEqualTo(CallbackEvent.MIP_NODE);
    assertThat(cbData.getRuntime()).isEqualTo(Duration.ofSeconds(5));
    assertThat(cbData.getVariableValues()).hasValue(ImmutableMap.of(x, 5.0, y, 10.0));
    assertThat(cbData.getMipStats()).isEqualTo(MipStats.newBuilder().setOpenNodes(10).build());
    assertThat(cbData.getBarrierStats()).isEqualToDefaultInstance();
    assertThat(cbData.getPresolveStats()).isEqualToDefaultInstance();
    assertThat(cbData.getSimplexStats()).isEqualToDefaultInstance();
  }

  @Test
  public void new_modelMissingVariableInSolution_throws() {
    var model = new Model();
    var unused = model.addVariable();
    var cbDataProto =
        CallbackDataProto.newBuilder()
            .setEvent(CallbackEventProto.CALLBACK_EVENT_MIP_NODE)
            .setRuntime(toProtoDuration(Duration.ofSeconds(5)))
            .setPrimalSolutionVector(
                SparseDoubleVectorProto.newBuilder().addIds(4).addValues(10.0).build())
            .build();

    assertThrows(IllegalArgumentException.class, () -> new CallbackData(model, cbDataProto));
  }

  @Test
  public void new_noSolution_hasData() {
    var model = new Model();
    var unused = model.addVariable();
    var cbDataProto = CallbackDataProto.newBuilder()
                          .setEvent(CallbackEventProto.CALLBACK_EVENT_PRESOLVE)
                          .setPresolveStats(PresolveStats.newBuilder().setBoundChanges(2).build())
                          .build();

    var cbData = new CallbackData(model, cbDataProto);

    assertThat(cbData.getEvent()).isEqualTo(CallbackEvent.PRESOLVE);
    assertThat(cbData.getRuntime()).isEqualTo(Duration.ZERO);
    assertThat(cbData.getVariableValues()).isEmpty();
    assertThat(cbData.getPresolveStats())
        .isEqualTo(PresolveStats.newBuilder().setBoundChanges(2).build());
  }

  @Test
  public void new_withBarrierStats_hasData() {
    var model = new Model();
    var cbDataProto = CallbackDataProto.newBuilder()
                          .setEvent(CallbackEventProto.CALLBACK_EVENT_BARRIER)
                          .setBarrierStats(BarrierStats.newBuilder().setIterationCount(3).build())
                          .build();

    var cbData = new CallbackData(model, cbDataProto);

    assertThat(cbData.getEvent()).isEqualTo(CallbackEvent.BARRIER);
    assertThat(cbData.getBarrierStats())
        .isEqualTo(BarrierStats.newBuilder().setIterationCount(3).build());
  }

  @Test
  public void new_withSimplexStats_hasData() {
    var model = new Model();
    var cbDataProto = CallbackDataProto.newBuilder()
                          .setEvent(CallbackEventProto.CALLBACK_EVENT_SIMPLEX)
                          .setSimplexStats(SimplexStats.newBuilder().setIterationCount(5).build())
                          .build();

    var cbData = new CallbackData(model, cbDataProto);

    assertThat(cbData.getEvent()).isEqualTo(CallbackEvent.SIMPLEX);
    assertThat(cbData.getSimplexStats())
        .isEqualTo(SimplexStats.newBuilder().setIterationCount(5).build());
  }
}
