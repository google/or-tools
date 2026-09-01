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

import com.google.ortools.sat.LinearExpressionProto;

/** A linear expression (sum (ai * xi) + b). It specifies methods to help parsing the expression. */
public interface LinearExpr extends LinearArgument {
  /** Returns the number of terms (excluding the constant one) in this expression. */
  int numElements();

  /** Returns the index of the ith variable. */
  int getVariableIndex(int index);

  /** Returns the ith coefficient. */
  long getCoefficient(int index);

  /** Returns the constant part of the expression. */
  long getOffset();

  /** Returns a builder */
  static LinearExprBuilder newBuilder() {
    return new LinearExprBuilder();
  }

  /** Shortcut for newBuilder().add(value).build() */
  static LinearExpr constant(long value) {
    return newBuilder().add(value).build();
  }

  /** Shortcut for newBuilder().addTerm(expr, coeff).build() */
  static LinearExpr term(LinearArgument expr, long coeff) {
    return newBuilder().addTerm(expr, coeff).build();
  }

  /** Shortcut for newBuilder().addTerm(expr, coeff).add(offset).build() */
  static LinearExpr affine(LinearArgument expr, long coeff, long offset) {
    return newBuilder().addTerm(expr, coeff).add(offset).build();
  }

  /** Shortcut for newBuilder().addSum(exprs).build() */
  static LinearExpr sum(LinearArgument[] exprs) {
    return newBuilder().addSum(exprs).build();
  }

  /** Shortcut for newBuilder().addWeightedSum(exprs, coeffs).build() */
  static LinearExpr weightedSum(LinearArgument[] exprs, long[] coeffs) {
    return newBuilder().addWeightedSum(exprs, coeffs).build();
  }

  static LinearExpr rebuildFromLinearExpressionProto(LinearExpressionProto proto) {
    int numElements = proto.getVarsCount();
    if (numElements == 0) {
      return new ConstantExpression(proto.getOffset());
    } else if (numElements == 1) {
      return new AffineExpression(proto.getVars(0), proto.getCoeffs(0), proto.getOffset());
    } else {
      int[] varsIndices = new int[numElements];
      long[] coeffs = new long[numElements];
      long offset = proto.getOffset();
      for (int i = 0; i < numElements; ++i) {
        varsIndices[i] = proto.getVars(i);
        coeffs[i] = proto.getCoeffs(i);
      }
      return new WeightedSumExpression(varsIndices, coeffs, offset);
    }
  }
}
