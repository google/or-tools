// Copyright 2010-2024 Google LLC
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
  private final int[] variableIndices;
  private final double[] coefficients;
  private double offset;

  /** Creates a sum expression. */
  public static DoubleLinearExpr sum(IntVar[] variables) {
    return sumWithOffset(variables, 0.0);
  }

  /** Creates a sum expression. */
  public static DoubleLinearExpr sum(Literal[] literals) {
    // We need the scalar product for the negative coefficient of negated Boolean variables.
    return sumWithOffset(literals, 0.0);
  }

  /** Creates a sum expression with a double offset. */
  public static DoubleLinearExpr sumWithOffset(IntVar[] variables, double offset) {
    return new DoubleLinearExpr(variables, offset);
  }

  /** Creates a sum expression with a double offset. */
  public static DoubleLinearExpr sumWithOffset(Literal[] literals, double offset) {
    // We need the scalar product for the negative coefficient of negated Boolean variables.
    return new DoubleLinearExpr(literals, offset);
  }

  /** Creates a scalar product. */
  public static DoubleLinearExpr weightedSum(IntVar[] variables, double[] coefficients) {
    return weightedSumWithOffset(variables, coefficients, 0.0);
  }

  /** Creates a scalar product. */
  public static DoubleLinearExpr weightedSum(Literal[] literals, double[] coefficients) {
    return weightedSumWithOffset(literals, coefficients, 0.0);
  }

  /** Creates a scalar product. */
  public static DoubleLinearExpr weightedSumWithOffset(
      IntVar[] variables, double[] coefficients, double offset) {
    if (variables.length != coefficients.length) {
      throw new CpModel.MismatchedArrayLengths(
          "DoubleLinearExpr.weightedSum", "variables", "coefficients");
    }
    return new DoubleLinearExpr(variables, coefficients, offset);
  }

  /** Creates a scalar product with a double offset. */
  public static DoubleLinearExpr weightedSumWithOffset(
      Literal[] literals, double[] coefficients, double offset) {
    if (literals.length != coefficients.length) {
      throw new CpModel.MismatchedArrayLengths(
          "DoubleLinearExpr.weightedSum", "literals", "coefficients");
    }
    return new DoubleLinearExpr(literals, coefficients, offset);
  }

  /** Creates a linear term (var * coefficient). */
  public static DoubleLinearExpr term(IntVar variable, double coefficient) {
    return new DoubleLinearExpr(variable, coefficient, 0.0);
  }

  /** Creates a linear term (lit * coefficient). */
  public static DoubleLinearExpr term(Literal lit, double coefficient) {
    return new DoubleLinearExpr(lit, coefficient, 0.0);
  }

  /** Creates an affine expression (var * coefficient + offset). */
  public static DoubleLinearExpr affine(IntVar variable, double coefficient, double offset) {
    return new DoubleLinearExpr(variable, coefficient, offset);
  }

  /** Creates an affine expression (lit * coefficient + offset). */
  public static DoubleLinearExpr affine(Literal lit, double coefficient, double offset) {
    return new DoubleLinearExpr(lit, coefficient, offset);
  }

  /** Creates a constant expression. */
  public static DoubleLinearExpr constant(double value) {
    return new DoubleLinearExpr(new IntVar[0], value);
  }

  /** Returns the number of elements in the interface. */
  public int numElements() {
    return variableIndices.length;
  }

  /** Returns the ith variable. */
  public int getVariableIndex(int index) {
    if (index < 0 || index >= variableIndices.length) {
      throw new IllegalArgumentException("wrong index in LinearExpr.getVariable(): " + index);
    }
    return variableIndices[index];
  }

  /** Returns the ith coefficient. */
  public double getCoefficient(int index) {
    if (index < 0 || index >= variableIndices.length) {
      throw new IllegalArgumentException("wrong index in LinearExpr.getCoefficient(): " + index);
    }
    return coefficients[index];
  }

  /** Returns the constant part of the expression. */
  public double getOffset() {
    return offset;
  }

  public DoubleLinearExpr(IntVar[] variables, double[] coefficients, double offset) {
    this.variableIndices = new int[variables.length];
    for (int i = 0; i < variables.length; ++i) {
      this.variableIndices[i] = variables[i].getIndex();
    }
    this.coefficients = coefficients;
    this.offset = offset;
  }

  public DoubleLinearExpr(Literal[] literals, double[] coefficients, double offset) {
    int size = literals.length;
    this.variableIndices = new int[size];
    this.coefficients = new double[size];
    this.offset = offset;

    for (int i = 0; i < size; ++i) {
      Literal lit = literals[i];
      double coeff = coefficients[i];
      if (lit.getIndex() >= 0) {
        this.variableIndices[i] = lit.getIndex();
        this.coefficients[i] = coeff;
      } else {
        this.variableIndices[i] = lit.not().getIndex();
        this.coefficients[i] = -coeff;
        this.offset -= coeff;
      }
    }
  }

  public DoubleLinearExpr(IntVar var, double coefficient, double offset) {
    this.variableIndices = new int[] {var.getIndex()};
    this.coefficients = new double[] {coefficient};
    this.offset = offset;
  }

  public DoubleLinearExpr(Literal lit, double coefficient, double offset) {
    if (lit.getIndex() >= 0) {
      this.variableIndices = new int[] {lit.getIndex()};
      this.coefficients = new double[] {coefficient};
      this.offset = offset;
    } else {
      this.variableIndices = new int[] {lit.not().getIndex()};
      this.coefficients = new double[] {-coefficient};
      this.offset = offset + coefficient;
    }
  }

  public DoubleLinearExpr(IntVar[] vars, double offset) {
    int size = vars.length;
    this.variableIndices = new int[size];
    this.coefficients = new double[size];
    this.offset = offset;

    for (int i = 0; i < size; ++i) {
      this.variableIndices[i] = vars[i].getIndex();
      this.coefficients[i] = 1;
    }
  }

  public DoubleLinearExpr(Literal[] literals, double offset) {
    int size = literals.length;
    this.variableIndices = new int[size];
    this.coefficients = new double[size];
    this.offset = offset;

    for (int i = 0; i < size; ++i) {
      Literal lit = literals[i];
      if (lit.getIndex() >= 0) {
        this.variableIndices[i] = lit.getIndex();
        this.coefficients[i] = 1;
      } else { // NotBoolVar.
        this.variableIndices[i] = lit.not().getIndex();
        this.coefficients[i] = -1.0;
        this.offset -= 1.0;
      }
    }
  }
}
