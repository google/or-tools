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

/** A specialized constant linear expression. */
public final class ConstantExpression implements LinearExpr {
  private final long offset;

  public ConstantExpression(long offset) {
    this.offset = offset;
  }

  // LinearArgument interface.
  @Override
  public LinearExpr build() {
    return this;
  }

  // LinearExpr interface.
  @Override
  public int numElements() {
    return 0;
  }

  @Override
  public int getVariableIndex(int index) {
    throw new IllegalArgumentException("wrong index in LinearExpr.getVariable(): " + index);
  }

  @Override
  public long getCoefficient(int index) {
    throw new IllegalArgumentException("wrong index in LinearExpr.getCoefficient(): " + index);
  }

  @Override
  public long getOffset() {
    return offset;
  }

  @Override
  public String toString() {
    return String.format("%d", offset);
  }
}
