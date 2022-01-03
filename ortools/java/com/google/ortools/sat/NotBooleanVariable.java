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

/**
 * The negation of a boolean variable. This class should not be used directly, Literal must be used
 * instead.
 */
public final class NotBooleanVariable implements Literal {
  public NotBooleanVariable(BoolVar boolVar) {
    this.boolVar = boolVar;
  }

  /** Internal: returns the index in the literal in the underlying CpModelProto. */
  @Override
  public int getIndex() {
    return -boolVar.getIndex() - 1;
  }

  /** Returns the negation of this literal. */
  @Override
  public Literal not() {
    return boolVar;
  }

  @Override
  public BoolVar getBoolVar() {
    return boolVar;
  }

  @Override
  public boolean negated() {
    return true;
  }

  // LinearExpr interface.
  @Override
  public int numElements() {
    return 1;
  }

  @Override
  public IntVar getVariable(int index) {
    if (index != 0) {
      throw new IllegalArgumentException("wrong index in LinearExpr.getVariable(): " + index);
    }
    return boolVar;
  }

  @Override
  public long getCoefficient(int index) {
    if (index != 0) {
      throw new IllegalArgumentException("wrong index in LinearExpr.getCoefficient(): " + index);
    }
    return -1;
  }

  @Override
  public long getOffset() {
    return 1;
  }

  /** Returns a short string describing this literal. */
  @Override
  public String getShortString() {
    return "not(" + boolVar.getShortString() + ")";
  }

  private final BoolVar boolVar;
}
