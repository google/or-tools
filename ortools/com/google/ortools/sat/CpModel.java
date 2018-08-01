package com.google.ortools.sat;

import com.google.ortools.sat.CpModelProto;

public class CpModel {
  public CpModel() {
    builder_ = CpModelProto.newBuilder();
  }

  public IntVar newIntVar(long lb, long ub, String name) {
    return new IntVar(builder_, lb, ub, name);
  }

  public IntVar newBoolVar(String name) {
    return new IntVar(builder_, 0, 1, name);
  }

  public CpModelProto Model() { return builder_.build(); }
  public int Negated(int index) { return -index - 1; }

  private CpModelProto.Builder builder_;
}
