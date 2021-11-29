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

/** the substraction of two linear expressions. Used internally. */
final class Difference implements LinearExpr {
  private final LinearExpr left;
  private final LinearExpr right;

  public Difference(LinearExpr left, LinearExpr right) {
    this.left = left;
    this.right = right;
  }

  @Override
  public int numElements() {
    return left.numElements() + right.numElements();
  }

  @Override
  public IntVar getVariable(int index) {
    if (index < left.numElements()) {
      return left.getVariable(index);
    } else {
      return right.getVariable(index - left.numElements());
    }
  }

  @Override
  public long getCoefficient(int index) {
    if (index < left.numElements()) {
      return left.getCoefficient(index);
    } else {
      return -right.getCoefficient(index - left.numElements());
    }
  }

  @Override
  public long getOffset() {
    return left.getOffset() - right.getOffset();
  }
}
