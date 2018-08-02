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
import com.google.ortools.sat.Constraint;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.CpObjectiveProto;

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

  public Constraint addBoolOr(ILiteral[] literals) {
    Constraint ct = new Constraint(builder_);
    BoolArgumentProto.Builder bool_or = ct.builder().getBoolOrBuilder();
    for (ILiteral lit : literals) {
      bool_or.addLiterals(lit.getIndex());
    }
    return ct;
  }

  public Constraint addBoolAnd(ILiteral[] literals) {
    Constraint ct = new Constraint(builder_);
    BoolArgumentProto.Builder bool_or = ct.builder().getBoolAndBuilder();
    for (ILiteral lit : literals) {
      bool_or.addLiterals(lit.getIndex());
    }
    return ct;
  }

  public Constraint addBoolXor(ILiteral[] literals) {
    Constraint ct = new Constraint(builder_);
    BoolArgumentProto.Builder bool_or = ct.builder().getBoolXorBuilder();
    for (ILiteral lit : literals) {
      bool_or.addLiterals(lit.getIndex());
    }
    return ct;
  }

  public Constraint addImplication(ILiteral a, ILiteral b) {
    return addBoolOr(new ILiteral[] {a.not(), b});
  }

  // Linear constraints.

  public Constraint addLinearSum(IntVar[] vars, long lb, long ub) {
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    for (IntVar var : vars) {
      lin.addVars(var.getIndex());
      lin.addCoeffs(1);
    }
    lin.addDomain(lb);
    lin.addDomain(ub);
    return ct;
  }

  public Constraint addLinearSumEqual(IntVar[] vars, long v) {
    return addLinearSum(vars, v, v);
  }

  public Constraint addLinearSumEqual(IntVar[] vars, IntVar target) {
    int size = vars.length;
    IntVar[] newVars = new IntVar[size + 1];
    long[] coeffs = new long[size + 1];
    for (int i = 0; i < size; ++i) {
      newVars[i] = vars[i];
      coeffs[i] = 1;
    }
    newVars[size] = target;
    coeffs[size] = -1;
    return addScalProd(newVars, coeffs, 0, 0);
  }

  public Constraint addScalProd(IntVar[] vars, long[] coeffs, long lb,
                                long ub) {
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    for (IntVar var : vars) {
      lin.addVars(var.getIndex());
    }
    for (long c : coeffs) {
      lin.addCoeffs(c);
    }
    lin.addDomain(lb);
    lin.addDomain(ub);
    return ct;
  }

  public Constraint addScalProd(IntVar[] vars, int[] coeffs, long lb, long ub) {
    return addScalProd(vars, toLongArray(coeffs), lb, ub);
  }

  public Constraint addScalProdEqual(IntVar[] vars, long[] coeffs, long value) {
    return addScalProd(vars, coeffs, value, value);
  }

  public Constraint addScalProdEqual(IntVar[] vars, int[] coeffs, long value) {
    return addScalProdEqual(vars, toLongArray(coeffs), value);
  }

  public Constraint addScalProdEqual(IntVar[] vars, long[] coeffs,
                                     IntVar target) {
    int size = vars.length;
    IntVar[] newVars = new IntVar[size + 1];
    long[] newCoeffs = new long[size + 1];
    for (int i = 0; i < size; ++i) {
      newVars[i] = vars[i];
      newCoeffs[i] = coeffs[i];
    }
    newVars[size] = target;
    newCoeffs[size] = -1;
    return addScalProd(newVars, newCoeffs, 0, 0);
  }

  public Constraint addScalProdEqual(IntVar[] vars, int[] coeffs,
                                     IntVar target) {
    return addScalProdEqual(vars, toLongArray(coeffs), target);
  }


  public Constraint addLessOrEqual(IntVar var, long value) {
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    lin.addVars(var.getIndex());
    lin.addCoeffs(1);
    lin.addDomain(java.lang.Long.MIN_VALUE);
    lin.addDomain(value);
    return ct;
  }

  public Constraint addGreaterOrEqual(IntVar var, long value) {
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    lin.addVars(var.getIndex());
    lin.addCoeffs(1);
    lin.addDomain(value);
    lin.addDomain(java.lang.Long.MAX_VALUE);
    return ct;
  }

  // before + offset <= after.
  public Constraint addLessOrEqualWithOffset(IntVar before, IntVar after,
                                             long offset) {
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    lin.addVars(before.getIndex());
    lin.addCoeffs(-1);
    lin.addVars(after.getIndex());
    lin.addCoeffs(1);
    lin.addDomain(offset);
    lin.addDomain(java.lang.Long.MAX_VALUE);
    return ct;
  }

  public Constraint addLessOrEqual(IntVar before, IntVar after) {
    return addLessOrEqualWithOffset(before, after, 0);
  }

  // Objective.

  public void MaximizeSum(IntVar[] vars) {
    CpObjectiveProto.Builder obj = builder_.getObjectiveBuilder();
    for (IntVar var : vars) {
      obj.addVars(Negated(var.getIndex()));
      obj.addCoeffs(1);
    }
    obj.setScalingFactor(-1.0);
  }

  // Helpers

  long[] toLongArray(int[] values) {
    long[] result = new long[values.length];
    for (int i = 0; i < values.length; ++i) {
      result[i] = values[i];
    }
    return result;
  }

  // Getters.

  public CpModelProto model() { return builder_.build(); }
  public int Negated(int index) { return -index - 1; }

  private CpModelProto.Builder builder_;
}
