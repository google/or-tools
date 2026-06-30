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

package com.google.ortools.mathopt.testing;

import static com.google.common.truth.Truth.assertAbout;

import com.google.common.truth.Fact;
import com.google.common.truth.FailureMetadata;
import com.google.common.truth.Subject;
import com.google.ortools.mathopt.LinearConstraint;
import com.google.ortools.mathopt.Variable;
import java.util.HashMap;
import java.util.Map;
import org.jspecify.annotations.Nullable;

/** Provides Truth assertions for {@link LinearConstraint}. */
public final class LinearConstraintSubject extends Subject {
  private final @Nullable LinearConstraint actual;

  private LinearConstraintSubject(FailureMetadata metadata, @Nullable LinearConstraint actual) {
    super(metadata, actual);
    this.actual = actual;
  }

  public static Factory<LinearConstraintSubject, LinearConstraint> linearConstraints() {
    return LinearConstraintSubject::new;
  }

  public static LinearConstraintSubject assertThat(@Nullable LinearConstraint actual) {
    return assertAbout(linearConstraints()).that(actual);
  }

  public void hasLb(double lb) {
    check("getLowerBound()").that(actual == null ? null : actual.getLowerBound()).isEqualTo(lb);
  }

  public void hasUb(double ub) {
    check("getUpperBound()").that(actual == null ? null : actual.getUpperBound()).isEqualTo(ub);
  }

  public void hasTerms(Map<Variable, Double> terms) {
    // containsExactlyEntriesIn throws a null pointer exception if actual is null.
    if (actual == null) {
      failWithActual(Fact.simpleFact("expected actual to be non-null, cannot assert on terms"));
      return;
    }
    check("getUnmodifiableTerms()")
        .that(actual.getUnmodifiableTerms())
        .containsExactlyEntriesIn(terms);
  }

  public void hasName(String name) {
    check("getName()").that(actual == null ? null : actual.getName()).isEqualTo(name);
  }

  public void matches(LinearConstraintMatcher matcher) {
    hasLb(matcher.lowerBound);
    hasUb(matcher.upperBound);
    hasTerms(matcher.terms);
    hasName(matcher.name);
  }

  /**
   * Specifies a subset of the fields of {@link LinearConstraint} to match against an actual {@link
   * LinearConstraint} using {@link #matches(LinearConstraintMatcher)}.
   */
  public static final class LinearConstraintMatcher {
    private double lowerBound = Double.NEGATIVE_INFINITY;
    private double upperBound = Double.POSITIVE_INFINITY;

    private final Map<Variable, Double> terms = new HashMap<>();

    private String name = "";

    public LinearConstraintMatcher setLb(double lowerBound) {
      this.lowerBound = lowerBound;
      return this;
    }

    public LinearConstraintMatcher setUb(double upperBound) {
      this.upperBound = upperBound;
      return this;
    }

    public LinearConstraintMatcher setName(String name) {
      this.name = name;
      return this;
    }

    LinearConstraintMatcher setTerm(Variable v, double coefficient) {
      terms.put(v, coefficient);
      return this;
    }

    public LinearConstraintMatcher setTerms(Variable v1, double c1, Variable v2, double c2) {
      terms.put(v1, c1);
      terms.put(v2, c2);
      return this;
    }
  }
}
