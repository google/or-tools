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

package com.google.ortools.sat;

/** A linear expression interface that can be parsed. */
public final class SumOfVariables implements LinearExpr {
  private final IntVar[] variables;
  private final long offset;

  public SumOfVariables(IntVar[] variables) {
    this.variables = variables;
    this.offset = 0;
  }

  public SumOfVariables(IntVar[] variables, long offset) {
    this.variables = variables;
    this.offset = offset;
  }

  @Override
  public int numElements() {
    return variables.length;
  }

  @Override
  public IntVar getVariable(int index) {
    if (index < 0 || index >= variables.length) {
      throw new IllegalArgumentException("wrong index in LinearExpr.getVariable(): " + index);
    }
    return variables[index];
  }

  @Override
  public long getCoefficient(int index) {
    return 1;
  }

  @Override
  public long getOffset() {
    return offset;
  }
}
