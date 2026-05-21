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

package com.google.ortools.util;

import static com.google.common.truth.Truth.assertThat;

import org.junit.jupiter.api.Test;

public final class ProtoDurationConversionTest {
  @Test
  public void zeroDuration_roundtrip() {
    com.google.protobuf.Duration protoDuration =
        ProtoDurationConversion.toProtoDuration(java.time.Duration.ZERO);
    assertThat(ProtoDurationConversion.toJavaDuration(protoDuration))
        .isEqualTo(java.time.Duration.ZERO);
  }

  @Test
  public void closeToZeroPositiveDuration_roundtrip() {
    java.time.Duration duration = java.time.Duration.ofNanos(454_785_843);
    com.google.protobuf.Duration protoDuration = ProtoDurationConversion.toProtoDuration(duration);
    assertThat(ProtoDurationConversion.toJavaDuration(protoDuration)).isEqualTo(duration);
  }

  @Test
  public void closeToZeroNegativeDuration_roundtrip() {
    java.time.Duration duration = java.time.Duration.ofNanos(-454_785_843);
    com.google.protobuf.Duration protoDuration = ProtoDurationConversion.toProtoDuration(duration);
    assertThat(ProtoDurationConversion.toJavaDuration(protoDuration)).isEqualTo(duration);
  }

  @Test
  public void validPositiveDuration_roundtrip() {
    java.time.Duration duration =
        java.time.Duration.ofDays(2).plusHours(15).plusMinutes(12).plusSeconds(15).plusNanos(
            454_785_843);
    com.google.protobuf.Duration protoDuration = ProtoDurationConversion.toProtoDuration(duration);
    assertThat(ProtoDurationConversion.toJavaDuration(protoDuration)).isEqualTo(duration);
  }

  @Test
  public void validNegativeDuration_roundtrip() {
    java.time.Duration duration =
        java.time.Duration.ofDays(-2).minusHours(15).minusMinutes(12).minusSeconds(15).minusNanos(
            454_785_843);
    com.google.protobuf.Duration protoDuration = ProtoDurationConversion.toProtoDuration(duration);
    assertThat(ProtoDurationConversion.toJavaDuration(protoDuration)).isEqualTo(duration);
  }
}
