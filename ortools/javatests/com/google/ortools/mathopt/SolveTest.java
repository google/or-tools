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

import static com.google.common.truth.Correspondence.tolerance;
import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.extensions.proto.ProtoTruth.assertThat;
import static com.google.ortools.mathopt.Expressions.linExpr;
import static com.google.ortools.mathopt.testing.StringCorrespondences.hasSubstring;
import static org.junit.jupiter.api.Assertions.assertThrows;

import com.google.common.base.Preconditions;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.ortools.Loader;
import com.google.ortools.mathopt.Basis.BasisStatus;
import com.google.ortools.mathopt.CallbackRegistrationProto;
import com.google.ortools.mathopt.ModelSolveParameters.SolutionHint;
import com.google.ortools.mathopt.ModelSolveParametersProto;
import com.google.ortools.mathopt.SolveParameters.Emphasis;
import com.google.ortools.mathopt.SolveParametersProto;
import com.google.ortools.mathopt.SolveResult.Limit;
import com.google.ortools.mathopt.SolveResult.TerminationReason;
import com.google.ortools.mathopt.testing.MathOptProtoSubject;
import com.google.ortools.util.SolveInterrupter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;
import org.junit.Ignore;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class SolveTest {
  @BeforeEach
  public void loadJni() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void testSolve_modelIsInvalidForSolver_throwsIllegalArgument() {
    var model = new Model();
    var unused = model.addBinaryVariable();

    var error =
        assertThrows(IllegalArgumentException.class, () -> Solve.solve(model, SolverType.GLOP));
    assertThat(error).hasMessageThat().contains("does not support integer variables");
  }

  @Test
  public void testSimpleLpGlop() {
    var model = new Model();
    Variable x = model.addVariable(0.0, 1.0);
    Variable y = model.addVariable(0.0, 2.0);
    model.maximize(linExpr().add(3.0, x).add(1.0, y));
    model.addLe(linExpr(x).add(y), 2.0, "");

    SolveResult result = Solve.solve(model, SolverType.GLOP);

    assertThat(result.getTermination().getReason()).isEqualTo(TerminationReason.OPTIMAL);
    assertThat(result.getBestObjectiveBound()).isWithin(1.0e-5).of(4.0);
    assertThat(result.getVariableValues())
        .comparingValuesUsing(tolerance(1.0e-5))
        .containsExactly(x, 1.0, y, 1.0);
  }

  @Test
  public void testSimpleIpCpSat() {
    var model = new Model();
    Variable x = model.addIntegerVariable(0.0, 1.0);
    Variable y = model.addIntegerVariable(0.0, 2.0);
    model.maximize(linExpr().add(3.0, x).add(1.0, y));
    model.addLe(linExpr(x).add(y), 2.0, "");

    SolveResult result = Solve.solve(model, SolverType.CP_SAT);

    assertThat(result.getTermination().getReason()).isEqualTo(TerminationReason.OPTIMAL);
    assertThat(result.getBestObjectiveBound()).isWithin(1e-5).of(4.0);
    assertThat(result.getVariableValues())
        .comparingValuesUsing(tolerance(1e-5))
        .containsExactly(x, 1.0, y, 1.0);
  }

  @Test
  public void testSolveParametersWithCutoff() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    model.minimize(x);
    var args = new Solve.Args();
    args.copySolveParamsFrom(new SolveParameters().setCutoffLimit(-1.0));

    SolveResult result = Solve.solve(model, SolverType.CP_SAT, args);

    assertThat(result.getTermination().getReason()).isEqualTo(TerminationReason.NO_SOLUTION_FOUND);
    assertThat(result.getTermination().getLimit()).hasValue(Limit.CUTOFF);
  }

  @Test
  public void testSolve_withModelSolveParameters_startingBasisEffect() {
    var model = new Model();
    Variable x = model.addVariable(0.0, 1.0);
    Variable y = model.addVariable(0.0, 1.0);
    model.maximize(Expressions.linExpr().add(x).add(y));
    var args = new Solve.Args();
    args.copySolveParamsFrom(new SolveParameters().setPresolve(Emphasis.OFF).setIterationLimit(1L));

    args.copyModelSolveParamsFrom(new ModelSolveParameters().setBasis(new Basis(ImmutableMap.of(),
        ImmutableMap.of(x, BasisStatus.AT_LOWER_BOUND, y, BasisStatus.AT_LOWER_BOUND))));

    // Solve starting from (0, 0) with iteration limit 1, we get feasible with objective value 1.
    SolveResult fromZeroResult = Solve.solve(model, SolverType.GLOP, args);
    assertThat(fromZeroResult.getTermination().getReason()).isEqualTo(TerminationReason.FEASIBLE);
    assertThat(fromZeroResult.getObjectiveValue()).isWithin(1.0e-5).of(1.0);

    // Solve starting from (1, 1) with iteration limit 1, we get optimal with objective value 2.
    args.copyModelSolveParamsFrom(new ModelSolveParameters().setBasis(new Basis(ImmutableMap.of(),
        ImmutableMap.of(x, BasisStatus.AT_UPPER_BOUND, y, BasisStatus.AT_UPPER_BOUND))));
    SolveResult hintResult = Solve.solve(model, SolverType.GLOP, args);
    assertThat(hintResult.getTermination().getReason()).isEqualTo(TerminationReason.OPTIMAL);
    assertThat(hintResult.getObjectiveValue()).isWithin(1.0e-4).of(2.0);
  }

  @Test
  public void testMessageCallback() {
    var model = new Model();
    Variable x = model.addVariable().setLowerBound(0.0).setUpperBound(1.0);
    model.maximize(x);
    List<String> messages = Collections.synchronizedList(new ArrayList<>());

    SolveResult result =
        Solve.solve(model, SolverType.GLOP, new Solve.Args().setMessageCallback(messages::addAll));

    assertThat(result.getTermination().getReason()).isEqualTo(TerminationReason.OPTIMAL);
    assertThat(messages).comparingElementsUsing(hasSubstring("status: OPTIMAL")).contains(true);
  }

  @Test
  public void testSolve_withSolutionCallback_invokedOnOptimalSolution() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    model.maximize(x);

    List<ImmutableMap<Variable, Double>> callbackSolutions =
        Collections.synchronizedList(new ArrayList<>());
    var args = new Solve.Args();
    args.copyCallbackRegistrationFrom(
        new CallbackRegistration().addEvent(CallbackEvent.MIP_SOLUTION));
    args.setCallback((CallbackData callbackData) -> {
      Preconditions.checkArgument(callbackData.getEvent() == CallbackEvent.MIP_SOLUTION);
      Preconditions.checkArgument(callbackData.getVariableValues().isPresent());
      callbackSolutions.add(callbackData.getVariableValues().get());
      return new CallbackResult();
    });
    SolveResult result = Solve.solve(model, SolverType.CP_SAT, args);

    assertThat(result.getTermination().getReason()).isEqualTo(TerminationReason.OPTIMAL);
    // CP-SAT returns integer variable values for binary variables
    assertThat(callbackSolutions).contains(ImmutableMap.of(x, 1.0));
  }

  @Test
  @Ignore("TODO(b/283281886): this currently is a C++ crash that should be a status error.")
  public void testSolve_noCallbackButHasCbRegistration_throws() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    model.maximize(x);
    var args = new Solve.Args();
    args.copyCallbackRegistrationFrom(
        new CallbackRegistration().addEvent(CallbackEvent.MIP_SOLUTION));
    assertThrows(RuntimeException.class, () -> Solve.solve(model, SolverType.CP_SAT, args));
  }

  @Test
  public void args_setCallback_hasCallback() {
    var args = new Solve.Args();
    assertThat(args.getMessageCallback()).isEmpty();
    Consumer<ImmutableList<String>> cb = (messages) -> {};
    assertThat(args.setMessageCallback(cb)).isSameInstanceAs(args);
    assertThat(args.getMessageCallback()).hasValue(cb);
  }

  @Test
  public void args_clearCallback_noLongerHasCallback() {
    var args = new Solve.Args();
    Consumer<ImmutableList<String>> cb = (messages) -> {};
    args.setMessageCallback(cb);

    assertThat(args.clearMessageCallback()).isSameInstanceAs(args);

    assertThat(args.getMessageCallback()).isEmpty();
  }

  @Test
  public void args_clearCallbackWhenEmpty_hasNoEffect() {
    var args = new Solve.Args();

    assertThat(args.clearMessageCallback()).isSameInstanceAs(args);

    assertThat(args.getMessageCallback()).isEmpty();
  }

  @Test
  public void args_setSolveParametersByProto_hasProto() {
    var args = new Solve.Args();
    var params = SolveParametersProto.newBuilder().setCutoffLimit(1.0).build();

    args.setSolveParams(params);

    assertThat(args.getSolveParams()).isEqualTo(params);
  }

  @Test
  public void args_setSolveParametersByObject_hasProto() {
    var args = new Solve.Args();

    args.copySolveParamsFrom(new SolveParameters().setCutoffLimit(1.0));

    SolveParametersProto expected = SolveParametersProto.newBuilder().setCutoffLimit(1.0).build();
    MathOptProtoSubject.assertThat(args.getSolveParams()).isEquivalentTo(expected);
  }

  @Test
  public void args_setModelSolveParametersByProto_hasProto() {
    var args = new Solve.Args();
    var modelParams = ModelSolveParametersProto.newBuilder();
    modelParams.addSolutionHintsBuilder().getVariableValuesBuilder().addIds(0).addValues(1.0);

    args.setModelSolveParams(modelParams.build());

    assertThat(args.getModelSolveParams()).isEqualTo(modelParams.build());
  }

  @Test
  public void args_setModelSolveParametersByObject_hasProto() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    var args = new Solve.Args();
    args.copyModelSolveParamsFrom(new ModelSolveParameters().addSolutionHint(
        new SolutionHint(ImmutableMap.of(x, 1.0), ImmutableMap.of())));

    var expected = ModelSolveParametersProto.newBuilder();
    expected.addSolutionHintsBuilder().getVariableValuesBuilder().addIds(0).addValues(1.0);
    MathOptProtoSubject.assertThat(args.getModelSolveParams()).isEquivalentTo(expected.build());
  }

  @Test
  public void args_setCallbackRegistrationByObject_hasProto() {
    var args = new Solve.Args();
    args.copyCallbackRegistrationFrom(new CallbackRegistration().setAddLazyConstraints(true));

    var expected = CallbackRegistrationProto.newBuilder().setAddLazyConstraints(true).build();
    MathOptProtoSubject.assertThat(args.getCallbackRegistration()).isEquivalentTo(expected);
  }

  @Test
  public void args_setCallbackRegistrationByProto_hasProto() {
    var args = new Solve.Args();
    args.setCallbackRegistration(
        CallbackRegistrationProto.newBuilder().setAddLazyConstraints(true).build());

    var expected = CallbackRegistrationProto.newBuilder().setAddLazyConstraints(true).build();
    MathOptProtoSubject.assertThat(args.getCallbackRegistration()).isEquivalentTo(expected);
  }

  @Test
  public void testSolveCallbackInterrupted() {
    var model = new Model();
    Variable x = model.addVariable().setLowerBound(0.0).setUpperBound(0.0);
    model.maximize(x);
    var interrupter = new SolveInterrupter();
    interrupter.interrupt();
    SolveResult result =
        Solve.solve(model, SolverType.GLOP, new Solve.Args().setInterrupter(interrupter));
    assertThat(result.getTermination().getReason()).isEqualTo(TerminationReason.NO_SOLUTION_FOUND);
    assertThat(result.getTermination().getLimit()).hasValue(Limit.INTERRUPTED);
  }

  @Test
  public void testSolveWithIndicatorConstraint() {
    var model = new Model();
    Variable x = model.addIntegerVariable(0.0, 10.0);
    Variable z = model.addBinaryVariable();
    model.maximize(Expressions.linExpr(x).add(100.0, z));

    // First solve without indicator constraint.
    // The solver should pick z=1 (reward 100) and x=10 (reward 10). Total = 110.
    SolveResult result1 = Solve.solve(model, SolverType.GSCIP);
    assertThat(result1.getTermination().getReason()).isEqualTo(TerminationReason.OPTIMAL);
    assertThat(result1.getVariableValues()).containsEntry(x, 10.0);
    assertThat(result1.getVariableValues()).containsEntry(z, 1.0);

    // Add indicator constraint: z=1 => x <= 5
    // Now if z=1, x must be <= 5. Best x=5. Total = 105.
    // If z=0, x can be 10. Total = 10.
    // So solver should still pick z=1, but now x=5.
    model.addLeIndicatorConstraint(z, false, linExpr(x), 5.0, "");

    // Solve again
    SolveResult result2 = Solve.solve(model, SolverType.GSCIP);
    assertThat(result2.getTermination().getReason()).isEqualTo(TerminationReason.OPTIMAL);
    assertThat(result2.getVariableValues())
        .comparingValuesUsing(tolerance(1.0e-5))
        .containsEntry(x, 5.0);
    assertThat(result2.getVariableValues())
        .comparingValuesUsing(tolerance(1.0e-5))
        .containsEntry(z, 1.0);
  }

  @Test
  public void indicatorConstraintWithDeletedIndicator_isOptimal() {
    var model = new Model();
    Variable x = model.addVariable(0.0, 10.0, "x");
    Variable z = model.addBinaryVariable("z");
    model.maximize(Expressions.linExpr(x));
    model.addLeIndicatorConstraint(z, false, Expressions.linExpr(x), 5.0, "c");
    model.deleteVariable(z);

    SolveResult result = Solve.solve(model, SolverType.GSCIP);

    assertThat(result.getTermination().getReason())
        .isEqualTo(SolveResult.TerminationReason.OPTIMAL);
    assertThat(result.getObjectiveValue()).isWithin(1.0e-4).of(10.0);
  }

  @Test
  public void indicatorConstraintWithDeletedIndicatorActivateOnZero_isOptimal() {
    var model = new Model();
    Variable x = model.addVariable(0.0, 10.0, "x");
    Variable z = model.addBinaryVariable("z");
    model.maximize(Expressions.linExpr(x));
    model.addLeIndicatorConstraint(z, true, Expressions.linExpr(x), 5.0, "c");
    model.deleteVariable(z);

    SolveResult result = Solve.solve(model, SolverType.GSCIP);

    assertThat(result.getTermination().getReason())
        .isEqualTo(SolveResult.TerminationReason.OPTIMAL);
    // When the indicator variable is deleted, the constraint becomes vacuous (ignored)
    // even if activateOnZero is true, because the missing indicator ID in the proto
    // is interpreted as "no indicator" -> "vacuous" by the solver/backend.
    assertThat(result.getObjectiveValue()).isWithin(1.0e-4).of(10.0);
  }

  @Test
  public void indicatorConstraintWithNonIntegerIndicator_throws() {
    var model = new Model();
    Variable x = model.addVariable(0.0, 10.0, "x");
    Variable z = model.addVariable(0.0, 1.0, "z"); // Continuous
    model.addLeIndicatorConstraint(z, false, Expressions.linExpr(x), 5.0, "c");

    // Expect error during solve.
    assertThrows(IllegalArgumentException.class, () -> Solve.solve(model, SolverType.GSCIP));
  }
}
