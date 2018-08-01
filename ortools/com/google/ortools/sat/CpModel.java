package com.google.ortools.sat;

import com.google.ortools.sat.CpModelProto;

public class CpModel {
  public CpModel() {
    builder_ = CpModelProto.newBuilder();
  }

  public CpModelProto Model() { return builder_.build(); }
  public int Negated(int index) { return -index - 1; }

  private CpModelProto.Builder builder_;
}
