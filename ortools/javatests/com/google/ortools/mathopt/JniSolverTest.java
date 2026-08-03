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
import static com.google.ortools.mathopt.testing.StringCorrespondences.hasSubstring;
import static org.junit.jupiter.api.Assertions.assertThrows;

import com.google.common.base.Preconditions;
import com.google.common.base.Splitter;
import com.google.common.collect.ImmutableList;
import com.google.ortools.Loader;
import com.google.ortools.util.JavaSwigSolveInterrupter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class JniSolverTest {
  @BeforeEach
  public void loadJni() {
    Loader.loadNativeLibraries();
  }

  private static final class TestMessageCallback extends CppMessageCallback {
    private final List<String> messages = new ArrayList<>();

    public TestMessageCallback() {}

    @Override
    public void onMessage(String message) {
      for (String line : Splitter.on("\n").split(message)) {
        messages.add(line);
      }
    }

    List<String> getUnmodifiableMessages() {
      return Collections.unmodifiableList(messages);
    }
  }

  /** Returns the model max 2*x, s.t. x in [0, 1]. */
  private static ModelProto simpleModel() {
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
    return model.build();
  }

  @Test
  public void cppSolve_simpleModel_isOptimal() {
    // max 2*x
    // s.t. 0 <= x <= 1

    var callback = new TestMessageCallback();
    SolveResultProto result = JniSolver.cppSolve(simpleModel(), SolverTypeProto.SOLVER_TYPE_GLOP,
        SolveParametersProto.getDefaultInstance(), ModelSolveParametersProto.getDefaultInstance(),
        callback, CallbackRegistrationProto.getDefaultInstance(), null, null);

    assertThat(result.getTermination().getReason())
        .isEqualTo(TerminationReasonProto.TERMINATION_REASON_OPTIMAL);
    assertThat(result.getSolutionsCount()).isGreaterThan(0);
    assertThat(result.getSolutions(0).hasPrimalSolution()).isTrue();
    PrimalSolutionProto.Builder expected = PrimalSolutionProto.newBuilder();
    expected.setFeasibilityStatus(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE)
        .setObjectiveValue(2.0)
        .getVariableValuesBuilder()
        .addIds(0)
        .addValues(1.0);
    assertThat(result.getSolutions(0).getPrimalSolution())
        .usingDoubleTolerance(1.0e-4)
        .isEqualTo(expected.build());
    assertThat(callback.getUnmodifiableMessages())
        .comparingElementsUsing(hasSubstring("status: OPTIMAL"))
        .contains(true);
  }

  @Test
  public void cppSolve_invalidInput_throws() {
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
        .addIds(7) // This variable id is invalid, it will cause MathOpt model validation to error.
        .addValues(2.0);

    RuntimeException e = assertThrows(RuntimeException.class,
        ()
            -> JniSolver.cppSolve(model.build(), SolverTypeProto.SOLVER_TYPE_GLOP,
                SolveParametersProto.getDefaultInstance(),
                ModelSolveParametersProto.getDefaultInstance(), null,
                CallbackRegistrationProto.getDefaultInstance(), null, null));

    assertThat(e).hasMessageThat().contains("ModelProto.objective");
  }

  @Test
  public void javaSwigSolveInterrupter_interrupt_isInterrupted() {
    var interrupter = new JavaSwigSolveInterrupter();
    try {
      assertThat(interrupter.isInterrupted()).isFalse();
      interrupter.interrupt();
      assertThat(interrupter.isInterrupted()).isTrue();
    } finally {
      interrupter.delete();
    }
  }

  @Test
  public void cppSolve_interrupt_isInterrupted() {
    var interrupter = new JavaSwigSolveInterrupter();
    try {
      interrupter.interrupt();
      SolveResultProto result = JniSolver.cppSolve(simpleModel(), SolverTypeProto.SOLVER_TYPE_GLOP,
          SolveParametersProto.getDefaultInstance(), ModelSolveParametersProto.getDefaultInstance(),
          null, CallbackRegistrationProto.getDefaultInstance(), null, interrupter);

      assertThat(result.getTermination().getReason())
          .isEqualTo(TerminationReasonProto.TERMINATION_REASON_NO_SOLUTION_FOUND);
      assertThat(result.getTermination().getLimit()).isEqualTo(LimitProto.LIMIT_INTERRUPTED);

    } finally {
      interrupter.delete();
    }
  }

  @Test
  public void cppSolve_withCallbackOnSolution_isInvoked() {
    List<ImmutableList<Double>> solutions = Collections.synchronizedList(new ArrayList<>());
    var solveCallback = new CppSolveCallback() {
      @Override
      public CallbackResultProto onEvent(CallbackDataProto cbData) {
        Preconditions.checkArgument(
            cbData.getEvent() == CallbackEventProto.CALLBACK_EVENT_MIP_SOLUTION);
        solutions.add(ImmutableList.copyOf(cbData.getPrimalSolutionVector().getValuesList()));
        return CallbackResultProto.getDefaultInstance();
      }
    };
    try {
      SolveResultProto result = JniSolver.cppSolve(simpleModel(),
          SolverTypeProto.SOLVER_TYPE_CP_SAT, SolveParametersProto.getDefaultInstance(),
          ModelSolveParametersProto.getDefaultInstance(), null,
          CallbackRegistrationProto.newBuilder()
              .addRequestRegistration(CallbackEventProto.CALLBACK_EVENT_MIP_SOLUTION)
              .build(),
          solveCallback, null);

      assertThat(result.getTermination().getReason())
          .isEqualTo(TerminationReasonProto.TERMINATION_REASON_OPTIMAL);
      // Note that CP-SAT will always return integer values for this problem.
      assertThat(solutions).contains(ImmutableList.of(1.0));
    } finally {
      solveCallback.delete();
    }
  }
}
