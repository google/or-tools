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

package com.google.ortools.mathopt;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.jupiter.api.Assertions.assertThrows;

import com.google.common.collect.ImmutableList;
import org.junit.jupiter.api.Test;

public final class ExpressionsTest {
  @Test
  public void linExpr_noArgs_emptyExpr() {
    HashLinearExpression expression = Expressions.linExpr();
    assertThat(expression.offset()).isEqualTo(0.0);
    assertThat(expression.getTerms()).isEmpty();
  }

  @Test
  public void linExpr_fromNumber_isNumber() {
    HashLinearExpression expression = Expressions.linExpr(4.0);
    assertThat(expression.offset()).isEqualTo(4.0);
    assertThat(expression.getTerms()).isEmpty();
  }

  @Test
  public void linExpr_fromCopy_isCopy() {
    var model = new Model();
    Variable x = model.addVariable();
    HashLinearExpression init = new HashLinearExpression().add(4.0).add(2.0, x);
    HashLinearExpression copy = Expressions.linExpr(init);
    assertThat(copy).isNotSameInstanceAs(init);
    assertThat(copy.offset()).isEqualTo(4.0);
    assertThat(copy.getTerms()).containsExactly(x, 2.0);
  }

  @Test
  public void sum_onList_hasSum() {
    var model = new Model();
    Variable x = model.addVariable();
    var summand =
        ImmutableList.of(x, Expressions.linExpr(2.0).add(4.0, x), Expressions.linExpr(-3.0));
    HashLinearExpression sum = Expressions.sum(summand);
    assertThat(sum.offset()).isEqualTo(-1.0);
    assertThat(sum.getTerms()).containsExactly(x, 5.0);
  }

  @Test
  public void sum_onEmptyList_isZero() {
    HashLinearExpression sum = Expressions.sum(ImmutableList.of());
    assertThat(sum.offset()).isEqualTo(0.0);
    assertThat(sum.getTerms()).isEmpty();
  }

  @Test
  public void sumStream_onList_hasSum() {
    var model = new Model();
    Variable x = model.addVariable();
    var summand =
        ImmutableList.of(x, Expressions.linExpr(2.0).add(4.0, x), Expressions.linExpr(-3.0));
    HashLinearExpression sum = summand.stream().collect(Expressions.sumCollector());
    assertThat(sum.offset()).isEqualTo(-1.0);
    assertThat(sum.getTerms()).containsExactly(x, 5.0);
  }

  @Test
  public void innerProduct_parallelLists_hasInnerProduct() {
    var model = new Model();
    Variable x = model.addVariable();
    // [3, 2] * [x, (3x - 1)] = -3x - 2
    HashLinearExpression innerProduct = Expressions.innerProduct(
        ImmutableList.of(3.0, 2.0), ImmutableList.of(x, Expressions.linExpr(1.0).subtract(3.0, x)));
    assertThat(innerProduct.offset()).isEqualTo(2.0);
    assertThat(innerProduct.getTerms()).containsExactly(x, -3.0);
  }

  @Test
  public void innerProduct_emptyLists_zero() {
    HashLinearExpression innerProduct =
        Expressions.innerProduct(ImmutableList.of(), ImmutableList.of());
    assertThat(innerProduct.offset()).isEqualTo(0.0);
    assertThat(innerProduct.getTerms()).isEmpty();
  }

  @Test
  public void innerProduct_unevenLists_throws() {
    assertThrows(IllegalArgumentException.class,
        () -> Expressions.innerProduct(ImmutableList.of(3.0), ImmutableList.of()));
  }

  @Test
  public void product_times2_isDouble() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    HashLinearExpression e = Expressions.linExpr(3.0).add(4.0, x).add(5.0, y);

    HashLinearExpression twoE = Expressions.product(2.0, e);

    assertThat(twoE.offset()).isEqualTo(6.0);
    assertThat(twoE.getTerms()).containsExactly(x, 8.0, y, 10.0);
    // Also check that the input was not modified.
    assertThat(e.offset()).isEqualTo(3.0);
    assertThat(e.getTerms()).containsExactly(x, 4.0, y, 5.0);
  }

  @Test
  public void quotient_div2_isHalf() {
    var model = new Model();
    Variable x = model.addVariable();
    Variable y = model.addVariable();
    HashLinearExpression e = Expressions.linExpr(6.0).add(8.0, x).add(10.0, y);

    HashLinearExpression halfE = Expressions.quotient(e, 2.0);

    assertThat(halfE.offset()).isEqualTo(3.0);
    assertThat(halfE.getTerms()).containsExactly(x, 4.0, y, 5.0);
    // Also check that the input was not modified.
    assertThat(e.offset()).isEqualTo(6.0);
    assertThat(e.getTerms()).containsExactly(x, 8.0, y, 10.0);
  }
}
