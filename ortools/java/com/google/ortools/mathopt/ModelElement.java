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
 * Components of a MathOpt model (e.g. variables, linear constraints) are assigned an id that is
 * unique for the Model and type of the component.
 *
 * <p>Ids start at zero and are assigned consecutively for each object of each type created in the
 * model. Note that if a model was loaded from proto, the ids will be increasing but can be
 * nonconsecutive (the ids are in the proto).
 *
 * <p>This interface allows us to write generic transformations from collections of objects from
 * MathOpt's java API, e.g. {@link Variable}, {@link LinearConstraint} to numerical data structures.
 * For example, you can sort a collection of variables by id and write their lower bounds to a
 * {@link com.google.ortools.mathopt.SparseDoubleVectorProto}.
 *
 * @see SparseContainers
 */
public interface ModelElement {
  /** A unique id for all objects of this type in the Model (e.g. all {@link Variable} objects). */
  long getId();
}
