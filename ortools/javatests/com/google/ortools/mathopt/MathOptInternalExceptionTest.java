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

import org.junit.jupiter.api.Test;

public class MathOptInternalExceptionTest {
  @Test
  public void new_messageOnly_hasMessage() {
    var error = new MathOptInternalException("error message");
    assertThat(error).hasMessageThat().isEqualTo("error message");
    assertThat(error).hasCauseThat().isNull();
  }

  @Test
  public void new_causeOnly_hasCause() {
    var cause = new IndexOutOfBoundsException();
    var error = new MathOptInternalException(cause);
    assertThat(error).hasMessageThat().contains("IndexOutOfBounds");
    assertThat(error).hasCauseThat().isSameInstanceAs(cause);
  }

  @Test
  public void new_messageAndCause_hasBoth() {
    var cause = new IndexOutOfBoundsException();
    var error = new MathOptInternalException("error message", cause);
    assertThat(error).hasMessageThat().isEqualTo("error message");
    assertThat(error).hasCauseThat().isSameInstanceAs(cause);
  }
}
