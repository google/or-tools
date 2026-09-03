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

import com.google.ortools.mathopt.SolutionStatusProto;
import java.util.Optional;
import org.junit.jupiter.api.Test;

public final class SolutionStatusTest {
  @Test
  public void solutionStatusToProto() {
    assertThat(SolutionStatus.UNDETERMINED.toProto())
        .isEqualTo(SolutionStatusProto.SOLUTION_STATUS_UNDETERMINED);
    assertThat(SolutionStatus.FEASIBLE.toProto())
        .isEqualTo(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE);
    assertThat(SolutionStatus.INFEASIBLE.toProto())
        .isEqualTo(SolutionStatusProto.SOLUTION_STATUS_INFEASIBLE);
  }

  @Test
  public void optionalSolutionStatusToProto() {
    assertThat(SolutionStatus.optionalToProto(Optional.of(SolutionStatus.UNDETERMINED)))
        .isEqualTo(SolutionStatusProto.SOLUTION_STATUS_UNDETERMINED);
    assertThat(SolutionStatus.optionalToProto(Optional.of(SolutionStatus.FEASIBLE)))
        .isEqualTo(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE);
    assertThat(SolutionStatus.optionalToProto(Optional.of(SolutionStatus.INFEASIBLE)))
        .isEqualTo(SolutionStatusProto.SOLUTION_STATUS_INFEASIBLE);
    assertThat(SolutionStatus.optionalToProto(Optional.empty()))
        .isEqualTo(SolutionStatusProto.SOLUTION_STATUS_UNSPECIFIED);
  }

  @Test
  public void solutionStatusFromProto() {
    assertThat(SolutionStatus.fromProto(SolutionStatusProto.SOLUTION_STATUS_UNDETERMINED))
        .isEqualTo(SolutionStatus.UNDETERMINED);
    assertThat(SolutionStatus.fromProto(SolutionStatusProto.SOLUTION_STATUS_INFEASIBLE))
        .isEqualTo(SolutionStatus.INFEASIBLE);
    assertThat(SolutionStatus.fromProto(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE))
        .isEqualTo(SolutionStatus.FEASIBLE);

    var error = assertThrows(IllegalArgumentException.class,
        () -> SolutionStatus.fromProto(SolutionStatusProto.SOLUTION_STATUS_UNSPECIFIED));
    assertThat(error).hasMessageThat().contains("solution status");
  }

  @Test
  public void solutionStatusOptionalFromProto() {
    assertThat(SolutionStatus.optionalFromProto(SolutionStatusProto.SOLUTION_STATUS_UNDETERMINED))
        .hasValue(SolutionStatus.UNDETERMINED);
    assertThat(SolutionStatus.optionalFromProto(SolutionStatusProto.SOLUTION_STATUS_INFEASIBLE))
        .hasValue(SolutionStatus.INFEASIBLE);
    assertThat(SolutionStatus.optionalFromProto(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE))
        .hasValue(SolutionStatus.FEASIBLE);
    assertThat(SolutionStatus.optionalFromProto(SolutionStatusProto.SOLUTION_STATUS_UNSPECIFIED))
        .isEmpty();
  }
}
