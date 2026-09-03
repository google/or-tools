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

import com.google.common.base.Splitter;
import com.google.common.collect.ImmutableList;
import java.util.Optional;
import java.util.function.Consumer;
import org.jspecify.annotations.Nullable;

/**
 * Wraps a CppMessageCallback in an AutoCloseable so SWIGed Cpp object can be cleaned up using
 * try-with-resources.
 *
 * <p>Note that the underlying CppMessageCallback is nullable.
 *
 * <p>This class is not thread-safe.
 */
final class ScopedCppMessageCallback implements AutoCloseable {
  @Nullable CppMessageCallback cppMessageCallback = null;

  /**
   * If {@code userCallback} is nonempty, creates a {@link CppMessageCallback} that invokes {@code
   * userCallback}.
   */
  ScopedCppMessageCallback(Optional<Consumer<ImmutableList<String>>> userCallback) {
    if (userCallback.isPresent()) {
      cppMessageCallback = new CppMessageCallback() {
        @Override
        public void onMessage(String message) {
          userCallback.get().accept(ImmutableList.copyOf(Splitter.on('\n').split(message)));
        }
      };
    }
  }

  /**
   * Returns the underlying {@link CppMessageCallback} if it was created.
   *
   * <p>The returned object must not be used after calling {@link #close()}, doing so will cause a
   * crash or memory corruption in C++.
   */
  @Nullable
  CppMessageCallback getCppMessageCallback() {
    return cppMessageCallback;
  }

  @Override
  public void close() {
    if (cppMessageCallback != null) {
      cppMessageCallback.delete();
      cppMessageCallback = null;
    }
  }
}
