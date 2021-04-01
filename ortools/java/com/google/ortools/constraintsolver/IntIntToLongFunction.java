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
 * Represents a function that accepts two int-valued arguments and produces a
 * long-valued result.  This is the {@code int}{@code int}-to-{@code long} primitive
 * specialization for {@link Function}.
 *
 * <p>This is a functional interface
 * whose functional method is {@link #applyAsLong(long, long)}.
 * @see Function
 * @see IntToLongFunction.
 */
@FunctionalInterface
public interface IntIntToLongFunction {
  /**
   * Applies this function to the given arguments.
   *
   * @param left the first argument
   * @param right the second argument
   * @return the function result
   */
  long applyAsLong(int left, int right);
}
