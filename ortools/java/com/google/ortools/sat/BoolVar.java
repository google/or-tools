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
import com.google.ortools.util.Domain;

/** An Boolean variable. */
public final class BoolVar extends IntVar implements Literal {
  BoolVar(CpModelProto.Builder builder, Domain domain, String name) {
    super(builder, domain, name);
    this.negation = null;
  }

  BoolVar(CpModelProto.Builder builder, int index) {
    super(builder, index);
    this.negation = null;
  }

  /** Returns the negation of a boolean variable. */
  @Override
  public Literal not() {
    if (negation == null) {
      negation = new NotBoolVar(this);
    }
    return negation;
  }

  @Override
  public String toString() {
    if (varBuilder.getName().isEmpty()) {
      if (varBuilder.getDomainCount() == 2 && varBuilder.getDomain(0) == varBuilder.getDomain(1)) {
        if (varBuilder.getDomain(0) == 0) {
          return "false";
        } else {
          return "true";
        }
      } else {
        return String.format("boolvar_%d(%s)", getIndex(), displayBounds());
      }
    } else {
      if (varBuilder.getDomainCount() == 2 && varBuilder.getDomain(0) == varBuilder.getDomain(1)) {
        if (varBuilder.getDomain(0) == 0) {
          return String.format("%s(false)", varBuilder.getName());
        } else {
          return String.format("%s(true)", varBuilder.getName());
        }
      } else {
        return String.format("%s(%s)", getName(), displayBounds());
      }
    }
  }

  private NotBoolVar negation = null;
}
