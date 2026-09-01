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

import com.google.common.base.Verify;
import com.google.ortools.mathopt.CallbackEventProto;
import java.util.EnumMap;
import java.util.Optional;

/** The supported events during {@link Solve#solve(Model, SolverType, Solve.Args)} for callbacks. */
public enum CallbackEvent {
  /**
   * The solver is currently running presolve.
   *
   * <p>This event is supported only for {@link SolverType#GUROBI}.
   */
  PRESOLVE(CallbackEventProto.CALLBACK_EVENT_PRESOLVE),

  /**
   * The solver is currently running the simplex method.
   *
   * <p>This event is supported only for {@link SolverType#GUROBI}.
   */
  SIMPLEX(CallbackEventProto.CALLBACK_EVENT_SIMPLEX),

  /**
   * The solver is in the MIP loop (called periodically before starting a new node).
   *
   * <p>Useful for early termination. Note that this event does not provide information on LP
   * relaxations nor about new incumbent solutions.
   *
   * <p>This event is fully supported for MIP models by {@link SolverType#GUROBI}. If used with
   * {@link SolverType#CP_SAT}, it is called when the dual bound is improved.
   */
  MIP(CallbackEventProto.CALLBACK_EVENT_MIP),

  /**
   * Called every time a new MIP incumbent is found.
   *
   * <p>This event is fully supported for MIP models by {@link SolverType#GUROBI}. SolverType.CP_SAT
   * has partial support: you can view the solutions and request termination, but you cannot add
   * lazy constraints. Other solvers don't support this event.
   */
  MIP_SOLUTION(CallbackEventProto.CALLBACK_EVENT_MIP_SOLUTION),

  /**
   * Called inside a MIP node.
   *
   * <p>Note that there is no guarantee that the callback function will be called on every node.
   * That behavior is solver-dependent.
   *
   * <p>Disabling cuts using {@link SolveParameters} may interfere with this event being called
   * and/or adding cuts at this event, the behavior is solver specific.
   *
   * <p>This event is supported for MIP models with {@link SolverType#GUROBI} only.
   */
  MIP_NODE(CallbackEventProto.CALLBACK_EVENT_MIP_NODE),

  /**
   * Called in each iterate of an interior point/barrier method.
   *
   * <p>This event is supported for {@link SolverType#GUROBI} only.
   */
  BARRIER(CallbackEventProto.CALLBACK_EVENT_BARRIER);

  private static class ProtoMap {
    private static final EnumMap<CallbackEventProto, CallbackEvent> map =
        new EnumMap<>(CallbackEventProto.class);
  }

  private final CallbackEventProto proto;

  CallbackEvent(CallbackEventProto proto) {
    ProtoMap.map.put(proto, this);
    this.proto = proto;
  }

  public CallbackEventProto toProto() {
    return proto;
  }

  /**
   * Returns a {@link CallbackEvent} from the given {@code proto}, or an empty optional if {@code
   * proto} is unspecified.
   *
   * @throws UnsupportedOperationException if {@code proto} is unrecognized.
   * @see #fromProto(CallbackEventProto)
   */
  public static Optional<CallbackEvent> optionalFromProto(CallbackEventProto proto) {
    switch (proto) {
      case CALLBACK_EVENT_UNSPECIFIED -> {
        return Optional.empty();
      }
      case UNRECOGNIZED ->
        throw new UnsupportedOperationException(
            "CallbackEventProto was UNRECOGNIZED, cannot convert to CallbackEvent. Typically"
            + " caused by proto data newer than the compiled proto file.");
      default -> {
        CallbackEvent event = ProtoMap.map.get(proto);
        // Unit tests ensure this is not reachable.
        Verify.verifyNotNull(
            event, "CallbackEventProto %s is not associated with any CallbackEvent", proto);
        return Optional.of(event);
      }
    }
  }

  /**
   * Returns a {@link CallbackEvent} from the given {@code proto}.
   *
   * @throws IllegalArgumentException if {@code proto} is unspecified.
   * @throws UnsupportedOperationException if {@code proto} is unrecognized.
   * @see #optionalFromProto(CallbackEventProto)
   */
  public static CallbackEvent fromProto(CallbackEventProto proto) {
    return optionalFromProto(proto).orElseThrow(
        ()
            -> new IllegalArgumentException(
                "CallbackEventProto was CALLBACK_EVENT_UNSPECIFIED, this value is not"
                + " convertible to CallbackEvent"));
  }
}
