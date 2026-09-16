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

import com.google.ortools.mathopt.SolutionStatusProto;
import java.util.EnumMap;
import java.util.Optional;

/** Feasibility of a primal or dual solution as claimed by the solver. */
public enum SolutionStatus {
  /** Solver does not claim a feasibility status. */
  UNDETERMINED(SolutionStatusProto.SOLUTION_STATUS_UNDETERMINED),

  /** Solver claims the solution is feasible. */
  FEASIBLE(SolutionStatusProto.SOLUTION_STATUS_FEASIBLE),

  /** Solver claims the solution is infeasible. */
  INFEASIBLE(SolutionStatusProto.SOLUTION_STATUS_INFEASIBLE);

  private static class ProtoMap {
    private static final EnumMap<SolutionStatusProto, SolutionStatus> map =
        new EnumMap<>(SolutionStatusProto.class);
  }

  private final SolutionStatusProto proto;

  SolutionStatus(SolutionStatusProto proto) {
    ProtoMap.map.put(proto, this);
    this.proto = proto;
  }

  public SolutionStatusProto toProto() {
    return this.proto;
  }

  /**
   * Returns a {@link SolutionStatus} from the given {@code proto}.
   *
   * @throws IllegalArgumentException if {@code proto} is unspecified.
   */
  public static SolutionStatus fromProto(SolutionStatusProto proto) {
    SolutionStatus status = ProtoMap.map.get(proto);
    if (status == null) {
      throw new IllegalArgumentException("solution status must be set");
    }
    return status;
  }

  public static Optional<SolutionStatus> optionalFromProto(SolutionStatusProto proto) {
    return Optional.ofNullable(ProtoMap.map.get(proto));
  }

  public static SolutionStatusProto optionalToProto(Optional<SolutionStatus> status) {
    if (status.isPresent()) {
      return status.get().proto;
    }
    return SolutionStatusProto.SOLUTION_STATUS_UNSPECIFIED;
  }
}
