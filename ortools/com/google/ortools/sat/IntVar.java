// Copyright 2010-2017 Google
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

/** An integer variable. */
public class IntVar implements ILiteral {
  IntVar(CpModelProto.Builder builder, long lb, long ub, String name) {
    this.builder_ = builder;
    this.index_ = builder_.getVariablesCount();
    this.var_ = builder_.addVariablesBuilder();
    this.var_.setName(name);
    this.var_.addDomain(lb);
    this.var_.addDomain(ub);
    this.negation_ = null;
  }

  IntVar(CpModelProto.Builder builder, long[] bounds, String name) {
    this.builder_ = builder;
    this.index_ = builder_.getVariablesCount();
    this.var_ = builder_.addVariablesBuilder();
    this.var_.setName(name);
    for (long b : bounds) {
      this.var_.addDomain(b);
    }
    this.negation_ = null;
  }

  @Override
  public String toString() {
    return var_.toString();
  }

  /** Internal, return the index of the variable in the underlying CpModelProto. */
  public int getIndex() {
    return index_;
  }

  /** Returns the name of the variable given upon creation. */
  public String getName() {
    return var_.getName();
  }

  // Returns a short string describing the variable.
  public String shortString() {
    if (var_.getName().isEmpty()) {
      return toString();
    } else {
      return var_.getName();
    }
  }

  /** Returns the negation of a boolean variable. */
  public ILiteral not() {
    if (negation_ == null) {
      negation_ = new NotBooleanVariable(this);
    }
    return negation_;
  }

  private CpModelProto.Builder builder_;
  private int index_;
  private IntegerVariableProto.Builder var_;
  private NotBooleanVariable negation_;
}
