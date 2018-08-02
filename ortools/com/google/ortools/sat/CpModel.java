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

  public IntVar newEnumeratedIntVar(long[] bounds, String name) {
    return new IntVar(builder_, bounds, name);
  }

  public IntVar newEnumeratedIntVar(int[] bounds, String name) {
    return new IntVar(builder_, toLongArray(bounds), name);
  }

  public IntVar newBoolVar(String name) {
    return new IntVar(builder_, 0, 1, name);
  }

  public IntVar newConstant(long value) {
    return newIntVar(value, value, "" + value);  // bounds and name.
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

  public Constraint addLinearSumWithBounds(IntVar[] vars, long[] bounds) {
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    for (IntVar var : vars) {
      lin.addVars(var.getIndex());
      lin.addCoeffs(1);
    }
    for (long b : bounds) {
      lin.addDomain(b);
    }
    return ct;
  }

  public Constraint addLinearSumWithBounds(IntVar[] vars, int[] bounds) {
    return addLinearSumWithBounds(vars, toLongArray(bounds));
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

  public Constraint addScalProdWithBounds(IntVar[] vars, long[] coeffs,
                                          long[] bounds){
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    for (IntVar var : vars) {
      lin.addVars(var.getIndex());
    }
    for (long c : coeffs) {
      lin.addCoeffs(c);
    }
    for (long b : bounds) {
      lin.addDomain(b);
    }
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

  public Constraint addEquality(IntVar var, long value) {
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    lin.addVars(var.getIndex());
    lin.addCoeffs(1);
    lin.addDomain(value);
    lin.addDomain(value);
    return ct;
  }

  public Constraint addEquality(IntVar a, IntVar b) {
    return addEqualityWithOffset(a, b, 0);
  }

  // a + offset == b
  public Constraint addEqualityWithOffset(IntVar a, IntVar b, long offset) {
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    lin.addVars(a.getIndex());
    lin.addCoeffs(-1);
    lin.addVars(b.getIndex());
    lin.addCoeffs(1);
    lin.addDomain(offset);
    lin.addDomain(offset);
    return ct;
  }

  public Constraint addDifferent(IntVar var, long value) {
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    lin.addVars(var.getIndex());
    lin.addCoeffs(1);
    lin.addDomain(java.lang.Long.MIN_VALUE);
    lin.addDomain(value - 1);
    lin.addDomain(value + 1);
    lin.addDomain(java.lang.Long.MAX_VALUE);
    return ct;
  }

  public Constraint addDifferent(IntVar a, IntVar b) {
    return addDifferentWithOffset(a, b, 0);
  }

  public Constraint addDifferentWithOffset(IntVar a, IntVar b, long offset) {
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    lin.addVars(a.getIndex());
    lin.addCoeffs(1);
    lin.addVars(b.getIndex());
    lin.addCoeffs(-1);
    lin.addDomain(java.lang.Long.MIN_VALUE);
    lin.addDomain(offset - 1);
    lin.addDomain(offset + 1);
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

  // Scheduling support.
  public IntervalVar newIntervalVar(IntVar start, IntVar duration, IntVar end,
                                    String name) {
    return new IntervalVar(
        builder_, start.getIndex(), duration.getIndex(), end.getIndex(), name);
  }

  public IntervalVar newIntervalVar(IntVar start, IntVar duration, long end,
                                    String name) {
    return new IntervalVar(
        builder_, start.getIndex(), duration.getIndex(), indexFromConstant(end),
        name);
  }

  public IntervalVar newIntervalVar(IntVar start, long duration, IntVar end,
                                    String name) {
    return new IntervalVar(
        builder_, start.getIndex(), indexFromConstant(duration), end.getIndex(),
        name);
  }

  public IntervalVar newIntervalVar(long start, IntVar duration, IntVar end,
                                    String name) {
    return new IntervalVar(
        builder_, indexFromConstant(start), duration.getIndex(), end.getIndex(),
        name);
  }

  public IntervalVar newFixedInterval(long start, long duration, String name) {
    return new IntervalVar(
        builder_, indexFromConstant(start), indexFromConstant(duration),
        indexFromConstant(start + duration), name);
  }

  public IntervalVar newOptionalIntervalVar(IntVar start, IntVar duration,
                                            IntVar end, ILiteral presence,
                                            String name) {
    return new IntervalVar(
        builder_, start.getIndex(), duration.getIndex(), end.getIndex(),
        presence.getIndex(), name);
  }

  public IntervalVar newOptionalIntervalVar(IntVar start, IntVar duration,
                                            long end, ILiteral presence,
                                            String name) {
    return new IntervalVar(
        builder_, start.getIndex(), duration.getIndex(), indexFromConstant(end),
        presence.getIndex(), name);
  }

  public IntervalVar newOptionalIntervalVar(IntVar start, long duration,
                                            IntVar end, ILiteral presence,
                                            String name) {
    return new IntervalVar(
        builder_, start.getIndex(), indexFromConstant(duration), end.getIndex(),
        presence.getIndex(), name);
  }

  public IntervalVar newOptionalIntervalVar(long start, IntVar duration,
                                            IntVar end, ILiteral presence,
                                            String name) {
    return new IntervalVar(
        builder_, indexFromConstant(start), duration.getIndex(), end.getIndex(),
        presence.getIndex(), name);
  }

  public IntervalVar newOptionalFixedInterval(long start, long duration,
                                              ILiteral presence, String name) {
    return new IntervalVar(
        builder_, indexFromConstant(start), indexFromConstant(duration),
        indexFromConstant(start + duration), presence.getIndex(), name);
  }

  // Objective.

  public void minimize(IntVar var) {
    CpObjectiveProto.Builder obj = builder_.getObjectiveBuilder();
    obj.addVars(var.getIndex());
    obj.addCoeffs(1);
  }

  public void minimizeSum(IntVar[] vars) {
    CpObjectiveProto.Builder obj = builder_.getObjectiveBuilder();
    for (IntVar var : vars) {
      obj.addVars(var.getIndex());
      obj.addCoeffs(1);
    }
  }

  public void minimizeScalProd(IntVar[] vars, long[] coeffs) {
    CpObjectiveProto.Builder obj = builder_.getObjectiveBuilder();
    for (IntVar var : vars) {
      obj.addVars(var.getIndex());
    }
    for (long c : coeffs) {
      obj.addCoeffs(c);
    }
  }

  public void minimizeScalProd(IntVar[] vars, int[] coeffs) {
    minimizeScalProd(vars, toLongArray(coeffs));
  }

  public void maximize(IntVar var) {
    CpObjectiveProto.Builder obj = builder_.getObjectiveBuilder();
    obj.addVars(Negated(var.getIndex()));
    obj.addCoeffs(1);
    obj.setScalingFactor(-1.0);
  }

  public void maximizeSum(IntVar[] vars) {
    CpObjectiveProto.Builder obj = builder_.getObjectiveBuilder();
    for (IntVar var : vars) {
      obj.addVars(Negated(var.getIndex()));
      obj.addCoeffs(1);
    }
    obj.setScalingFactor(-1.0);
  }

  public void maximizeScalProd(IntVar[] vars, long[] coeffs) {
    CpObjectiveProto.Builder obj = builder_.getObjectiveBuilder();
    for (IntVar var : vars) {
      obj.addVars(Negated(var.getIndex()));
    }
    for (long c : coeffs) {
      obj.addCoeffs(c);
    }
    obj.setScalingFactor(-1.0);
  }

  public void maximizeScalProd(IntVar[] vars, int[] coeffs) {
    maximizeScalProd(vars, toLongArray(coeffs));
  }

  // Helpers

  long[] toLongArray(int[] values) {
    long[] result = new long[values.length];
    for (int i = 0; i < values.length; ++i) {
      result[i] = values[i];
    }
    return result;
  }

  int indexFromConstant(long constant) {
    int index = builder_.getVariablesCount();
    IntegerVariableProto.Builder cst = builder_.addVariablesBuilder();
    cst.addDomain(constant);
    cst.addDomain(constant);
    return index;
  }

  // Getters.

  public CpModelProto model() { return builder_.build(); }
  public int Negated(int index) { return -index - 1; }

  private CpModelProto.Builder builder_;
}
