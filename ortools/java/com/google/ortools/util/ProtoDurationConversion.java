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

import com.google.protobuf.util.Durations;

/**
 * A utility class that provides conversions between {@link com.google.protobuf.Duration} and {@link
 * java.time.Duration}.
 */
public final class ProtoDurationConversion {
  private ProtoDurationConversion() {}

  /**
   * Converts a {@link com.google.protobuf.Duration} to a {@link java.time.Duration}.
   *
   * @throws ArithmeticException if it does not fit in a {@link java.time.Duration}.
   */
  public static java.time.Duration toJavaDuration(com.google.protobuf.Duration protoDuration) {
    // Note that java.time.Duration.ofSeconds() handles negative nanos seconds correctly.
    return java.time.Duration.ofSeconds(protoDuration.getSeconds(), protoDuration.getNanos());
  }

  /**
   * Converts a {@link java.time.Duration} to a {@link com.google.protobuf.Duration}.
   *
   * @throws ArithmeticException if the given duration does not fit in {@link long} in nanoseconds.
   */
  public static com.google.protobuf.Duration toProtoDuration(java.time.Duration duration) {
    // Note that here we don't support cases of durations that don't fit in `long` when using
    // nanoseconds.
    return Durations.fromNanos(duration.toNanos());
  }
}
