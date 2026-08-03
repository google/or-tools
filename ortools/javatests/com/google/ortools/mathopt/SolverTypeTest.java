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

public final class SolverTypeTest {
  @Test
  public void fromProtoToProto_validProtos_roundTrip() {
    for (SolverTypeProto proto : SolverTypeProto.values()) {
      if (proto.equals(SolverTypeProto.SOLVER_TYPE_UNSPECIFIED)
          || proto.equals(SolverTypeProto.UNRECOGNIZED)) {
        continue;
      }
      assertThat(SolverType.fromProto(proto).toProto()).isEqualTo(proto);
    }
  }

  @Test
  public void optionalFromProto_validProtos_matchesFromProto() {
    for (SolverTypeProto proto : SolverTypeProto.values()) {
      if (proto.equals(SolverTypeProto.SOLVER_TYPE_UNSPECIFIED)
          || proto.equals(SolverTypeProto.UNRECOGNIZED)) {
        continue;
      }
      assertThat(SolverType.optionalFromProto(proto)).hasValue(SolverType.fromProto(proto));
    }
  }

  @Test
  public void fromProto_unspecified_throws() {
    var error = assertThrows(IllegalArgumentException.class,
        () -> SolverType.fromProto(SolverTypeProto.SOLVER_TYPE_UNSPECIFIED));
    assertThat(error).hasMessageThat().ignoringCase().contains("unspecified");
  }

  @Test
  public void optionalFromProto_unspecified_isEmpty() {
    assertThat(SolverType.optionalFromProto(SolverTypeProto.SOLVER_TYPE_UNSPECIFIED)).isEmpty();
  }

  @Test
  public void fromProto_unrecognized_throws() {
    var error = assertThrows(UnsupportedOperationException.class,
        () -> SolverType.fromProto(SolverTypeProto.UNRECOGNIZED));
    assertThat(error).hasMessageThat().ignoringCase().contains("unrecognized");
  }

  @Test
  public void optionalFromProto_unrecognized_throws() {
    var error = assertThrows(UnsupportedOperationException.class,
        () -> SolverType.optionalFromProto(SolverTypeProto.UNRECOGNIZED));
    assertThat(error).hasMessageThat().ignoringCase().contains("unrecognized");
  }
}
