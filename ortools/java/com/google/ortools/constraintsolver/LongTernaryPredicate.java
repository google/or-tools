// Copyright 2010-2021 Google LLC
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

package com.google.ortools.constraintsolver;

/**
 * Represents a predicate (boolean-valued function) uppon
 * three {@code long}-valued operands. This is the {@code long}-consuming primitive type
 * specialization of {@link Predicate}.
 *
 * <p>This is a functional interface
 * whose functional method is {@link #test(long, long, long)}.
 * @see Predicate
 */
@FunctionalInterface
public interface LongTernaryPredicate {
  /**
   * Evaluates this predicate on the given arguments.
   *
   * @param left the first operand
   * @param center the second operand
   * @param right the third operand
   * @return {@code true} if the input argument matches the predicate,
   * otherwise {@code false}
   */
  boolean test(long left, long center, long right);

  /**
   * Returns a predicate that represents the logical negation of this
   * predicate.
   *
   * @return a predicate that represents the logical negation of this
   * predicate
   */
  default LongTernaryPredicate negate() {
    return (left, center, right) -> !test(left, center, right);
  }
}
