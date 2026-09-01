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

import com.google.ortools.sat.CpModelProto;
import com.google.ortools.sat.IntegerVariableProto;
import com.google.ortools.util.Domain;

/** An integer variable. */
public class IntVar implements LinearArgument {
  IntVar(CpModelProto.Builder builder, Domain domain, String name) {
    this.modelBuilder = builder;
    this.variableIndex = modelBuilder.getVariablesCount();
    this.varBuilder = modelBuilder.addVariablesBuilder();
    this.varBuilder.setName(name);
    for (long b : domain.flattenedIntervals()) {
      this.varBuilder.addDomain(b);
    }
  }

  IntVar(CpModelProto.Builder builder, int index) {
    this.modelBuilder = builder;
    this.variableIndex = index;
    this.varBuilder = modelBuilder.getVariablesBuilder(index);
  }

  /** Returns the name of the variable given upon creation. */
  public String getName() {
    return varBuilder.getName();
  }

  /** Returns the index of the variable in the underlying CpModelProto. */
  public int getIndex() {
    return variableIndex;
  }

  /** Returns the variable protobuf builder. */
  public IntegerVariableProto.Builder getBuilder() {
    return varBuilder;
  }

  // LinearArgument interface
  @Override
  public LinearExpr build() {
    return new AffineExpression(variableIndex, 1, 0);
  }

  /** Returns the domain as a string without the enclosing []. */
  public String displayBounds() {
    String out = "";
    for (int i = 0; i < varBuilder.getDomainCount(); i += 2) {
      if (i != 0) {
        out += ", ";
      }
      if (varBuilder.getDomain(i) == varBuilder.getDomain(i + 1)) {
        out += String.format("%d", varBuilder.getDomain(i));
      } else {
        out += String.format("%d..%d", varBuilder.getDomain(i), varBuilder.getDomain(i + 1));
      }
    }
    return out;
  }

  /** Returns the domain of the variable. */
  public Domain getDomain() {
    return CpSatHelper.variableDomain(varBuilder.build());
  }

  @Override
  public String toString() {
    if (varBuilder.getName().isEmpty()) {
      if (varBuilder.getDomainCount() == 2 && varBuilder.getDomain(0) == varBuilder.getDomain(1)) {
        return String.format("%d", varBuilder.getDomain(0));
      } else {
        return String.format("var_%d(%s)", getIndex(), displayBounds());
      }
    } else {
      return String.format("%s(%s)", getName(), displayBounds());
    }
  }

  protected final CpModelProto.Builder modelBuilder;
  protected final int variableIndex;
  protected final IntegerVariableProto.Builder varBuilder;
}
