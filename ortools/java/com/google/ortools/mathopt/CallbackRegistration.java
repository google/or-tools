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
import java.util.EnumSet;

/**
 * Provided with a callback to {@link Solve#solve(Model, SolverType, Solve.Args)} to inform the
 * solver of (1) what information the callback needs and (2) how the callback might alter the solve
 * process.
 */
public final class CallbackRegistration {
  private final EnumSet<CallbackEvent> events = EnumSet.noneOf(CallbackEvent.class);
  boolean addCuts = false;
  boolean addLazyConstraints = false;

  /** An empty registration on no events that does not add user cuts or lazy constraints. */
  public CallbackRegistration() {}

  /**
   * An equivalent {@link CallbackRegistration} to the proto representation.
   *
   * @see #toProto()
   */
  public CallbackRegistration(CallbackRegistrationProto proto) {
    for (CallbackEventProto event : proto.getRequestRegistrationList()) {
      addEvent(CallbackEvent.fromProto(event));
    }
    setAddCuts(proto.getAddCuts());
    setAddLazyConstraints(proto.getAddLazyConstraints());
  }

  /**
   * The events the solver should invoke the callback at.
   *
   * <p>When a solver is called with registered events that are not supported, an exception is
   * thrown. The supported events may depend on the model. For example registering for {@link
   * CallbackEvent#MIP} with a model that only contains continuous variables will fail for most
   * solvers. See the documentation of each event to see their supported solvers/model types.
   */
  public EnumSet<CallbackEvent> getEvents() {
    return events;
  }

  /** See {@link #getEvents()}. */
  public CallbackRegistration addEvent(CallbackEvent event) {
    events.add(event);
    return this;
  }

  /**
   * Must be true to add "user cuts" within a callback.
   *
   * <p>See {@link CallbackResult#addUserCut(double, com.google.common.collect.ImmutableMap,
   * double)} for the definition of "user cuts" and for how to use them.
   *
   * @see #setAddCuts(boolean)
   * @see #enableCuts()
   */
  public boolean getAddCuts() {
    return addCuts;
  }

  /**
   * Must be true to add "lazy constraints" within a callback.
   *
   * <p>See CallbackResult#addLazyConstraint(double, com.google.common.collect.ImmutableMap, double)
   * for the definition of "lazy constraints" and for how to use them.
   *
   * @see #setAddLazyConstraints(boolean)
   * @see #enableLazyConstraints()
   */
  public boolean getAddLazyConstraints() {
    return addLazyConstraints;
  }

  /** See {@link #getAddCuts()}. */
  public CallbackRegistration setAddCuts(boolean addsCuts) {
    this.addCuts = addsCuts;
    return this;
  }

  /** Equivalent to {@code setAddCuts(true)}, see {@link #getAddCuts()}. */
  public CallbackRegistration enableCuts() {
    return setAddCuts(true);
  }

  /** See {@link #getAddLazyConstraints()}. */
  public CallbackRegistration setAddLazyConstraints(boolean addsLazyConstraints) {
    this.addLazyConstraints = addsLazyConstraints;
    return this;
  }

  /** Equivalent to {@code setAddLazyConstraints(true)}, see {@link #getAddLazyConstraints()}. */
  public CallbackRegistration enableLazyConstraints() {
    return setAddLazyConstraints(true);
  }

  /**
   * Returns a protocol buffer equivalent to {@code this}.
   *
   * @see #CallbackRegistration(CallbackRegistrationProto)
   */
  public CallbackRegistrationProto toProto() {
    var registrationProto = CallbackRegistrationProto.newBuilder();
    for (CallbackEvent event : ImmutableList.sortedCopyOf(events)) {
      registrationProto.addRequestRegistration(event.toProto());
    }
    registrationProto.setAddCuts(addCuts);
    registrationProto.setAddLazyConstraints(addLazyConstraints);
    return registrationProto.build();
  }
}
