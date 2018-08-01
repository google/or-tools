package com.google.ortools.sat;

import com.google.ortools.sat.CpModelProto;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.IntegerVariableProto;

public class IntVar {
  public IntVar(CpModelProto.Builder builder, long lb, long ub, String name) {
    this.builder_ = builder;
    this.index_ = builder_.getVariablesCount();
    this.var_ = builder_.addVariablesBuilder();
    this.var_.setName(name);
    this.var_.addDomain(lb);
    this.var_.addDomain(ub);
  }

  @Override
  public String toString() {
    return var_.toString();
  }

  public int getIndex() {
    return index_;
  }

  public String getName() {
    return var_.getName();
  }

  public String ShortString() {
    if (var_.getName().isEmpty()) {
      return toString();
    } else {
      return var_.getName();
    }
  }

  private CpModelProto.Builder builder_;
  private int index_;
  private IntegerVariableProto.Builder var_;
}
