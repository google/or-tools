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
import com.google.ortools.util.Domain;

/** An Boolean variable. */
public final class BoolVar extends IntVar implements Literal {
  BoolVar(CpModelProto.Builder builder, Domain domain, String name) {
    super(builder, domain, name);
    this.negation_ = null;
  }

  BoolVar(CpModelProto.Builder builder, int index) {
    super(builder, index);
    this.negation_ = null;
  }

  /** Returns the negation of a boolean variable. */
  @Override
  public Literal not() {
    if (negation_ == null) {
      negation_ = new NotBooleanVariable(this);
    }
    return negation_;
  }

  @Override
  public BoolVar getBoolVar() {
    return this;
  }

  @Override
  public boolean negated() {
    return false;
  }

  private NotBooleanVariable negation_ = null;
}
