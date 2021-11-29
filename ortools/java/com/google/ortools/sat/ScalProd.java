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
public final class ScalProd implements LinearExpr {
  private final IntVar[] variables;
  private final long[] coefficients;
  private long offset;

  public ScalProd(IntVar[] variables, long[] coefficients) {
    this.variables = variables;
    this.coefficients = coefficients;
    this.offset = 0;
  }

  public ScalProd(Literal[] literals, long[] coefficients) {
    int size = literals.length;
    this.variables = new IntVar[size];
    this.coefficients = new long[size];
    this.offset = 0;

    for (int i = 0; i < size; ++i) {
      Literal lit = literals[i];
      long coeff = coefficients[i];
      if (lit.getIndex() >= 0) {
        this.variables[i] = (IntVar) lit;
        this.coefficients[i] = coeff;
      } else {
        this.variables[i] = (IntVar) lit.not();
        this.coefficients[i] = -coeff;
        this.offset -= coeff;
      }
    }
  }

  public ScalProd(IntVar var, long coefficient, long offset) {
    this.variables = new IntVar[] {var};
    this.coefficients = new long[] {coefficient};
    this.offset = offset;
  }

  public ScalProd(Literal lit, long coefficient, long offset) {
    if (lit.getIndex() >= 0) {
      this.variables = new IntVar[] {(IntVar) lit};
      this.coefficients = new long[] {coefficient};
      this.offset = offset;
    } else {
      this.variables = new IntVar[] {(IntVar) lit.not()};
      this.coefficients = new long[] {-coefficient};
      this.offset = offset + coefficient;
    }
  }

  public ScalProd(Literal[] literals) {
    int size = literals.length;
    this.variables = new IntVar[size];
    this.coefficients = new long[size];
    this.offset = 0;

    for (int i = 0; i < size; ++i) {
      Literal lit = literals[i];
      if (lit.getIndex() >= 0) {
        this.variables[i] = (IntVar) lit;
        this.coefficients[i] = 1;
      } else { // NotBooleanVar.
        this.variables[i] = (IntVar) lit.not();
        this.coefficients[i] = -1;
        this.offset -= 1;
      }
    }
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
    if (index < 0 || index >= variables.length) {
      throw new IllegalArgumentException("wrong index in LinearExpr.getCoefficient(): " + index);
    }
    return coefficients[index];
  }

  @Override
  public long getOffset() {
    return offset;
  }
}
