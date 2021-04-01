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

import com.google.ortools.sat.ConstraintProto;
import com.google.ortools.sat.CpModelProto;

/**
 * Wrapper around a ConstraintProto.
 *
 * <p>Constraints created by the CpModel class are automatically added to the model. One needs this
 * class to add an enforcement literal to a constraint.
 */
public class Constraint {
  public Constraint(CpModelProto.Builder builder) {
    this.constraintIndex = builder.getConstraintsCount();
    this.constraintBuilder = builder.addConstraintsBuilder();
  }

  /** Adds a literal to the constraint. */
  public void onlyEnforceIf(Literal lit) {
    constraintBuilder.addEnforcementLiteral(lit.getIndex());
  }

  /** Adds a list of literals to the constraint. */
  public void onlyEnforceIf(Literal[] lits) {
    for (Literal lit : lits) {
      constraintBuilder.addEnforcementLiteral(lit.getIndex());
    }
  }

  /** Returns the index of the constraint in the model. */
  public int getIndex() {
    return constraintIndex;
  }

  /** Returns the constraint builder. */
  public ConstraintProto.Builder getBuilder() {
    return constraintBuilder;
  }

  private final int constraintIndex;
  private final ConstraintProto.Builder constraintBuilder;
}
