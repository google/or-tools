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

import com.google.common.collect.ImmutableList;
import com.google.ortools.Loader;
import com.google.ortools.mathopt.CallbackDataProto;
import com.google.ortools.mathopt.CallbackEventProto;
import com.google.ortools.mathopt.CallbackRegistrationProto;
import com.google.ortools.mathopt.CallbackResultProto;
import com.google.ortools.mathopt.ModelProto;
import com.google.ortools.mathopt.ModelSolveParametersProto;
import com.google.ortools.mathopt.SolveParametersProto;
import com.google.ortools.mathopt.SolveResultProto;
import com.google.ortools.mathopt.SolverTypeProto;
import com.google.ortools.mathopt.TerminationReasonProto;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Optional;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public class ScopedCppSolveCallbackTest {
  @BeforeEach
  public void loadJni() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void new_withEmptyFunction_swiggedObjectIsNull() {
    var cb = new ScopedCppSolveCallback(Optional.empty());
    assertThat(cb.getCppSolveCallback()).isNull();
  }

  @Test
  public void new_withNullConsumer_closeOk() {
    var cb = new ScopedCppSolveCallback(Optional.empty());
    cb.close();
    assertThat(cb.getCppSolveCallback()).isNull();
  }

  @Test
  public void new_withConsumer_closeOnceOk() {
    var cb = new ScopedCppSolveCallback(
        Optional.of((cbData) -> CallbackResultProto.getDefaultInstance()));
    assertThat(cb.getCppSolveCallback()).isNotNull();
    cb.close();
    assertThat(cb.getCppSolveCallback()).isNull();
  }

  @Test
  public void new_withConsumer_closeTwiceOk() {
    var cb = new ScopedCppSolveCallback(
        Optional.of((cbData) -> CallbackResultProto.getDefaultInstance()));
    cb.close();
    cb.close();
    assertThat(cb.getCppSolveCallback()).isNull();
  }

  // Instead of calling solve, a better test would be to add a test function in C++ that takes
  // in a callback and a CallbackDataProto and invokes the callback on the proto, swig wrap this
  // function, and call it from the test.
  @Test
  public void invokeCallback_solveSimpleModel_hasOptimalString() {
    // max x
    // x binary
    ModelProto.Builder model = ModelProto.newBuilder();
    model.getVariablesBuilder()
        .addIds(0)
        .addLowerBounds(0.0)
        .addUpperBounds(1.0)
        .addIntegers(true)
        .addNames("x");
    model.getObjectiveBuilder()
        .setMaximize(true)
        .getLinearCoefficientsBuilder()
        .addIds(0)
        .addValues(1.0);

    List<CallbackDataProto> allCallbackInvocations =
        Collections.synchronizedList(new ArrayList<>());
    var registration = CallbackRegistrationProto.newBuilder()
                           .addRequestRegistration(CallbackEventProto.CALLBACK_EVENT_MIP_SOLUTION)
                           .build();
    try (var callback = new ScopedCppSolveCallback(Optional.of(callbackDataProto -> {
      allCallbackInvocations.add(callbackDataProto);
      return CallbackResultProto.getDefaultInstance();
    }))) {
      SolveResultProto result = JniSolver.cppSolve(model.build(),
          SolverTypeProto.SOLVER_TYPE_CP_SAT, SolveParametersProto.getDefaultInstance(),
          ModelSolveParametersProto.getDefaultInstance(), null, registration,
          callback.getCppSolveCallback(), null);
      assertThat(result.getTermination().getReason())
          .isEqualTo(TerminationReasonProto.TERMINATION_REASON_OPTIMAL);
    }
    boolean foundOptimal = false;
    for (CallbackDataProto cbData : allCallbackInvocations) {
      assertThat(cbData.getEvent()).isEqualTo(CallbackEventProto.CALLBACK_EVENT_MIP_SOLUTION);
      assertThat(cbData.hasPrimalSolutionVector()).isTrue();
      assertThat(cbData.getPrimalSolutionVector().getIdsList()).containsExactly(0L);
      if (cbData.getPrimalSolutionVector().getValuesList().equals(ImmutableList.of(1.0))) {
        foundOptimal = true;
      }
    }
    assertThat(foundOptimal).isTrue();
  }
}
