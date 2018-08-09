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

import com.google.ortools.sat.ConstraintProto;
import com.google.ortools.sat.CpModelProto;

/**
 * Wrapper around a ConstraintProto.
 *
 * <p>Constraint create by the CpModel class are automatically added to the model. One need this
 * class to adds an enforcement literal to a constraint.
 */
public class Constraint {
  public Constraint(CpModelProto.Builder builder) {
    this.index_ = builder.getConstraintsCount();
    this.constraint_ = builder.addConstraintsBuilder();
  }

  /** Adds a literal to the constraint. */
  public void onlyEnforceIf(ILiteral lit) {
    constraint_.addEnforcementLiteral(lit.getIndex());
  }

  int getIndex() {
    return index_;
  }

  ConstraintProto.Builder builder() {
    return constraint_;
  }

  private int index_;
  private ConstraintProto.Builder constraint_;
}
