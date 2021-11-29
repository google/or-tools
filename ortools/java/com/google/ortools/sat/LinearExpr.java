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
public interface LinearExpr {
  /** Returns the number of elements in the interface. */
  int numElements();

  /** Returns the ith variable. */
  IntVar getVariable(int index);

  /** Returns the ith coefficient. */
  long getCoefficient(int index);

  /** Returns the constant part of the expression. */
  long getOffset();

  /** Creates a sum expression. */
  static LinearExpr sum(IntVar[] variables) {
    return new SumOfVariables(variables);
  }

  /** Creates a sum expression. */
  static LinearExpr booleanSum(Literal[] literals) {
    // We need the scalar product for the negative coefficient of negated Boolean variables.
    return new ScalProd(literals);
  }

  /** Creates a scalar product. */
  static LinearExpr scalProd(IntVar[] variables, long[] coefficients) {
    if (variables.length != coefficients.length) {
      throw new CpModel.MismatchedArrayLengths("LinearExpr.scalProd", "variables", "coefficients");
    }
    return new ScalProd(variables, coefficients);
  }

  /** Creates a scalar product. */
  static LinearExpr scalProd(IntVar[] variables, int[] coefficients) {
    if (variables.length != coefficients.length) {
      throw new CpModel.MismatchedArrayLengths("LinearExpr.scalProd", "variables", "coefficients");
    }
    long[] tmp = new long[coefficients.length];
    for (int i = 0; i < coefficients.length; ++i) {
      tmp[i] = coefficients[i];
    }
    return new ScalProd(variables, tmp);
  }

  /** Creates a scalar product. */
  static LinearExpr booleanScalProd(Literal[] literals, long[] coefficients) {
    if (literals.length != coefficients.length) {
      throw new CpModel.MismatchedArrayLengths("LinearExpr.scalProd", "literals", "coefficients");
    }
    return new ScalProd(literals, coefficients);
  }

  /** Creates a scalar product. */
  static LinearExpr booleanScalProd(Literal[] literals, int[] coefficients) {
    if (literals.length != coefficients.length) {
      throw new CpModel.MismatchedArrayLengths("LinearExpr.scalProd", "literals", "coefficients");
    }

    long[] tmp = new long[coefficients.length];
    for (int i = 0; i < coefficients.length; ++i) {
      tmp[i] = coefficients[i];
    }
    return new ScalProd(literals, tmp);
  }

  /** Creates a linear term (var * coefficient). */
  static LinearExpr term(IntVar variable, long coefficient) {
    return new ScalProd(variable, coefficient, 0);
  }

  /** Creates a linear term (lit * coefficient). */
  static LinearExpr term(Literal lit, long coefficient) {
    return new ScalProd(lit, coefficient, 0);
  }

  /** Creates an affine expression (var * coefficient + offset). */
  static LinearExpr affine(IntVar variable, long coefficient, long offset) {
    return new ScalProd(variable, coefficient, offset);
  }

  /** Creates an affine expression (lit * coefficient + offset). */
  static LinearExpr affine(Literal lit, long coefficient, long offset) {
    return new ScalProd(lit, coefficient, offset);
  }

  /** Creates an constant expression. */
  static LinearExpr constant(long value) {
    return new Constant(value);
  }
}
