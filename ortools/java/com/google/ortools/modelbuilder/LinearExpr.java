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

package com.google.ortools.modelbuilder;

/** A linear expression (sum (ai * xi) + b). It specifies methods to help parsing the expression. */
public interface LinearExpr extends LinearArgument {
  /** Returns the number of terms (excluding the constant one) in this expression. */
  int numElements();

  /** Returns the index of the ith variable. */
  int getVariableIndex(int index);

  /** Returns the ith coefficient. */
  double getCoefficient(int index);

  /** Returns the constant part of the expression. */
  double getOffset();

  /** Returns a builder */
  static LinearExprBuilder newBuilder() {
    return new LinearExprBuilder();
  }

  /** Shortcut for newBuilder().add(value).build() */
  static LinearExpr constant(double value) {
    return newBuilder().add(value).build();
  }

  /** Shortcut for newBuilder().addTerm(expr, coeff).build() */
  static LinearExpr term(LinearArgument expr, double coeff) {
    return newBuilder().addTerm(expr, coeff).build();
  }

  /** Shortcut for newBuilder().addTerm(expr, coeff).add(offset).build() */
  static LinearExpr affine(LinearArgument expr, double coeff, double offset) {
    return newBuilder().addTerm(expr, coeff).add(offset).build();
  }

  /** Shortcut for newBuilder().addSum(exprs).build() */
  static LinearExpr sum(LinearArgument[] exprs) {
    return newBuilder().addSum(exprs).build();
  }

  /** Shortcut for newBuilder().addWeightedSum(exprs, coeffs).build() */
  static LinearExpr weightedSum(LinearArgument[] exprs, double[] coeffs) {
    return newBuilder().addWeightedSum(exprs, coeffs).build();
  }
}
