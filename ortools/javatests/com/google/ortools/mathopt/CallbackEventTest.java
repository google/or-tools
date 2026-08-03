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
import static org.junit.jupiter.api.Assertions.assertThrows;

import org.junit.jupiter.api.Test;

public final class CallbackEventTest {
  @Test
  public void fromProtoToProto_validProtos_roundTrip() {
    for (CallbackEventProto proto : CallbackEventProto.values()) {
      if (proto.equals(CallbackEventProto.CALLBACK_EVENT_UNSPECIFIED)
          || proto.equals(CallbackEventProto.UNRECOGNIZED)) {
        continue;
      }
      assertThat(CallbackEvent.fromProto(proto).toProto()).isEqualTo(proto);
    }
  }

  @Test
  public void optionalFromProto_validProtos_matchesFromProto() {
    for (CallbackEventProto proto : CallbackEventProto.values()) {
      if (proto.equals(CallbackEventProto.CALLBACK_EVENT_UNSPECIFIED)
          || proto.equals(CallbackEventProto.UNRECOGNIZED)) {
        continue;
      }
      assertThat(CallbackEvent.optionalFromProto(proto)).hasValue(CallbackEvent.fromProto(proto));
    }
  }

  @Test
  public void fromProto_unspecified_throws() {
    var error = assertThrows(IllegalArgumentException.class,
        () -> CallbackEvent.fromProto(CallbackEventProto.CALLBACK_EVENT_UNSPECIFIED));
    assertThat(error).hasMessageThat().ignoringCase().contains("unspecified");
  }

  @Test
  public void optionalFromProto_unspecified_isEmpty() {
    assertThat(CallbackEvent.optionalFromProto(CallbackEventProto.CALLBACK_EVENT_UNSPECIFIED))
        .isEmpty();
  }

  @Test
  public void fromProto_unrecognized_throws() {
    var error = assertThrows(UnsupportedOperationException.class,
        () -> CallbackEvent.fromProto(CallbackEventProto.UNRECOGNIZED));
    assertThat(error).hasMessageThat().ignoringCase().contains("unrecognized");
  }

  @Test
  public void optionalFromProto_unrecognized_throws() {
    var error = assertThrows(UnsupportedOperationException.class,
        () -> CallbackEvent.optionalFromProto(CallbackEventProto.UNRECOGNIZED));
    assertThat(error).hasMessageThat().ignoringCase().contains("unrecognized");
  }
}
