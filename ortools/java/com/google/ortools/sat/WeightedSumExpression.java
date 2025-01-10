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

package com.google.ortools.sat;

/** A specialized linear expression: sum(ai * xi) + b. */
public final class WeightedSumExpression implements LinearExpr {
  private final int[] variablesIndices;
  private final long[] coefficients;
  private final long offset;

  public WeightedSumExpression(int[] variablesIndices, long[] coefficients, long offset) {
    this.variablesIndices = variablesIndices;
    this.coefficients = coefficients;
    this.offset = offset;
  }

  @Override
  public LinearExpr build() {
    return this;
  }

  @Override
  public int numElements() {
    return variablesIndices.length;
  }

  @Override
  public int getVariableIndex(int index) {
    if (index < 0 || index >= variablesIndices.length) {
      throw new IllegalArgumentException("wrong index in LinearExpr.getVariable(): " + index);
    }
    return variablesIndices[index];
  }

  @Override
  public long getCoefficient(int index) {
    if (index < 0 || index >= variablesIndices.length) {
      throw new IllegalArgumentException("wrong index in LinearExpr.getCoefficient(): " + index);
    }
    return coefficients[index];
  }

  @Override
  public long getOffset() {
    return offset;
  }
}
