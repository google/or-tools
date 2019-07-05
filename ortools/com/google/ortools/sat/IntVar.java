// Copyright 2010-2018 Google LLC
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
public final class IntVar implements Literal, LinearExpr {
  IntVar(CpModelProto.Builder builder, Domain domain, String name) {
    this.modelBuilder = builder;
    this.variableIndex = modelBuilder.getVariablesCount();
    this.varBuilder = modelBuilder.addVariablesBuilder();
    this.varBuilder.setName(name);
    for (long b : domain.flattenedIntervals()) {
      this.varBuilder.addDomain(b);
    }
    this.negation_ = null;
  }

  @Override
  public String toString() {
    return varBuilder.toString();
  }

  /** Returns the name of the variable given upon creation. */
  public String getName() {
    return varBuilder.getName();
  }

  /** Internal, returns the index of the variable in the underlying CpModelProto. */
  @Override
  public int getIndex() {
    return variableIndex;
  }

  /** Returns the variable protobuf builder. */
  public IntegerVariableProto.Builder getBuilder() {
    return varBuilder;
  }

  // LinearExpr interface.
  @Override
  public int numElements() {
    return 1;
  }

  @Override
  public IntVar getVariable(int index) {
    assert (index == 0);
    return this;
  }

  @Override
  public long getCoefficient(int index) {
    assert (index == 0);
    return 1;
  }

  /** Returns a short string describing the variable. */
  @Override
  public String getShortString() {
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

  /** Returns the negation of a boolean variable. */
  @Override
  public Literal not() {
    if (negation_ == null) {
      negation_ = new NotBooleanVariable(this);
    }
    return negation_;
  }

  private final CpModelProto.Builder modelBuilder;
  private final int variableIndex;
  private final IntegerVariableProto.Builder varBuilder;
  private NotBooleanVariable negation_;
}
