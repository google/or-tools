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
public class DoubleLinearExpr {
  private final IntVar[] variables;
  private final double[] coefficients;
  private double offset;

  /** Creates a sum expression. */
  static DoubleLinearExpr sum(IntVar[] variables) {
    return sumWithOffset(variables, 0.0);
  }

  /** Creates a sum expression with a double offset. */
  static DoubleLinearExpr sumWithOffset(IntVar[] variables, double offset) {
    return new DoubleLinearExpr(variables, offset);
  }

  /** Creates a sum expression. */
  static DoubleLinearExpr booleanSum(Literal[] literals) {
    // We need the scalar product for the negative coefficient of negated Boolean variables.
    return booleanSumWithOffset(literals, 0.0);
  }

  /** Creates a sum expression with a double offset. */
  static DoubleLinearExpr booleanSumWithOffset(Literal[] literals, double offset) {
    // We need the scalar product for the negative coefficient of negated Boolean variables.
    return new DoubleLinearExpr(literals, offset);
  }

  /** Creates a scalar product. */
  static DoubleLinearExpr scalProd(IntVar[] variables, double[] coefficients) {
    return scalProdWithOffset(variables, coefficients, 0.0);
  }

  /** Creates a scalar product. */
  static DoubleLinearExpr scalProdWithOffset(
      IntVar[] variables, double[] coefficients, double offset) {
    if (variables.length != coefficients.length) {
      throw new CpModel.MismatchedArrayLengths(
          "DoubleLinearExpr.scalProd", "variables", "coefficients");
    }
    return new DoubleLinearExpr(variables, coefficients, offset);
  }

  /** Creates a scalar product. */
  static DoubleLinearExpr booleanScalProd(Literal[] literals, double[] coefficients) {
    return booleanScalProdWithOffset(literals, coefficients, 0.0);
  }

  /** Creates a scalar product with a double offset. */
  static DoubleLinearExpr booleanScalProdWithOffset(
      Literal[] literals, double[] coefficients, double offset) {
    if (literals.length != coefficients.length) {
      throw new CpModel.MismatchedArrayLengths(
          "DoubleLinearExpr.scalProd", "literals", "coefficients");
    }
    return new DoubleLinearExpr(literals, coefficients, offset);
  }

  /** Creates a linear term (var * coefficient). */
  static DoubleLinearExpr term(IntVar variable, double coefficient) {
    return new DoubleLinearExpr(variable, coefficient, 0.0);
  }

  /** Creates a linear term (lit * coefficient). */
  static DoubleLinearExpr term(Literal lit, double coefficient) {
    return new DoubleLinearExpr(lit, coefficient, 0.0);
  }

  /** Creates an affine expression (var * coefficient + offset). */
  static DoubleLinearExpr affine(IntVar variable, double coefficient, double offset) {
    return new DoubleLinearExpr(variable, coefficient, offset);
  }

  /** Creates an affine expression (lit * coefficient + offset). */
  static DoubleLinearExpr affine(Literal lit, double coefficient, double offset) {
    return new DoubleLinearExpr(lit, coefficient, offset);
  }

  /** Creates an constant expression. */
  static DoubleLinearExpr constant(double value) {
    return new DoubleLinearExpr(new IntVar[0], value);
  }

  /** Returns the number of elements in the interface. */
  public int numElements() {
    return variables.length;
  }

  /** Returns the ith variable. */
  public IntVar getVariable(int index) {
    if (index < 0 || index >= variables.length) {
      throw new IllegalArgumentException("wrong index in LinearExpr.getVariable(): " + index);
    }
    return variables[index];
  }

  /** Returns the ith coefficient. */
  public double getCoefficient(int index) {
    if (index < 0 || index >= variables.length) {
      throw new IllegalArgumentException("wrong index in LinearExpr.getCoefficient(): " + index);
    }
    return coefficients[index];
  }

  /** Returns the constant part of the expression. */
  public double getOffset() {
    return offset;
  }

  public DoubleLinearExpr(IntVar[] variables, double[] coefficients, double offset) {
    this.variables = variables;
    this.coefficients = coefficients;
    this.offset = offset;
  }

  public DoubleLinearExpr(Literal[] literals, double[] coefficients, double offset) {
    int size = literals.length;
    this.variables = new IntVar[size];
    this.coefficients = new double[size];
    this.offset = offset;

    for (int i = 0; i < size; ++i) {
      Literal lit = literals[i];
      double coeff = coefficients[i];
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

  public DoubleLinearExpr(IntVar var, double coefficient, double offset) {
    this.variables = new IntVar[] {var};
    this.coefficients = new double[] {coefficient};
    this.offset = offset;
  }

  public DoubleLinearExpr(Literal lit, double coefficient, double offset) {
    if (lit.getIndex() >= 0) {
      this.variables = new IntVar[] {(IntVar) lit};
      this.coefficients = new double[] {coefficient};
      this.offset = offset;
    } else {
      this.variables = new IntVar[] {(IntVar) lit.not()};
      this.coefficients = new double[] {-coefficient};
      this.offset = offset + coefficient;
    }
  }

  public DoubleLinearExpr(IntVar[] vars, double offset) {
    int size = vars.length;
    this.variables = new IntVar[size];
    this.coefficients = new double[size];
    this.offset = offset;

    for (int i = 0; i < size; ++i) {
      this.variables[i] = vars[i];
      this.coefficients[i] = 1;
    }
  }

  public DoubleLinearExpr(Literal[] literals, double offset) {
    int size = literals.length;
    this.variables = new IntVar[size];
    this.coefficients = new double[size];
    this.offset = offset;

    for (int i = 0; i < size; ++i) {
      Literal lit = literals[i];
      if (lit.getIndex() >= 0) {
        this.variables[i] = (IntVar) lit;
        this.coefficients[i] = 1;
      } else { // NotBooleanVar.
        this.variables[i] = (IntVar) lit.not();
        this.coefficients[i] = -1.0;
        this.offset -= 1.0;
      }
    }
  }
}
