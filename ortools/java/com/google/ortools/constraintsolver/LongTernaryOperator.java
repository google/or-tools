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
 * Represents an operation upon three {@code long}-valued operands and producing a
 * {@code long}-valued result.  This is the primitive type specialization of
 * TernaryOperator for {@code long}.
 *
 * <p>This is a functional interface
 * whose functional method is {@link #applyAsLong(long, long, long)}.
 * @see BinaryOperator
 * @see LongUnaryOperator.
 */
@FunctionalInterface
public interface LongTernaryOperator {
  /**
   * Applies this operator to the given operands.
   *
   * @param left the first operand
   * @param center the second operand
   * @param right the third operand
   * @return the operator result
   */
  long applyAsLong(long left, long center, long right);
}
