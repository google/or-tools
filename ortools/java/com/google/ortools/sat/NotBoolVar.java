// Copyright 2010-2022 Google LLC
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

/**
 * The negation of a boolean variable. This class should not be used directly, Literal must be used
 * instead.
 */
public final class NotBoolVar implements Literal {
  public NotBoolVar(BoolVar boolVar) {
    this.boolVar = boolVar;
  }

  /** returns the index in the literal in the underlying CpModelProto. */
  @Override
  public int getIndex() {
    return -boolVar.getIndex() - 1;
  }

  /** Returns the negation of this literal. */
  @Override
  public Literal not() {
    return boolVar;
  }

  // LinearArgument interface.
  @Override
  public LinearExpr build() {
    return new AffineExpression(boolVar.getIndex(), -1, 1);
  }

  /** Returns a short string describing this literal. */
  @Override
  public String toString() {
    return "not(" + boolVar + ")";
  }

  private final BoolVar boolVar;
}
