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
import static java.util.Arrays.asList;
import static org.junit.jupiter.api.Assertions.assertThrows;

import com.google.common.collect.ImmutableList;
import java.util.Arrays;
import java.util.Map;
import org.junit.jupiter.api.Test;

public final class HashLinearExpressionTest {
  @Test
  public void newExpression_empty_gettersOk() {
    var model = new Model();
    Variable x = model.addBinaryVariable();

    var expr = new HashLinearExpression();

    assertThat(expr.offset()).isEqualTo(0.0);
    assertThat(expr.unmodifiableTerms()).isEmpty();
    assertThat(expr.getTerms()).isEmpty();
    assertThat(expr.getTerm(x)).isEqualTo(0.0);
  }

  @Test
  public void add_setTerms_gettersOk() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();

    var expr = new HashLinearExpression(8.0).add(x).add(3.0, y);

    assertThat(expr.offset()).isEqualTo(8.0);
    assertThat(expr.unmodifiableTerms()).containsExactly(Map.entry(x, 1.0), Map.entry(y, 3.0));
    assertThat(expr.getTerms()).containsExactly(x, 1.0, y, 3.0);
    assertThat(expr.getTerm(x)).isEqualTo(1.0);
    assertThat(expr.getTerm(y)).isEqualTo(3.0);
  }

  @Test
  public void add_addConstant_gettersOk() {
    var expr = new HashLinearExpression().add(3.0);

    assertThat(expr.offset()).isEqualTo(3.0);
    assertThat(expr.unmodifiableTerms()).isEmpty();
  }

  @Test
  public void add_addVariable_gettersOk() {
    var model = new Model();
    Variable x = model.addBinaryVariable();

    var expr = new HashLinearExpression().add(x);

    assertThat(expr.offset()).isEqualTo(0.0);
    assertThat(expr.getTerms()).containsExactly(x, 1.0);
  }

  @Test
  public void add_addScaledVariable_gettersOk() {
    var model = new Model();
    Variable x = model.addBinaryVariable();

    var expr = new HashLinearExpression().add(3.0, x);

    assertThat(expr.offset()).isEqualTo(0.0);
    assertThat(expr.getTerms()).containsExactly(x, 3.0);
  }

  @Test
  public void add_addLinearExpr_gettersOk() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    var toAdd = new HashLinearExpression().add(3.0).add(x);

    var expr = new HashLinearExpression().add(toAdd);

    assertThat(expr.offset()).isEqualTo(3.0);
    assertThat(expr.getTerms()).containsExactly(x, 1.0);
  }

  @Test
  public void add_addScaledLinearExpr_gettersOk() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    var toAdd = new HashLinearExpression().add(3.0).add(x);

    var expr = new HashLinearExpression().add(4.0, toAdd);

    assertThat(expr.offset()).isEqualTo(12.0);
    assertThat(expr.getTerms()).containsExactly(x, 4.0);
  }

  @Test
  public void add_addSameTerm_gettersOk() {
    var model = new Model();
    Variable x = model.addBinaryVariable();

    var expr = new HashLinearExpression(x).add(x).add(3.0, x);

    assertThat(expr.offset()).isEqualTo(0.0);
    assertThat(expr.unmodifiableTerms()).containsExactly(Map.entry(x, 5.0));
    assertThat(expr.getTerms()).containsExactly(x, 5.0);
    assertThat(expr.getTerm(x)).isEqualTo(5.0);
  }

  @Test
  public void sum_emptyList_isZero() {
    HashLinearExpression expr = HashLinearExpression.sum(ImmutableList.of());

    assertThat(expr.offset()).isEqualTo(0.0);
    assertThat(expr.getTerms()).isEmpty();
  }

  @Test
  public void sum_variableList_hasVariables() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();
    HashLinearExpression expr = HashLinearExpression.sum(asList(x, y, x));

    assertThat(expr.offset()).isEqualTo(0.0);
    assertThat(expr.getTerms()).containsExactly(x, 2.0, y, 1.0);
  }

  @Test
  public void sum_linearExpressionList_hasValues() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();

    HashLinearExpression expr = HashLinearExpression.sum(
        asList(new HashLinearExpression(4.0), x, new HashLinearExpression(x).add(y)));

    assertThat(expr.offset()).isEqualTo(4.0);
    assertThat(expr.getTerms()).containsExactly(x, 2.0, y, 1.0);
  }

  @Test
  public void innerProduct_emptyList_isZero() {
    HashLinearExpression expr =
        HashLinearExpression.innerProduct(ImmutableList.of(), ImmutableList.of());

    assertThat(expr.offset()).isEqualTo(0.0);
    assertThat(expr.unmodifiableTerms()).isEmpty();
  }

  @Test
  public void innerProduct_unevenInputLists_throws() {
    var model = new Model();
    Variable x = model.addBinaryVariable();

    var error = assertThrows(IllegalArgumentException.class,
        () -> HashLinearExpression.innerProduct(ImmutableList.of(), asList(x)));

    assertThat(error).hasMessageThat().contains("to be equal");
  }

  @Test
  public void unmodifiableTerms_update_throws() {
    var model = new Model();
    var unused = model.addBinaryVariable();
    var expr = new HashLinearExpression();

    for (Map.Entry<Variable, Double> entry : expr.unmodifiableTerms()) {
      assertThrows(UnsupportedOperationException.class, () -> entry.setValue(3.0));
    }
  }

  @Test
  public void setTerm_notPresent_added() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    var expr = new HashLinearExpression();

    expr.setTerm(x, 2.0);

    assertThat(expr.getTerms()).containsExactly(x, 2.0);
  }

  @Test
  public void setTerm_present_overwritten() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    var expr = new HashLinearExpression(x);

    expr.setTerm(x, 3.0);

    assertThat(expr.getTerms()).containsExactly(x, 3.0);
  }

  @Test
  public void setTerm_wasPresentToZero_noEffect() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    var expr = new HashLinearExpression(x);

    expr.setTerm(x, 0.0);

    assertThat(expr.getTerms()).isEmpty();
  }

  @Test
  public void setTerm_wasNotPresentToZero_noEffect() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    var expr = new HashLinearExpression();

    expr.setTerm(x, 0.0);

    assertThat(expr.getTerms()).isEmpty();
  }

  @Test
  public void negate_isNegated() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();
    var expr = new HashLinearExpression(9.0).add(x).add(-3.0, y);

    assertThat(expr.negate()).isSameInstanceAs(expr);

    assertThat(expr.offset()).isEqualTo(-9.0);
    assertThat(expr.getTerms()).containsExactly(x, -1.0, y, 3.0);
  }

  @Test
  public void scale_isScaled() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();
    var expr = new HashLinearExpression(9.0).add(x).add(-3.0, y);

    assertThat(expr.scale(2.0)).isSameInstanceAs(expr);

    assertThat(expr.offset()).isEqualTo(18.0);
    assertThat(expr.getTerms()).containsExactly(x, 2.0, y, -6.0);
  }

  @Test
  public void divide_byNonzero_isDivided() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();
    var expr = new HashLinearExpression(8.0).add(2.0, x).add(-4.0, y);

    assertThat(expr.divide(2.0)).isSameInstanceAs(expr);

    assertThat(expr.offset()).isEqualTo(4.0);
    assertThat(expr.getTerms()).containsExactly(x, 1.0, y, -2.0);
  }

  @Test
  public void divide_byZero_throws() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    var expr = new HashLinearExpression(8.0).add(2.0, x);

    var error = assertThrows(IllegalArgumentException.class, () -> expr.divide(0.0));

    assertThat(error).hasMessageThat().contains("nonzero");
  }

  @Test
  public void subtract_subtractVariable_gettersOk() {
    var model = new Model();
    Variable x = model.addBinaryVariable();

    var expr = new HashLinearExpression().subtract(x);

    assertThat(expr.offset()).isEqualTo(0.0);
    assertThat(expr.getTerms()).containsExactly(x, -1.0);
  }

  @Test
  public void subtract_subtractScaledVariable_gettersOk() {
    var model = new Model();
    Variable x = model.addBinaryVariable();

    var expr = new HashLinearExpression().subtract(3.0, x);

    assertThat(expr.offset()).isEqualTo(0.0);
    assertThat(expr.getTerms()).containsExactly(x, -3.0);
  }

  @Test
  public void subtract_expression_gettersOk() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();
    var exprA = new HashLinearExpression(8.0).add(2.0, x).add(-4.0, y);
    var exprB = new HashLinearExpression(4.0).add(-1.0, x).add(-2.0, y);

    assertThat(exprA.subtract(exprB)).isSameInstanceAs(exprA);

    assertThat(exprA.offset()).isEqualTo(4.0);
    assertThat(exprA.getTerms()).containsExactly(x, 3.0, y, -2.0);
    assertThat(exprB.offset()).isEqualTo(4.0);
    assertThat(exprB.getTerms()).containsExactly(x, -1.0, y, -2.0);
  }

  @Test
  public void subtract_scaledExpression_gettersOk() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    var exprA = new HashLinearExpression(8.0).add(2.0, x);
    var exprB = new HashLinearExpression(4.0).add(-1.0, x);

    assertThat(exprA.subtract(3.0, exprB)).isSameInstanceAs(exprA);

    assertThat(exprA.offset()).isEqualTo(-4.0);
    assertThat(exprA.getTerms()).containsExactly(x, 5.0);
    assertThat(exprB.offset()).isEqualTo(4.0);
    assertThat(exprB.getTerms()).containsExactly(x, -1.0);
  }

  @Test
  public void sum_variableListToLinearExpression_sumIsCorrect() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();

    HashLinearExpression sum = HashLinearExpression.sum(ImmutableList.of(x, y, x));

    assertThat(sum.offset()).isEqualTo(0.0);
    assertThat(sum.getTerms()).containsExactly(x, 2.0, y, 1.0);
  }

  @Test
  public void innerProduct_variablesAndDoublesToLinearExpression_sumIsCorrect() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();

    var result = HashLinearExpression.innerProduct(
        ImmutableList.of(1.0, 2.0, 10.0), ImmutableList.of(x, y, x));

    assertThat(result.offset()).isEqualTo(0.0);
    assertThat(result.getTerms()).containsExactly(x, 11.0, y, 2.0);
  }

  @Test
  public void innerProduct_inputSizeUneven_throws() {
    var error = assertThrows(IllegalArgumentException.class,
        () -> HashLinearExpression.innerProduct(Arrays.asList(2.0), ImmutableList.<Variable>of()));
    assertThat(error).hasMessageThat().contains("to be equal");
  }

  @Test
  public void sumCollector_variableListToLinearExpression_sumIsCorrect() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();

    HashLinearExpression sum =
        ImmutableList.of(x, y, x).stream().collect(HashLinearExpression.sumCollector());

    assertThat(sum.offset()).isEqualTo(0.0);
    assertThat(sum.getTerms()).containsExactly(x, 2.0, y, 1.0);
  }

  @Test
  public void sumCollector_linearExpressionListToLinearExpression_sumIsCorrect() {
    var model = new Model();
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();

    HashLinearExpression sum = HashLinearExpression.sum(ImmutableList.of(
        new HashLinearExpression(x).add(2, y).add(4.0), y, new HashLinearExpression(7.0)));

    assertThat(sum.offset()).isEqualTo(11.0);
    assertThat(sum.getTerms()).containsExactly(x, 1.0, y, 3.0);
  }

  @Test
  public void sumCollector_parallelSum_sumIsCorrect() {
    // The combiner is only used if the stream is executed in parallel.
    var model = new Model();
    Variable x = model.addBinaryVariable();
    Variable y = model.addBinaryVariable();
    Variable z = model.addBinaryVariable();

    HashLinearExpression sum = ImmutableList.of(x, y, x, z, z, x, x)
                                   .parallelStream()
                                   .collect(HashLinearExpression.sumCollector());

    assertThat(sum.offset()).isEqualTo(0.0);
    assertThat(sum.getTerms()).containsExactly(x, 4.0, y, 1.0, z, 2.0);
  }

  @Test
  public void sumCollector_finish_returnsIdentity() {
    // No way to invoke finish via stream operations because of the characteristics.
    var input = new HashLinearExpression(3.0);
    var output = HashLinearExpression.sumCollector().finisher().apply(input);
    assertThat(output).isSameInstanceAs(input);
  }
}
