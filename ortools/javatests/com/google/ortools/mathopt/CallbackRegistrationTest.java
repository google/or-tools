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

import com.google.common.collect.ImmutableList;
import org.junit.jupiter.api.Test;

public final class CallbackRegistrationTest {
  @Test
  public void new_empty_isEmpty() {
    var registration = new CallbackRegistration();
    assertThat(registration.getEvents()).isEmpty();
    assertThat(registration.getAddCuts()).isFalse();
    assertThat(registration.getAddLazyConstraints()).isFalse();
  }

  @Test
  public void enableCuts_areEnabled() {
    var registration = new CallbackRegistration();
    assertThat(registration.enableCuts()).isSameInstanceAs(registration);
    assertThat(registration.getAddCuts()).isTrue();
  }

  @Test
  public void setAddCuts_falseToTrue_isTrue() {
    var registration = new CallbackRegistration();
    assertThat(registration.setAddCuts(true)).isSameInstanceAs(registration);
    assertThat(registration.getAddCuts()).isTrue();
  }

  @Test
  public void setAddCuts_trueToFalse_isFalse() {
    var registration = new CallbackRegistration();
    registration.enableCuts();
    registration.setAddCuts(false);
    assertThat(registration.getAddCuts()).isFalse();
  }

  @Test
  public void enableLazyConstraints_areEnabled() {
    var registration = new CallbackRegistration();
    assertThat(registration.enableLazyConstraints()).isSameInstanceAs(registration);
    assertThat(registration.getAddLazyConstraints()).isTrue();
  }

  @Test
  public void setAddLazyConstraints_falseToTrue_isTrue() {
    var registration = new CallbackRegistration();
    assertThat(registration.setAddLazyConstraints(true)).isSameInstanceAs(registration);
    assertThat(registration.getAddLazyConstraints()).isTrue();
  }

  @Test
  public void setAddLazyConstraints_trueToFalse_isFalse() {
    var registration = new CallbackRegistration();
    registration.enableLazyConstraints();
    registration.setAddLazyConstraints(false);
    assertThat(registration.getAddLazyConstraints()).isFalse();
  }

  @Test
  public void addEvent_wasEmpty_hasEvent() {
    var registration = new CallbackRegistration();
    assertThat(registration.addEvent(CallbackEvent.MIP_NODE)).isSameInstanceAs(registration);
    registration.addEvent(CallbackEvent.MIP_SOLUTION);
    assertThat(registration.getEvents())
        .containsExactly(CallbackEvent.MIP_NODE, CallbackEvent.MIP_SOLUTION);
  }

  @Test
  public void toProto_empty_isDefault() {
    assertThat(new CallbackRegistration().toProto()).isEqualToDefaultInstance();
  }

  @Test
  public void toProto_full_isEquivalent() {
    var expected = CallbackRegistrationProto.newBuilder()
                       .setAddCuts(true)
                       .setAddLazyConstraints(true)
                       .addAllRequestRegistration(ImmutableList.sortedCopyOf(
                           ImmutableList.of(CallbackEventProto.CALLBACK_EVENT_MIP_NODE,
                               CallbackEventProto.CALLBACK_EVENT_MIP_SOLUTION)))
                       .build();
    assertThat(new CallbackRegistration()
                   .enableCuts()
                   .enableLazyConstraints()
                   .addEvent(CallbackEvent.MIP_SOLUTION)
                   .addEvent(CallbackEvent.MIP_NODE)
                   .toProto())
        .isEqualTo(expected);
  }

  @Test
  public void new_fromEmptyProto_isEmpty() {
    var registration = new CallbackRegistration(CallbackRegistrationProto.getDefaultInstance());
    assertThat(registration.getAddCuts()).isFalse();
    assertThat(registration.getAddLazyConstraints()).isFalse();
    assertThat(registration.getEvents()).isEmpty();
  }

  @Test
  public void new_fromProto_isEquivalent() {
    var proto = CallbackRegistrationProto.newBuilder()
                    .setAddCuts(true)
                    .setAddLazyConstraints(true)
                    .addAllRequestRegistration(ImmutableList.sortedCopyOf(
                        ImmutableList.of(CallbackEventProto.CALLBACK_EVENT_MIP_NODE,
                            CallbackEventProto.CALLBACK_EVENT_MIP_SOLUTION)))
                    .build();
    var registration = new CallbackRegistration(proto);
    assertThat(registration.getAddCuts()).isTrue();
    assertThat(registration.getAddLazyConstraints()).isTrue();
    assertThat(registration.getEvents())
        .containsExactly(CallbackEvent.MIP_NODE, CallbackEvent.MIP_SOLUTION);
  }
}
