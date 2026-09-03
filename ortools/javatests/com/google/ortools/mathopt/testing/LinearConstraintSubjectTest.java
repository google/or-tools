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

import static com.google.common.truth.ExpectFailure.expectFailureAbout;
import static com.google.common.truth.Truth.assertThat;
import static com.google.ortools.mathopt.testing.LinearConstraintSubject.assertThat;

import com.google.common.collect.ImmutableMap;
import com.google.common.truth.ExpectFailure.SimpleSubjectBuilderCallback;
import com.google.ortools.mathopt.LinearConstraint;
import com.google.ortools.mathopt.Model;
import com.google.ortools.mathopt.Variable;
import com.google.ortools.mathopt.testing.LinearConstraintSubject.LinearConstraintMatcher;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class LinearConstraintSubjectTest {
  private Model model;
  private Variable x;
  private Variable y;

  @BeforeEach
  public void setUp() {
    model = new Model();
    x = model.addVariable("x");
    y = model.addVariable("y");
  }

  @Test
  public void assertThatHazzers_correctData_ok() {
    LinearConstraint c =
        model.addLinearConstraint("c").setLowerBound(-4).setUpperBound(5).setTerm(x, 2.0);
    assertThat(c).hasLb(-4);
    assertThat(c).hasUb(5);
    assertThat(c).hasTerms(ImmutableMap.of(x, 2.0));
    assertThat(c).hasName("c");
  }

  @Test
  public void assertThatLb_badData_fails() {
    LinearConstraint c = model.addLinearConstraint("c").setLowerBound(-4);
    AssertionError error = expectFailure(whenTesting -> whenTesting.that(c).hasLb(-3));
    assertThat(error).hasMessageThat().contains("getLowerBound");
  }

  @Test
  public void assertThatLb_null_fails() {
    AssertionError error = expectFailure(whenTesting -> whenTesting.that(null).hasLb(-3));
    assertThat(error).hasMessageThat().contains("getLowerBound");
  }

  @Test
  public void assertThatUb_badData_fails() {
    LinearConstraint c = model.addLinearConstraint("c").setUpperBound(5);
    AssertionError error = expectFailure(whenTesting -> whenTesting.that(c).hasUb(12));
    assertThat(error).hasMessageThat().contains("getUpperBound");
  }

  @Test
  public void assertThatUb_null_fails() {
    AssertionError error = expectFailure(whenTesting -> whenTesting.that(null).hasUb(12));
    assertThat(error).hasMessageThat().contains("getUpperBound");
  }

  @Test
  public void assertThatName_badName_fails() {
    LinearConstraint c = model.addLinearConstraint("c");
    AssertionError error = expectFailure(whenTesting -> whenTesting.that(c).hasName("dog"));
    assertThat(error).hasMessageThat().contains("getName");
  }

  @Test
  public void assertThatName_null_fails() {
    AssertionError error = expectFailure(whenTesting -> whenTesting.that(null).hasName("dog"));
    assertThat(error).hasMessageThat().contains("getName");
  }

  @Test
  public void assertThatTerms_badValues_fails() {
    LinearConstraint c = model.addLinearConstraint("c").setTerm(x, 3.0);
    AssertionError error =
        expectFailure(whenTesting -> whenTesting.that(c).hasTerms(ImmutableMap.of(y, 4.0)));
    assertThat(error).hasMessageThat().contains("getUnmodifiableTerms");
  }

  @Test
  public void assertThatTerms_null_fails() {
    AssertionError error =
        expectFailure(whenTesting -> whenTesting.that(null).hasTerms(ImmutableMap.of(y, 4.0)));
    assertThat(error).hasMessageThat().contains("non-null");
  }

  @Test
  public void assertThatMatcher_correctData_ok() {
    LinearConstraint c =
        model.addLinearConstraint("c").setLowerBound(-4).setUpperBound(5).setTerm(x, 2.0);
    assertThat(c).matches(
        new LinearConstraintMatcher().setLb(-4).setUb(5).setTerm(x, 2.0).setName("c"));
  }

  @Test
  public void assertThatMatcher_badLb_fails() {
    LinearConstraint c = model.addLinearConstraint("").setLowerBound(-4);
    AssertionError error = expectFailure(
        whenTesting -> whenTesting.that(c).matches(new LinearConstraintMatcher().setLb(-5)));
    assertThat(error).hasMessageThat().contains("getLowerBound");
  }

  @Test
  public void assertThatMatcher_badUb_fails() {
    LinearConstraint c = model.addLinearConstraint("").setUpperBound(5);
    AssertionError error = expectFailure(
        whenTesting -> whenTesting.that(c).matches(new LinearConstraintMatcher().setUb(12)));
    assertThat(error).hasMessageThat().contains("getUpperBound");
  }

  @Test
  public void assertThatMatcher_badName_fails() {
    LinearConstraint c = model.addLinearConstraint("c");
    AssertionError error = expectFailure(
        whenTesting -> whenTesting.that(c).matches(new LinearConstraintMatcher().setName("dog")));
    assertThat(error).hasMessageThat().contains("getName");
  }

  @Test
  public void assertThatMatcher_badTerms_fails() {
    LinearConstraint c = model.addLinearConstraint("").setTerm(x, 3.0);
    AssertionError error = expectFailure(whenTesting
        -> whenTesting.that(c).matches(new LinearConstraintMatcher().setTerms(x, -5, y, 10)));
    assertThat(error).hasMessageThat().contains("getUnmodifiableTerms");
  }

  private static AssertionError expectFailure(
      SimpleSubjectBuilderCallback<LinearConstraintSubject, LinearConstraint> callback) {
    return expectFailureAbout(LinearConstraintSubject.linearConstraints(), callback);
  }
}
