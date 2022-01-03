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

import com.google.ortools.sat.CpModelProto;
import com.google.ortools.sat.LinearExpressionProto;

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

  /** Returns a builder */
  static LinearExprBuilder newBuilder() {
    return new LinearExprBuilder();
  }

  /** Shortcut for newBuilder().add(value).build() */
  static LinearExpr constant(long value) {
    return newBuilder().add(value).build();
  }

  /** Shortcut for newBuilder().addTerm(var, coeff).build() */
  static LinearExpr term(IntVar var, long coeff) {
    return newBuilder().addTerm(var, coeff).build();
  }

  /** Shortcut for newBuilder().addTerm(expr, coeff).build() */
  static LinearExpr term(LinearExpr expr, long coeff) {
    return newBuilder().addTerm(expr, coeff).build();
  }

  /** Shortcut for newBuilder().addTerm(literal, coeff).build() */
  static LinearExpr term(Literal literal, long coeff) {
    return newBuilder().addTerm(literal, coeff).build();
  }

  /** Shortcut for newBuilder().addSum(vars).build() */
  static LinearExpr sum(IntVar[] vars) {
    return newBuilder().addSum(vars).build();
  }

  /** Shortcut for newBuilder().addSum(exprs).build() */
  static LinearExpr sum(LinearExpr[] exprs) {
    return newBuilder().addSum(exprs).build();
  }

  /** Shortcut for newBuilder().addSum(literals).build() */
  static LinearExpr sum(Literal[] literals) {
    return newBuilder().addSum(literals).build();
  }

  /** Shortcut for newBuilder().addWeightedSum(vars, coeffs).build() */
  static LinearExpr weightedSum(IntVar[] vars, long[] coeffs) {
    return newBuilder().addWeightedSum(vars, coeffs).build();
  }

  /** Shortcut for newBuilder().addWeightedSum(literals, coeffs).build() */
  static LinearExpr weightedSum(Literal[] literals, long[] coeffs) {
    return newBuilder().addWeightedSum(literals, coeffs).build();
  }

  /** Shortcut for newBuilder().addWeightedSum(exprs, coeffs).build() */
  static LinearExpr weightedSum(LinearExpr[] exprs, long[] coeffs) {
    return newBuilder().addWeightedSum(exprs, coeffs).build();
  }

  static LinearExpr rebuildFromLinearExpressionProto(
      LinearExpressionProto proto, CpModelProto.Builder builder) {
    int numElements = proto.getVarsCount();
    if (numElements == 0) {
      return new ConstantExpression(proto.getOffset());
    } else if (numElements == 1) {
      return new AffineExpression(
          new IntVar(builder, proto.getVars(0)), proto.getCoeffs(0), proto.getOffset());
    } else {
      IntVar[] vars = new IntVar[numElements];
      long[] coeffs = new long[numElements];
      long offset = proto.getOffset();
      boolean allOnes = true;
      for (int i = 0; i < numElements; ++i) {
        vars[i] = new IntVar(builder, proto.getVars(i));
        coeffs[i] = proto.getCoeffs(i);
        if (coeffs[i] != 1) {
          allOnes = false;
        }
      }
      if (allOnes) {
        return new SumExpression(vars, offset);
      } else {
        return new WeightedSumExpression(vars, coeffs, offset);
      }
    }
  }
}
