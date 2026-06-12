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

import com.google.ortools.util.StatusProto;
import java.util.Optional;
import org.junit.jupiter.api.Test;

public final class ErrorsTest {
  @Test
  public void statusProtoToException_ok_empty() {
    assertThat(Errors.statusProtoToException(StatusProto.newBuilder().setCode(0).build()))
        .isEmpty();
  }

  @Test
  public void statusProtoToException_unexpected_error() {
    Optional<RuntimeException> exception = Errors.statusProtoToException(StatusProto.newBuilder()
            .setCode(Errors.StatusCode.UNAVAILABLE.getCode())
            .setMessage("something")
            .build());
    assertThat(exception).isPresent();
    assertThat(exception.get()).isInstanceOf(MathOptInternalException.class);
    assertThat(exception.get())
        .hasMessageThat()
        .isEqualTo("unexpected C++ error UNAVAILABLE: something");
  }

  @Test
  public void statusProtoToException_unknown_error() {
    Optional<RuntimeException> exception = Errors.statusProtoToException(
        StatusProto.newBuilder().setCode(-4).setMessage("something").build());
    assertThat(exception).isPresent();
    assertThat(exception.get()).isInstanceOf(MathOptInternalException.class);
    assertThat(exception.get())
        .hasMessageThat()
        .isEqualTo("unknown C++ error (code = -4): something");
  }

  @Test
  public void statusProtoToException_invalid_argument_error() {
    Optional<RuntimeException> exception = Errors.statusProtoToException(StatusProto.newBuilder()
            .setCode(Errors.StatusCode.INVALID_ARGUMENT.getCode())
            .setMessage("something")
            .build());
    assertThat(exception).isPresent();
    assertThat(exception.get()).isInstanceOf(IllegalArgumentException.class);
    assertThat(exception.get()).hasMessageThat().isEqualTo("something (was C++ INVALID_ARGUMENT)");
  }

  @Test
  public void statusProtoToException_failed_precondition_error() {
    Optional<RuntimeException> exception = Errors.statusProtoToException(StatusProto.newBuilder()
            .setCode(Errors.StatusCode.FAILED_PRECONDITION.getCode())
            .setMessage("something")
            .build());
    assertThat(exception).isPresent();
    assertThat(exception.get()).isInstanceOf(IllegalStateException.class);
    assertThat(exception.get())
        .hasMessageThat()
        .isEqualTo("something (was C++ FAILED_PRECONDITION)");
  }

  @Test
  public void statusProtoToException_unimplemented_error() {
    Optional<RuntimeException> exception = Errors.statusProtoToException(StatusProto.newBuilder()
            .setCode(Errors.StatusCode.UNIMPLEMENTED.getCode())
            .setMessage("something")
            .build());
    assertThat(exception).isPresent();
    assertThat(exception.get()).isInstanceOf(UnsupportedOperationException.class);
    assertThat(exception.get()).hasMessageThat().isEqualTo("something (was C++ UNIMPLEMENTED)");
  }

  @Test
  public void statusProtoToException_internal_error() {
    Optional<RuntimeException> exception = Errors.statusProtoToException(StatusProto.newBuilder()
            .setCode(Errors.StatusCode.INTERNAL.getCode())
            .setMessage("something")
            .build());
    assertThat(exception).isPresent();
    assertThat(exception.get()).isInstanceOf(MathOptInternalException.class);
    assertThat(exception.get()).hasMessageThat().isEqualTo("something (was C++ INTERNAL)");
  }
}
