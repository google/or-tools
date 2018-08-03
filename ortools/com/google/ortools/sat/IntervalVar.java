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
import com.google.ortools.sat.IntervalConstraintProto;

/** An interval variable. */
public class IntervalVar {
  public IntervalVar(
      CpModelProto.Builder builder, int startIndex, int sizeIndex, int endIndex, String name) {
    this.builder_ = builder;
    this.index_ = builder_.getConstraintsCount();
    ConstraintProto.Builder ct = builder_.addConstraintsBuilder();
    ct.setName(name);
    this.var_ = ct.getIntervalBuilder();
    this.var_.setStart(startIndex);
    this.var_.setSize(sizeIndex);
    this.var_.setEnd(endIndex);
  }

  public IntervalVar(
      CpModelProto.Builder builder,
      int startIndex,
      int sizeIndex,
      int endIndex,
      int isPresentIndex,
      String name) {
    this.builder_ = builder;
    this.index_ = builder_.getConstraintsCount();
    ConstraintProto.Builder ct = builder_.addConstraintsBuilder();
    ct.setName(name);
    ct.addEnforcementLiteral(isPresentIndex);
    this.var_ = ct.getIntervalBuilder();
    this.var_.setStart(startIndex);
    this.var_.setSize(sizeIndex);
    this.var_.setEnd(endIndex);
  }

  @Override
  public String toString() {
    return builder_.getConstraints(index_).toString();
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
