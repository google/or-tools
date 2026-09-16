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

import static com.google.ortools.util.ProtoDurationConversion.toJavaDuration;

import com.google.common.collect.ImmutableMap;
import com.google.ortools.mathopt.CallbackDataProto;
import java.time.Duration;
import java.util.Optional;

/** The input for the callback on {@link Solve#solve(Model, SolverType, Solve.Args)}. */
public final class CallbackData {
  private final CallbackEvent event;
  private final Optional<ImmutableMap<Variable, Double>> variableValues;
  private final Duration runtime;

  private final CallbackDataProto.PresolveStats presolveStats;
  private final CallbackDataProto.SimplexStats simplexStats;
  private final CallbackDataProto.BarrierStats barrierStats;
  private final CallbackDataProto.MipStats mipStats;

  /**
   * An equivalent {@link CallbackData} to the protocol buffer.
   *
   * @throws IllegalArgumentException if {@code proto} is invalid (e.g. has a bad runtime) or if
   *     {@code model} is not consistent with {@code proto}, (e.g. it is missing variables
   *     referenced by the primal solution).
   * @throws UnsupportedOperationException if {@code proto} contains an unknown value (e.g. the
   *     callback event is unrecognized).
   */
  public CallbackData(Model model, CallbackDataProto proto) {
    event = CallbackEvent.fromProto(proto.getEvent());
    variableValues = proto.hasPrimalSolutionVector()
        ? Optional.of(UtilFromProto.variableValuesFromProto(model, proto.getPrimalSolutionVector()))
        : Optional.empty();
    runtime = toJavaDuration(proto.getRuntime());
    presolveStats = proto.getPresolveStats();
    simplexStats = proto.getSimplexStats();
    barrierStats = proto.getBarrierStats();
    mipStats = proto.getMipStats();
  }

  /** The current state of the underlying solver. */
  public CallbackEvent getEvent() {
    return event;
  }

  /**
   * The current primal solution, if a primal solution is available (otherwise is empty).
   *
   * <p>If {@link #getEvent()} is {@link CallbackEvent#MIP_NODE}, the returned value will be the
   * solution of the LP relaxation for this node. Further, it may be missing (typically when the LP
   * was imprecise).
   *
   * <p>If {@link #getEvent()} is {@link CallbackEvent#MIP_SOLUTION}, the returned value never be
   * empty. Further, it must be feasible (unless you are adding lazy constraints).
   */
  public Optional<ImmutableMap<Variable, Double>> getVariableValues() {
    return variableValues;
  }

  /**
   * Returns the elapsed time since {@link Solve#solve(Model, SolverType, Solve.Args)} was called.
   *
   * <p>Available for all events.
   */
  public Duration getRuntime() {
    return runtime;
  }

  /** Solver statistics available when {@link #getEvent()} is {@link CallbackEvent#PRESOLVE}. */
  public CallbackDataProto.PresolveStats getPresolveStats() {
    return presolveStats;
  }

  /** Solver statistics available when {@link #getEvent()} is {@link CallbackEvent#SIMPLEX}. */
  public CallbackDataProto.SimplexStats getSimplexStats() {
    return simplexStats;
  }

  /** Solver statistics available when {@link #getEvent()} is {@link CallbackEvent#BARRIER}. */
  public CallbackDataProto.BarrierStats getBarrierStats() {
    return barrierStats;
  }

  /**
   * Solver statistics available when {@link #getEvent()} is any of {@link CallbackEvent#MIP},
   * {@link CallbackEvent#MIP_NODE}, or {@link CallbackEvent#MIP_SOLUTION}.
   */
  public CallbackDataProto.MipStats getMipStats() {
    return mipStats;
  }
}
