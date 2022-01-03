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

import static java.util.Comparator.comparingInt;

import java.util.Map;
import java.util.TreeMap;

/** Builder class for the LinearExpr container. */
public final class LinearExprBuilder {
  private final TreeMap<IntVar, Long> coefficients;
  private long offset;

  LinearExprBuilder() {
    this.coefficients = new TreeMap<>(comparingInt(IntVar::getIndex));
    this.offset = 0;
  }

  public LinearExprBuilder add(LinearExpr expr) {
    addTerm(expr, 1);
    return this;
  }

  public LinearExprBuilder add(Literal literal) {
    addTerm(literal, 1);
    return this;
  }

  public LinearExprBuilder add(long constant) {
    offset = offset + constant;
    return this;
  }

  public LinearExprBuilder addTerm(LinearExpr expr, long coeff) {
    final int numElements = expr.numElements();
    for (int i = 0; i < numElements; ++i) {
      coefficients.merge(expr.getVariable(i), expr.getCoefficient(i) * coeff, Long::sum);
    }
    offset = offset + expr.getOffset() * coeff;
    return this;
  }

  public LinearExprBuilder addTerm(Literal literal, long coeff) {
    final BoolVar boolVar = literal.getBoolVar();
    if (literal.negated()) {
      coefficients.merge(boolVar, -coeff, Long::sum);
      offset = offset + coeff;
    } else {
      coefficients.merge(boolVar, coeff, Long::sum);
    }
    return this;
  }

  public LinearExprBuilder addSum(IntVar[] vars) {
    for (final IntVar var : vars) {
      addTerm(var, 1);
    }
    return this;
  }

  public LinearExprBuilder addSum(Literal[] literals) {
    for (final Literal literal : literals) {
      addTerm(literal, 1);
    }
    return this;
  }

  public LinearExprBuilder addSum(LinearExpr[] exprs) {
    for (final LinearExpr expr : exprs) {
      addTerm(expr, 1);
    }
    return this;
  }

  public LinearExprBuilder addWeightedSum(IntVar[] vars, long[] coeffs) {
    for (int i = 0; i < vars.length; ++i) {
      addTerm(vars[i], coeffs[i]);
    }
    return this;
  }

  public LinearExprBuilder addWeightedSum(IntVar[] vars, int[] coeffs) {
    for (int i = 0; i < vars.length; ++i) {
      addTerm(vars[i], coeffs[i]);
    }
    return this;
  }

  public LinearExprBuilder addWeightedSum(Literal[] literals, long[] coeffs) {
    for (int i = 0; i < literals.length; ++i) {
      addTerm(literals[i], coeffs[i]);
    }
    return this;
  }

  public LinearExprBuilder addWeightedSum(Literal[] literals, int[] coeffs) {
    for (int i = 0; i < literals.length; ++i) {
      addTerm(literals[i], coeffs[i]);
    }
    return this;
  }

  public LinearExprBuilder addWeightedSum(LinearExpr[] exprs, long[] coeffs) {
    for (int i = 0; i < exprs.length; ++i) {
      addTerm(exprs[i], coeffs[i]);
    }
    return this;
  }

  public LinearExprBuilder addWeightedSum(LinearExpr[] exprs, int[] coeffs) {
    for (int i = 0; i < exprs.length; ++i) {
      addTerm(exprs[i], coeffs[i]);
    }
    return this;
  }

  public LinearExpr build() {
    boolean allOnes = true;
    int numElements = 0;
    IntVar lastVar = null;
    long lastCoeff = 0;
    for (Map.Entry<IntVar, Long> entry : coefficients.entrySet()) {
      if (entry.getValue() != 0) {
        numElements++;
        lastVar = entry.getKey();
        lastCoeff = entry.getValue();
        if (lastCoeff != 1) {
          allOnes = false;
        }
      }
    }
    if (numElements == 0) {
      return new ConstantExpression(offset);
    } else if (numElements == 1) {
      return new AffineExpression(lastVar, lastCoeff, offset);
    } else if (allOnes) {
      IntVar[] vars = new IntVar[numElements];
      int index = 0;
      for (Map.Entry<IntVar, Long> entry : coefficients.entrySet()) {
        if (entry.getValue() != 0) {
          vars[index] = entry.getKey();
          index++;
        }
      }
      return new SumExpression(vars, offset);
    } else {
      IntVar[] vars = new IntVar[numElements];
      long[] coeffs = new long[numElements];
      int index = 0;
      for (Map.Entry<IntVar, Long> entry : coefficients.entrySet()) {
        if (entry.getValue() != 0) {
          vars[index] = entry.getKey();
          coeffs[index] = entry.getValue();
          index++;
        }
      }
      return new WeightedSumExpression(vars, coeffs, offset);
    }
  }
}
