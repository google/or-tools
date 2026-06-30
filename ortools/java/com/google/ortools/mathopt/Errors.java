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

import com.google.ortools.util.StatusProto;
import java.util.HashMap;
import java.util.Optional;

/** Translate C++'s absl::Status errors to Java standard exceptions. */
public final class Errors {
  /** Returns the Java exception corresponding to the C++ Status. */
  public static Optional<RuntimeException> statusProtoToException(StatusProto statusProto) {
    Optional<StatusCode> code = StatusCode.fromInteger(statusProto.getCode());
    if (code.isEmpty()) {
      return Optional.of(new MathOptInternalException(
          "unknown C++ error (code = " + statusProto.getCode() + "): " + statusProto.getMessage()));
    }
    return code.get().exception(statusProto.getMessage());
  }

  /** The C++ absl::Status::code() values. */
  public enum StatusCode {
    OK(0),
    CANCELLED(1),
    UNKNOWN(2),
    INVALID_ARGUMENT(3),
    DEADLINE_EXCEEDED(4),
    NOT_FOUND(5),
    ALREADY_EXISTS(6),
    PERMISSION_DENIED(7),
    UNAUTHENTICATED(16),
    RESOURCE_EXHAUSTED(8),
    FAILED_PRECONDITION(9),
    ABORTED(10),
    OUT_OF_RANGE(11),
    UNIMPLEMENTED(12),
    INTERNAL(13),
    UNAVAILABLE(14),
    DATA_LOSS(15);

    /** Returns the StatusCode with the same integer code; empty if no enum matches this value. */
    public static Optional<StatusCode> fromInteger(int code) {
      return Optional.ofNullable(Map.value.get(code));
    }

    /** Returns the Java exception corresponding to the status code. */
    public Optional<RuntimeException> exception(String statusMessage) {
      return switch (this) {
        case OK -> Optional.empty();
        case INVALID_ARGUMENT ->
          Optional.of(new IllegalArgumentException(expectedExceptionMessage(statusMessage)));
        case FAILED_PRECONDITION ->
          Optional.of(new IllegalStateException(expectedExceptionMessage(statusMessage)));
        case UNIMPLEMENTED ->
          Optional.of(new UnsupportedOperationException(expectedExceptionMessage(statusMessage)));
        case INTERNAL ->
          Optional.of(new MathOptInternalException(expectedExceptionMessage(statusMessage)));
        default ->
          Optional.of(
              new MathOptInternalException("unexpected C++ error " + this + ": " + statusMessage));
      };
    }

    /**
     * Returns the message of Java exceptions for expected MathOpt StatusCode's.
     *
     * @see exception()
     */
    private String expectedExceptionMessage(String statusMessage) {
      return statusMessage + " (was C++ " + this + ")";
    }

    /** Returns the integer absl::StatusCode. */
    public int getCode() {
      return this.code;
    }

    // Note that we can't access a static member in an enum constructor; we thus use an intermediate
    // class here.
    private static class Map {
      static final HashMap<Integer, StatusCode> value = new HashMap<>();
    }

    private final int code;

    StatusCode(int code) {
      Map.value.put(code, this);
      this.code = code;
    }
  }

  private Errors() {}
}
