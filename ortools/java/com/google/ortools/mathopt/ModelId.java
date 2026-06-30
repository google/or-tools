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

/**
 * An object that uniquely identifies a model and whose identity can be used to test if components
 * of a model (e.g. {@link Variable} objects) came from the same model.
 *
 * <p>The field {@link #name} is used to generate better error messages (e.g. if you add a variable
 * to a constraint from a different model), but object identity determines if the model is the same.
 */
final class ModelId {
  private final String name;

  ModelId(String name) {
    this.name = name;
  }

  /** The name of the model, for display and error messages. */
  String getName() {
    return name;
  }
}
