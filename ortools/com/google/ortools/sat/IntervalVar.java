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

import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpModelProto;
import com.google.ortools.sat.ILiteral;
import com.google.ortools.sat.IntegerVariableProto;
import com.google.ortools.sat.NotBooleanVariable;

public class IntervalVar{
  public IntervalVar(CpModelProto.Builder builder,
                     int start_index, int size_index, int end_index,
                     String name) {
    this.builder_ = builder;
    this.index_ = builder_.getConstraintsCount();
    ConstraintProto.Builder ct = builder_.addConstraintsBuilder();
    ct.setName(name);
    this.var_ = ct.getIntervalBuilder();
    this.var_.setStart(start_index);
    this.var_.setSize(size_index);
    this.var_.setEnd(end_index);
  }


  public IntervalVar(CpModelProto.Builder builder,
                     int start_index, int size_index, int end_index,
                     int is_present_index, String name) {
    this.builder_ = builder;
    this.index_ = builder_.getConstraintsCount();
    ConstraintProto.Builder ct = builder_.addConstraintsBuilder();
    ct.setName(name);
    ct.addEnforcementLiteral(is_present_index);
    this.var_ = ct.getIntervalBuilder();
    this.var_.setStart(start_index);
    this.var_.setSize(size_index);
    this.var_.setEnd(end_index);
  }

  @Override
  public String toString() {
    if (getName().isEmpty()) {
      return "Interval(" + var_.toString() + ")";
    } else {
      return getName() + "(" + var_.toString() + ")";
    }
  }

  public int getIndex() {
    return index_;
  }

  public String getName() {
    return builder_.getConstraints(index_).getName();
  }

  private CpModelProto.Builder builder_;
  private int index_;
  private IntervalConstraintProto.Builder var_;
}
