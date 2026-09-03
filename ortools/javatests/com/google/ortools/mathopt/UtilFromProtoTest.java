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

import com.google.ortools.mathopt.SparseDoubleVectorProto;
import com.google.ortools.mathopt.SparseInt32VectorProto;
import org.junit.jupiter.api.Test;

public final class UtilFromProtoTest {
  @Test
  public void variableValuesFromProto_validConversion() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addVariable("y");
    var proto = SparseDoubleVectorProto.newBuilder()
                    .addIds(x.getId())
                    .addValues(10.0)
                    .addIds(y.getId())
                    .addValues(20.0)
                    .build();

    assertThat(UtilFromProto.variableValuesFromProto(model, proto))
        .containsExactly(x, 10.0, y, 20.0);
  }

  @Test
  public void variableValuesFromProto_protoInvalid_throws() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addVariable("y");
    var proto = SparseDoubleVectorProto.newBuilder()
                    .addIds(x.getId())
                    .addValues(10.0)
                    .addIds(y.getId())
                    .build();

    var error = assertThrows(
        IllegalArgumentException.class, () -> UtilFromProto.variableValuesFromProto(model, proto));
    assertThat(error).hasMessageThat().contains("must be the same");
  }

  @Test
  public void variableValuesFromProto_absentVariableInModel_throws() {
    var model1 = new Model("test_model");
    var model2 = new Model("test_model_2");
    Variable x = model1.addBinaryVariable("x");
    Variable unused = model2.addBinaryVariable("y");
    Variable z = model2.addVariable("z");
    var proto = SparseDoubleVectorProto.newBuilder()
                    .addIds(x.getId())
                    .addValues(10.0)
                    .addIds(z.getId())
                    .addValues(20.0)
                    .build();

    var error = assertThrows(
        IllegalArgumentException.class, () -> UtilFromProto.variableValuesFromProto(model1, proto));
    assertThat(error).hasMessageThat().contains("no variable");
  }

  @Test
  public void variableInt32ValuesFromProto_validConversion() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addVariable("y");
    var proto = SparseInt32VectorProto.newBuilder()
                    .addIds(x.getId())
                    .addValues(10)
                    .addIds(y.getId())
                    .addValues(20)
                    .build();

    assertThat(UtilFromProto.variableInt32ValuesFromProto(model, proto))
        .containsExactly(x, 10, y, 20);
  }

  @Test
  public void variableInt32ValuesFromProto_protoInvalid_throws() {
    var model = new Model("test_model");
    Variable x = model.addBinaryVariable("x");
    Variable y = model.addVariable("y");
    var proto = SparseInt32VectorProto.newBuilder()
                    .addIds(x.getId())
                    .addValues(10)
                    .addIds(y.getId())
                    .build();

    var error = assertThrows(IllegalArgumentException.class,
        () -> UtilFromProto.variableInt32ValuesFromProto(model, proto));
    assertThat(error).hasMessageThat().contains("must be the same");
  }

  @Test
  public void variableInt32ValuesFromProto_absentVariableInModel_throws() {
    var model1 = new Model("test_model");
    var model2 = new Model("test_model_2");
    Variable x = model1.addBinaryVariable("x");
    Variable unused = model2.addBinaryVariable("y");
    Variable z = model2.addVariable("z");
    var proto = SparseInt32VectorProto.newBuilder()
                    .addIds(x.getId())
                    .addValues(10)
                    .addIds(z.getId())
                    .addValues(20)
                    .build();

    var error = assertThrows(IllegalArgumentException.class,
        () -> UtilFromProto.variableInt32ValuesFromProto(model1, proto));
    assertThat(error).hasMessageThat().contains("no variable");
  }

  @Test
  public void linearConstraintFromProto_validConversion() {
    Model model = new Model("test_model");
    LinearConstraint l1 = model.addLinearConstraint("lc_1");
    LinearConstraint l2 = model.addLinearConstraint("lc_2");
    SparseDoubleVectorProto proto = SparseDoubleVectorProto.newBuilder()
                                        .addIds(l1.getId())
                                        .addValues(10.0)
                                        .addIds(l2.getId())
                                        .addValues(20.0)
                                        .build();

    assertThat(UtilFromProto.linearConstraintValuesFromProto(model, proto))
        .containsExactly(l1, 10.0, l2, 20.0);
  }

  @Test
  public void linearConstraintFromProto_protoInvalid_throws() {
    Model model = new Model("test_model");
    LinearConstraint l1 = model.addLinearConstraint("lc_1");
    LinearConstraint l2 = model.addLinearConstraint("lc_2");
    SparseDoubleVectorProto proto = SparseDoubleVectorProto.newBuilder()
                                        .addIds(l1.getId())
                                        .addValues(10.0)
                                        .addIds(l2.getId())
                                        .build();

    var error = assertThrows(IllegalArgumentException.class,
        () -> UtilFromProto.linearConstraintValuesFromProto(model, proto));
    assertThat(error).hasMessageThat().contains("must be the same");
  }

  @Test
  public void linearConstraintFromProto_absentConstraintInModel_throws() {
    Model model1 = new Model("test_model");
    Model model2 = new Model("test_model_2");
    LinearConstraint l1 = model1.addLinearConstraint("lc_1");
    LinearConstraint unused = model2.addLinearConstraint("lc_2");
    LinearConstraint l3 = model2.addLinearConstraint("lc_3");
    SparseDoubleVectorProto proto = SparseDoubleVectorProto.newBuilder()
                                        .addIds(l1.getId())
                                        .addValues(10.0)
                                        .addIds(l3.getId())
                                        .addValues(20.0)
                                        .build();

    var error = assertThrows(IllegalArgumentException.class,
        () -> UtilFromProto.linearConstraintValuesFromProto(model1, proto));
    assertThat(error).hasMessageThat().contains("no linear constraint");
  }
}
