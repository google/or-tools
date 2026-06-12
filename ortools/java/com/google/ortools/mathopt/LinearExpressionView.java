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

import java.util.Map;

/**
 * A read only view of an expression linear in {@link Variable} objects.
 *
 * <p>E.g. given variables x and y, the expression 3 * x + 4 * y - 7.0. We refer to 3 * x + 4 * y as
 * as {@link #unmodifiableTerms()} and -7.0 as the {@link #offset()}.
 */
public interface LinearExpressionView {
  double offset();

  /**
   * Returns an unmodifiable view of the variable coefficient pairs of the expression.
   *
   * <p>Modifying entries is not allowed and may result in an exception. Some values may be zero.
   *
   * <p>For some implementations, keys can be repeated, if so sum their values.
   */
  Iterable<Map.Entry<Variable, Double>> unmodifiableTerms();

  /**
   * Adds {@code scale} * {@code this} to {@code sink}.
   *
   * <p>Prefer calling {@link LinearExpressionSink#add(double, LinearExpressionView)} to invoking
   * this directly.
   *
   * <p>This is an advanced API that can offer a small performance improvement in some situations,
   * but can be ignored by most users. The default implementation method forces every coefficient to
   * be boxed and unboxed. This function is provided to avoid this boxing.
   */
  default void addTo(double scale, LinearExpressionSink sink) {
    sink.add(scale * offset());
    for (Map.Entry<Variable, Double> entry : unmodifiableTerms()) {
      sink.add(entry.getValue() * scale, entry.getKey());
    }
  }
}
