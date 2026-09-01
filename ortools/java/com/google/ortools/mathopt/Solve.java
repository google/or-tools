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

import com.google.common.collect.ImmutableList;
import com.google.ortools.mathopt.CallbackDataProto;
import com.google.ortools.mathopt.CallbackRegistrationProto;
import com.google.ortools.mathopt.CallbackResultProto;
import com.google.ortools.mathopt.ModelSolveParametersProto;
import com.google.ortools.mathopt.SolveParametersProto;
import com.google.ortools.mathopt.SolveResultProto;
import com.google.ortools.util.ScopedSwigInterrupter;
import com.google.ortools.util.SolveInterrupter;
import java.util.Optional;
import java.util.function.Consumer;
import java.util.function.Function;

/** Solves MathOpt optimization models, see {@link #solve(Model, SolverType)} for details. */
public final class Solve {
  /**
   * Solves the optimization locally calling into C++.
   *
   * <p>Check termination reason in the returned {@link SolveResult#getTermination()} before trying
   * to access the solution, as the solution may not be present.
   *
   * <p>This method is threadsafe to invoke concurrently on the same model (but some solvers e.g.
   * GLPK have additional restrictions).
   *
   * <p>Some solvers may invoke the message callback from {@code args} concurrently in multiple
   * threads, users should protect against this.
   *
   * @param model the optimization problem to solve, not modified by this function
   * @param solverType the solver to use
   * @param args configuration of the solver
   * @return the reason for termination and the solutions, rays, and basis if present
   * @throws IllegalArgumentException if the C++ solver returns a status error with code {@code
   *     InvalidArgument}. This is typically caused by bad user input (e.g. trying to solve a MIP
   *     with an LP solver like GLOP).
   * @throws MathOptInternalException if the C++ solver has any other {@code Status} error failure
   *     (most often with the code {@code Internal}). This is typically caused by an internal error
   *     in MathOpt or the underlying solver.
   */
  public static SolveResult solve(Model model, SolverType solverType, Args args) {
    try (var messageCallback = new ScopedCppMessageCallback(args.messageCallback);
        var callback =
            new ScopedCppSolveCallback(makeProtoSolveCallback(model, args.getCallback()));
        var swigInterrupter = new ScopedSwigInterrupter(args.interrupter.isPresent());
        var unused =
            args.interrupter.map(interrupter -> interrupter.chain(swigInterrupter)).orElse(null)) {
      // TODO(b/283248659): we are silently dropping exceptions thrown callbacks.
      SolveResultProto resultProto = JniSolver.cppSolve(model.toProto(), solverType.toProto(),
          args.solveParams, args.modelSolveParams, messageCallback.getCppMessageCallback(),
          args.getCallbackRegistration(), callback.getCppSolveCallback(),
          swigInterrupter.getSwigInterrupter());
      return new SolveResult(model, resultProto);
    } catch (RuntimeException exception) {
      if (exception.getMessage().startsWith("INVALID_ARGUMENT:")) {
        throw new IllegalArgumentException(exception);
      }
      // TODO(b/287299887): this is not properly tested.
      throw new MathOptInternalException(exception);
    }
  }

  /** Like {@link #solve(Model, SolverType, Args)} with default Args. */
  public static SolveResult solve(Model model, SolverType solverType) {
    return solve(model, solverType, new Args());
  }

  /** Configuration for a single call to {@link #solve(Model, SolverType, Args)}. */
  public static class Args {
    private SolveParametersProto solveParams = SolveParametersProto.getDefaultInstance();
    private ModelSolveParametersProto modelSolveParams =
        ModelSolveParametersProto.getDefaultInstance();
    private Optional<Consumer<ImmutableList<String>>> messageCallback = Optional.empty();
    private CallbackRegistrationProto callbackRegistration =
        CallbackRegistrationProto.getDefaultInstance();
    private Optional<Function<CallbackData, CallbackResult>> callback = Optional.empty();
    private Optional<SolveInterrupter> interrupter = Optional.empty();

    public Args() {}

    /**
     * Configuration of the underlying solver for this solve.
     *
     * <p>The returned type is NOT the (mutable) {@link SolveParameters} but instead its equivalent
     * (and immutable) {@link SolveParametersProto}. These types support bidirectional conversion.
     */
    public SolveParametersProto getSolveParams() {
      return solveParams;
    }

    /** See {@link #getSolveParams()} for details. */
    public Args setSolveParams(SolveParametersProto solveParams) {
      this.solveParams = solveParams;
      return this;
    }

    /**
     * Converts {@code solveParamsBuilder} to its proto representation and sets this value, see
     * {@link #getSolveParams()} for details.
     */
    public Args copySolveParamsFrom(SolveParameters solveParamsBuilder) {
      this.solveParams = solveParamsBuilder.toProto();
      return this;
    }

    /**
     * Configuration of the underlying solver for this solve that depends on the model (e.g. hint).
     *
     * <p>The returned type is NOT the (mutable) {@link ModelSolveParameters} but instead its
     * equivalent (and immutable) {@link ModelSolveParametersProto}. These types support
     * bidirectional conversion.
     */
    public ModelSolveParametersProto getModelSolveParams() {
      return modelSolveParams;
    }

    /** See {@link #getModelSolveParams()} for details. */
    public Args setModelSolveParams(ModelSolveParametersProto modelSolveParams) {
      this.modelSolveParams = modelSolveParams;
      return this;
    }

    /**
     * Converts {@code modelSolveParamsBuilder} to its proto representation and sets this value, see
     * {@link #getModelSolveParams()} for details.
     */
    public Args copyModelSolveParamsFrom(ModelSolveParameters modelSolveParamsBuilder) {
      this.modelSolveParams = modelSolveParamsBuilder.toProto();
      return this;
    }

    /** A callback that gives the underlying solver's logs line by line. */
    public Optional<Consumer<ImmutableList<String>>> getMessageCallback() {
      return messageCallback;
    }

    /** See {@link #getMessageCallback()} for details. */
    public Args setMessageCallback(Consumer<ImmutableList<String>> messageCallback) {
      this.messageCallback = Optional.of(messageCallback);
      return this;
    }

    /** See {@link #getMessageCallback()} for details. */
    public Args clearMessageCallback() {
      this.messageCallback = Optional.empty();
      return this;
    }

    /**
     * Determines on which {@link CallbackEvent} the solver will invoke {@link #getCallback()}, and
     * what actions the user is allowed to take in the callback.
     *
     * <p>If this class has any registered events and {@link #getCallback()} is empty, an exception
     * is thrown. TODO(b/283281886): this is crashing for some solvers, in particular, CP-SAT.
     *
     * <p>The returned type is NOT the (mutable) {@link CallbackRegistration} but instead its
     * equivalent (and immutable) {@link CallbackRegistrationProto}. These types support
     * bidirectional conversion.
     */
    public CallbackRegistrationProto getCallbackRegistration() {
      return callbackRegistration;
    }

    /** See {@link #getCallbackRegistration()} for details. */
    public Args setCallbackRegistration(CallbackRegistrationProto callbackRegistration) {
      this.callbackRegistration = callbackRegistration;
      return this;
    }

    /**
     * Converts {@code callbackRegistration} to its proto representation and sets this value, see
     * {@link #getCallbackRegistration()} for details.
     */
    public Args copyCallbackRegistrationFrom(CallbackRegistration callbackRegistration) {
      this.callbackRegistration = callbackRegistration.toProto();
      return this;
    }

    /**
     * A callback invoked periodically by the solver for observing progress and/or customizing the
     * solver's behavior.
     *
     * <p>The {@link CallbackEvent} from {@link CallbackData#getEvent()} gives the current state of
     * the solver and informs what actions the user is allowed to take. The callback will only be
     * invoked on events listed in {@link #getCallbackRegistration()}.
     *
     * <p>Some solvers may invoke the callback from multiple threads (Gurobi will not).
     */
    public Optional<Function<CallbackData, CallbackResult>> getCallback() {
      return callback;
    }

    /** See {@link #getCallback()} for details. */
    public Args setCallback(Function<CallbackData, CallbackResult> callback) {
      this.callback = Optional.of(callback);
      return this;
    }

    /** See {@link #getCallback()} for details. */
    public Args clearCallback() {
      this.callback = Optional.empty();
      return this;
    }

    /** When interrupted, requests that the underlying solver stops as soon as possible. */
    public Optional<SolveInterrupter> getInterrupter() {
      return interrupter;
    }

    /** See {@link #getInterrupter()} for details. */
    public Args setInterrupter(SolveInterrupter interrupter) {
      this.interrupter = Optional.of(interrupter);
      return this;
    }

    /** See {@link #getInterrupter()} for details. */
    public Args clearInterrupter() {
      this.interrupter = Optional.empty();
      return this;
    }
  }

  private static Optional<Function<CallbackDataProto, CallbackResultProto>> makeProtoSolveCallback(
      Model model, Optional<Function<CallbackData, CallbackResult>> userCallback) {
    if (userCallback.isEmpty()) {
      return Optional.empty();
    }
    Function<CallbackDataProto, CallbackResultProto> protoCb = (callbackDataProto)
        -> userCallback.get().apply(new CallbackData(model, callbackDataProto)).toProto();
    return Optional.of(protoCb);
  }

  private Solve() {}
}
