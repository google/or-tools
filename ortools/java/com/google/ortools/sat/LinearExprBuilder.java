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

import java.util.Map;
import java.util.TreeMap;

/** Builder class for the LinearExpr container. */
public final class LinearExprBuilder implements LinearArgument {
  private final TreeMap<Integer, Long> coefficients;
  private long offset;

  LinearExprBuilder() {
    this.coefficients = new TreeMap<>();
    this.offset = 0;
  }

  public LinearExprBuilder add(LinearArgument expr) {
    addTerm(expr, 1);
    return this;
  }

  public LinearExprBuilder add(long constant) {
    offset = offset + constant;
    return this;
  }

  public LinearExprBuilder addTerm(LinearArgument expr, long coeff) {
    final LinearExpr e = expr.build();
    final int numElements = e.numElements();
    for (int i = 0; i < numElements; ++i) {
      coefficients.merge(e.getVariableIndex(i), e.getCoefficient(i) * coeff, Long::sum);
    }
    offset = offset + e.getOffset() * coeff;
    return this;
  }

  public LinearExprBuilder addSum(LinearArgument[] exprs) {
    for (final LinearArgument expr : exprs) {
      addTerm(expr, 1);
    }
    return this;
  }

  public LinearExprBuilder addWeightedSum(LinearArgument[] exprs, long[] coeffs) {
    for (int i = 0; i < exprs.length; ++i) {
      addTerm(exprs[i], coeffs[i]);
    }
    return this;
  }

  public LinearExprBuilder addWeightedSum(LinearArgument[] exprs, int[] coeffs) {
    for (int i = 0; i < exprs.length; ++i) {
      addTerm(exprs[i], coeffs[i]);
    }
    return this;
  }

  @Override
  public LinearExpr build() {
    int numElements = 0;
    int lastVarIndex = -1;
    long lastCoeff = 0;
    for (Map.Entry<Integer, Long> entry : coefficients.entrySet()) {
      if (entry.getValue() != 0) {
        numElements++;
        lastVarIndex = entry.getKey();
        lastCoeff = entry.getValue();
      }
    }
    if (numElements == 0) {
      return new ConstantExpression(offset);
    } else if (numElements == 1) {
      return new AffineExpression(lastVarIndex, lastCoeff, offset);
    } else {
      int[] varIndices = new int[numElements];
      long[] coeffs = new long[numElements];
      int index = 0;
      for (Map.Entry<Integer, Long> entry : coefficients.entrySet()) {
        if (entry.getValue() != 0) {
          varIndices[index] = entry.getKey();
          coeffs[index] = entry.getValue();
          index++;
        }
      }
      return new WeightedSumExpression(varIndices, coeffs, offset);
    }
  }
}
