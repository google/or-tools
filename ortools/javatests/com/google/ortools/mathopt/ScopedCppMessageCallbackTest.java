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
import static com.google.ortools.mathopt.testing.StringCorrespondences.hasSubstring;

import com.google.ortools.Loader;
import com.google.ortools.mathopt.CallbackRegistrationProto;
import com.google.ortools.mathopt.ModelProto;
import com.google.ortools.mathopt.ModelSolveParametersProto;
import com.google.ortools.mathopt.SolveParametersProto;
import com.google.ortools.mathopt.SolveResultProto;
import com.google.ortools.mathopt.SolverTypeProto;
import com.google.ortools.mathopt.TerminationReasonProto;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class ScopedCppMessageCallbackTest {
  @BeforeEach
  public void loadJni() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void new_withEmptyConsumer_swiggedObjectIsNull() {
    var cb = new ScopedCppMessageCallback(Optional.empty());
    assertThat(cb.getCppMessageCallback()).isNull();
  }

  @Test
  public void new_withEmptyConsumer_closeOk() {
    var cb = new ScopedCppMessageCallback(Optional.empty());
    cb.close();
    assertThat(cb.getCppMessageCallback()).isNull();
  }

  @Test
  public void new_withConsumer_closeOnceOk() {
    var cb = new ScopedCppMessageCallback(Optional.of((messages) -> {}));
    assertThat(cb.getCppMessageCallback()).isNotNull();
    cb.close();
    assertThat(cb.getCppMessageCallback()).isNull();
  }

  @Test
  public void new_withConsumer_closeTwiceOk() {
    var cb = new ScopedCppMessageCallback(Optional.of((messages) -> {}));
    cb.close();
    cb.close();
    assertThat(cb.getCppMessageCallback()).isNull();
  }

  // Instead of calling solve, a better test would be to add a test function in C++ that takes
  // in a callback and a list of strings and invokes the callback in sequence on these strings, swig
  // wrap this function, and call it from the test.
  @Test
  public void invokeCallback_solveSimpleModel_hasOptimalString() {
    // max 2*x
    // s.t. 0 <= x <= 1
    ModelProto.Builder model = ModelProto.newBuilder();
    model.getVariablesBuilder()
        .addIds(0)
        .addLowerBounds(0.0)
        .addUpperBounds(1.0)
        .addIntegers(false)
        .addNames("x");
    model.getObjectiveBuilder()
        .setMaximize(true)
        .getLinearCoefficientsBuilder()
        .addIds(0)
        .addValues(2.0);

    List<String> allMessages = new ArrayList<>();
    try (var messageCallback = new ScopedCppMessageCallback(Optional.of(allMessages::addAll))) {
      SolveResultProto result = JniSolver.cppSolve(model.build(), SolverTypeProto.SOLVER_TYPE_GLOP,
          SolveParametersProto.getDefaultInstance(), ModelSolveParametersProto.getDefaultInstance(),
          messageCallback.getCppMessageCallback(), CallbackRegistrationProto.getDefaultInstance(),
          null, null);
      assertThat(result.getTermination().getReason())
          .isEqualTo(TerminationReasonProto.TERMINATION_REASON_OPTIMAL);
    }
    assertThat(allMessages).comparingElementsUsing(hasSubstring("status: OPTIMAL")).contains(true);
  }
}
