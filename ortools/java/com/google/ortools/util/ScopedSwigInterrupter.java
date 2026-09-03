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

import org.jspecify.annotations.Nullable;

/**
 * Wraps a JavaSwigSolveInterrupter in an AutoCloseable so SWIGed Cpp object can be cleaned up using
 * try-with-resources.
 *
 * <p>Note that the underlying JavaSwigSolveInterrupter is nullable.
 *
 * <p>This class is not thread-safe.
 */
public class ScopedSwigInterrupter implements AutoCloseable, Interruptable {
  // Use nullable instead of optional because the JniSolver.cppSolve() wants a nullable value.
  @Nullable JavaSwigSolveInterrupter swigInterrupter;

  /** The underlying {@link JavaSwigSolveInterrupter} is created only when {@code isPresent.} */
  public ScopedSwigInterrupter(boolean isPresent) {
    swigInterrupter = isPresent ? new JavaSwigSolveInterrupter() : null;
  }

  /** The underlying {@link JavaSwigSolveInterrupter} is created. */
  public ScopedSwigInterrupter() {
    this(true);
  }

  /**
   * Returns the underlying SWIGed interrupter, which is non-null if and only if the input user
   * interrupter is non-null.
   *
   * <p>The returned object must not be used after calling {@link #close()}, doing so will cause a
   * crash or memory corruption in C++.
   */
  public @Nullable JavaSwigSolveInterrupter getSwigInterrupter() {
    return swigInterrupter;
  }

  @Override
  public void close() {
    if (swigInterrupter != null) {
      swigInterrupter.delete();
      swigInterrupter = null;
    }
  }

  /**
   * Interrupts the underlying {@link JavaSwigSolveInterrupter}, has no effect if it is initialized
   * as null.
   */
  @Override
  public void interrupt() {
    if (swigInterrupter != null) {
      swigInterrupter.interrupt();
    }
  }
}
