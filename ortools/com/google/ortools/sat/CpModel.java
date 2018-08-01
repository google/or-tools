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

public class CpModel {
  public CpModel() {
    builder_ = CpModelProto.newBuilder();
  }

  // Integer variables.

  public IntVar newIntVar(long lb, long ub, String name) {
    return new IntVar(builder_, lb, ub, name);
  }

  public IntVar newBoolVar(String name) {
    return new IntVar(builder_, 0, 1, name);
  }

  // Boolean Constraints.

  public void addBoolOr(ILiteral[] literals) {
    Constraint ct = new Constraint(builder_);
    BoolArgumentProto.Builder bool_or = ct.builder().getBoolOrBuilder();
    for (ILiteral lit : literals) {
      bool_or.addLiterals(lit.getIndex());
    }
  }

  // Getters.

  public CpModelProto Model() { return builder_.build(); }
  public int Negated(int index) { return -index - 1; }

  private CpModelProto.Builder builder_;
}
