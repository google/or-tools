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

import com.google.ortools.mathopt.CallbackDataProto;
import com.google.ortools.mathopt.CallbackResultProto;
import java.util.Optional;
import java.util.function.Function;
import org.jspecify.annotations.Nullable;

/**
 * Wraps a CppSolveCallback in an AutoCloseable so SWIGed Cpp object can be cleaned up using
 * try-with-resources.
 *
 * <p>Note that the underlying CppSolveCallback is nullable.
 *
 * <p>This class is not thread-safe.
 */
final class ScopedCppSolveCallback implements AutoCloseable {
  @Nullable CppSolveCallback cppSolveCallback = null;

  /**
   * If {@code userCallback} is nonempty, creates a {@link CppSolveCallback} that invokes {@code
   * userCallback}.
   */
  ScopedCppSolveCallback(Optional<Function<CallbackDataProto, CallbackResultProto>> userCallback) {
    if (userCallback.isPresent()) {
      cppSolveCallback = new CppSolveCallback() {
        @Override
        public CallbackResultProto onEvent(CallbackDataProto callbackData) {
          return userCallback.get().apply(callbackData);
        }
      };
    }
  }

  /**
   * Returns the underlying {@link CppSolveCallback} if it was created.
   *
   * <p>The returned object must not be used after calling {@link #close()}, doing so will cause a
   * crash or memory corruption in C++.
   */
  @Nullable
  CppSolveCallback getCppSolveCallback() {
    return cppSolveCallback;
  }

  @Override
  public void close() {
    if (cppSolveCallback != null) {
      cppSolveCallback.delete();
      cppSolveCallback = null;
    }
  }
}
