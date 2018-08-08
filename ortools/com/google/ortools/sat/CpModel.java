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

import com.google.ortools.sat.AllDifferentConstraintProto;
import com.google.ortools.sat.BoolArgumentProto;
import com.google.ortools.sat.CpModelProto;
import com.google.ortools.sat.CpObjectiveProto;
import com.google.ortools.sat.CumulativeConstraintProto;
import com.google.ortools.sat.IntegerArgumentProto;
import com.google.ortools.sat.IntegerVariableProto;
import com.google.ortools.sat.LinearConstraintProto;
import com.google.ortools.sat.NoOverlap2DConstraintProto;
import com.google.ortools.sat.NoOverlapConstraintProto;

/**
 * Main modeling class.
 *
 * <p>It proposes a factory to create all modeling objects understood by the SAT solver.
 */
public class CpModel {
  public CpModel() {
    builder_ = CpModelProto.newBuilder();
  }

  // Integer variables.

  /** Creates an integer variable with domain [lb, ub]. */
  public IntVar newIntVar(long lb, long ub, String name) {
    return new IntVar(builder_, lb, ub, name);
  }

  /**
   * Creates an integer variable with an enumerated domain.
   *
   * @param bounds a flattened list of disjoint intervals
   * @param name the name of the variable
   * @return a variable whose domain is the union of [bounds[2 * i] .. bounds[2 * i + 1]]
   *     <p>To create a variable with enumerated domain [1, 2, 3, 5, 7, 8], pass in the array [1, 3,
   *     5, 5, 7, 8].
   */
  public IntVar newEnumeratedIntVar(long[] bounds, String name) {
    return new IntVar(builder_, bounds, name);
  }

  /**
   * Creates an integer variable with an enumerated domain.
   *
   * @param bounds a flattened list of disjoint intervals
   * @param name the name of the variable
   * @return a variable whose domain is the union of [bounds[2 * i] .. bounds[2 * i + 1]]
   *     <p>To create a variable with enumerated domain [1, 2, 3, 5, 7, 8], pass in the array [1, 3,
   *     5, 5, 7, 8].
   */
  public IntVar newEnumeratedIntVar(int[] bounds, String name) {
    return new IntVar(builder_, toLongArray(bounds), name);
  }

  /** Creates a Boolean variable with the given name. */
  public IntVar newBoolVar(String name) {
    return new IntVar(builder_, 0, 1, name);
  }

  /** Creates a constant variable. */
  public IntVar newConstant(long value) {
    return newIntVar(value, value, "" + value); // bounds and name.
  }

  // Boolean Constraints.

  /** Adds Or(literals) == true. */
  public Constraint addBoolOr(ILiteral[] literals) {
    Constraint ct = new Constraint(builder_);
    BoolArgumentProto.Builder boolOr = ct.builder().getBoolOrBuilder();
    for (ILiteral lit : literals) {
      boolOr.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /** Adds And(literals) == true. */
  public Constraint addBoolAnd(ILiteral[] literals) {
    Constraint ct = new Constraint(builder_);
    BoolArgumentProto.Builder boolOr = ct.builder().getBoolAndBuilder();
    for (ILiteral lit : literals) {
      boolOr.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /** Adds XOr(literals) == true. */
  public Constraint addBoolXor(ILiteral[] literals) {
    Constraint ct = new Constraint(builder_);
    BoolArgumentProto.Builder boolOr = ct.builder().getBoolXorBuilder();
    for (ILiteral lit : literals) {
      boolOr.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /** Adds a => b. */
  public Constraint addImplication(ILiteral a, ILiteral b) {
    return addBoolOr(new ILiteral[] {a.not(), b});
  }

  // Linear constraints.

  /** Adds lb <= sum(vars) <= ub. */
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

  /**
   * Adds sum(vars) in bounds, where bounds is a flattened domain similar to the one for enumerated
   * integer variables.
   */
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

  /**
   * Adds sum(vars) in bounds, where bounds is a flattened domain similar to the one for enumerated
   * integer variables.
   */
  public Constraint addLinearSumWithBounds(IntVar[] vars, int[] bounds) {
    return addLinearSumWithBounds(vars, toLongArray(bounds));
  }

  /** Adds sum(vars) == value. */
  public Constraint addLinearSumEqual(IntVar[] vars, long value) {
    return addLinearSum(vars, value, value);
  }

  /** Adds sum(vars) == target. */
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

  /** Adds lb <= sum(vars[i] * coeffs[i]) <= ub. */
  public Constraint addScalProd(IntVar[] vars, long[] coeffs, long lb, long ub) {
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

  /** Adds lb <= sum(vars[i] * coeffs[i]) <= ub. */
  public Constraint addScalProd(IntVar[] vars, int[] coeffs, long lb, long ub) {
    return addScalProd(vars, toLongArray(coeffs), lb, ub);
  }

  /**
   * Adds sum(vars[i] * coeffs[i]) in bounds, where bounds is a flattened domain similar to the one
   * for enumerated integer variables.
   */
  public Constraint addScalProdWithBounds(IntVar[] vars, long[] coeffs, long[] bounds) {
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

  /** Adds sum(vars[i] * coeffs[i]) == value. */
  public Constraint addScalProdEqual(IntVar[] vars, long[] coeffs, long value) {
    return addScalProd(vars, coeffs, value, value);
  }

  /** Adds sum(vars[i] * coeffs[i]) == value. */
  public Constraint addScalProdEqual(IntVar[] vars, int[] coeffs, long value) {
    return addScalProdEqual(vars, toLongArray(coeffs), value);
  }

  /** Adds sum(vars[i] * coeffs[i]) == target. */
  public Constraint addScalProdEqual(IntVar[] vars, long[] coeffs, IntVar target) {
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

  /** Adds sum(vars[i] * coeffs[i]) == target. */
  public Constraint addScalProdEqual(IntVar[] vars, int[] coeffs, IntVar target) {
    return addScalProdEqual(vars, toLongArray(coeffs), target);
  }

  /** Adds var <= value. */
  public Constraint addLessOrEqual(IntVar var, long value) {
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    lin.addVars(var.getIndex());
    lin.addCoeffs(1);
    lin.addDomain(java.lang.Long.MIN_VALUE);
    lin.addDomain(value);
    return ct;
  }

  /** Adds var >= value. */
  public Constraint addGreaterOrEqual(IntVar var, long value) {
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    lin.addVars(var.getIndex());
    lin.addCoeffs(1);
    lin.addDomain(value);
    lin.addDomain(java.lang.Long.MAX_VALUE);
    return ct;
  }

  /** Adds var == value. */
  public Constraint addEquality(IntVar var, long value) {
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    lin.addVars(var.getIndex());
    lin.addCoeffs(1);
    lin.addDomain(value);
    lin.addDomain(value);
    return ct;
  }

  /** Adds a == b. */
  public Constraint addEquality(IntVar a, IntVar b) {
    return addEqualityWithOffset(a, b, 0);
  }

  /** Adds a + offset == b. */
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

  /** Adds var != value. */
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

  /** Adds a != b. */
  public Constraint addDifferent(IntVar a, IntVar b) {
    return addDifferentWithOffset(a, b, 0);
  }

  /** Adds a + offset != b */
  public Constraint addDifferentWithOffset(IntVar a, IntVar b, long offset) {
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    lin.addVars(a.getIndex());
    lin.addCoeffs(-1);
    lin.addVars(b.getIndex());
    lin.addCoeffs(1);
    lin.addDomain(java.lang.Long.MIN_VALUE);
    lin.addDomain(offset - 1);
    lin.addDomain(offset + 1);
    lin.addDomain(java.lang.Long.MAX_VALUE);
    return ct;
  }

  /** before + offset <= after. */
  public Constraint addLessOrEqualWithOffset(IntVar before, IntVar after, long offset) {
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

  /** Adds before <= after. */
  public Constraint addLessOrEqual(IntVar before, IntVar after) {
    return addLessOrEqualWithOffset(before, after, 0);
  }

  // Integer constraints.

  /**
   * Adds AllDifferent(variables).
   *
   * <p>This constraint forces all variables to have different values.
   *
   * @param variables a list of integer variables.
   * @return an instance of the Constraint class.
   */
  public Constraint addAllDifferent(IntVar[] vars) {
    Constraint ct = new Constraint(builder_);
    AllDifferentConstraintProto.Builder allDiff = ct.builder().getAllDiffBuilder();
    for (IntVar var : vars) {
      allDiff.addVars(var.getIndex());
    }
    return ct;
  }

  // addElement + variations
  // addCircuit
  // addAllowedAssignments
  // addForbiddenAssignments
  // addAutomata
  // addInverse
  // addReservoirConstraint
  // addMapDomain

  /** Adds target == Min(vars). */
  public Constraint addMinEquality(IntVar target, IntVar[] vars) {
    Constraint ct = new Constraint(builder_);
    IntegerArgumentProto.Builder intMin = ct.builder().getIntMinBuilder();
    intMin.setTarget(target.getIndex());
    for (IntVar var : vars) {
      intMin.addVars(var.getIndex());
    }
    return ct;
  }

  /** Adds target == Max(vars). */
  public Constraint addMaxEquality(IntVar target, IntVar[] vars) {
    Constraint ct = new Constraint(builder_);
    IntegerArgumentProto.Builder intMax = ct.builder().getIntMaxBuilder();
    intMax.setTarget(target.getIndex());
    for (IntVar var : vars) {
      intMax.addVars(var.getIndex());
    }
    return ct;
  }

  /** Adds target == num / denom, rounded towards 0. */
  public Constraint addDivisionEquality(IntVar target, IntVar num, IntVar denom) {
    Constraint ct = new Constraint(builder_);
    IntegerArgumentProto.Builder intDiv = ct.builder().getIntDivBuilder();
    intDiv.setTarget(target.getIndex());
    intDiv.addVars(num.getIndex());
    intDiv.addVars(denom.getIndex());
    return ct;
  }

  /** Adds target == var / mod. */
  public Constraint addModuloEquality(IntVar target, IntVar var, IntVar mod) {
    Constraint ct = new Constraint(builder_);
    IntegerArgumentProto.Builder intMod = ct.builder().getIntModBuilder();
    intMod.setTarget(target.getIndex());
    intMod.addVars(var.getIndex());
    intMod.addVars(mod.getIndex());
    return ct;
  }

  /** Adds target == var / mod. */
  public Constraint addModuloEquality(IntVar target, IntVar var, long mod) {
    Constraint ct = new Constraint(builder_);
    IntegerArgumentProto.Builder intMod = ct.builder().getIntModBuilder();
    intMod.setTarget(target.getIndex());
    intMod.addVars(var.getIndex());
    intMod.addVars(indexFromConstant(mod));
    return ct;
  }

  /** Adds target == Product(vars). */
  public Constraint addProductEquality(IntVar target, IntVar[] vars) {
    Constraint ct = new Constraint(builder_);
    IntegerArgumentProto.Builder intProd = ct.builder().getIntProdBuilder();
    intProd.setTarget(target.getIndex());
    for (IntVar var : vars) {
      intProd.addVars(var.getIndex());
    }
    return ct;
  }

  // Scheduling support.

  /**
   * Creates an interval variables from start, size, and end.
   *
   * <p>An interval variable is a constraint, that is itself used in other constraints like
   * NoOverlap.
   *
   * <p>Internally, it ensures that start + size == end.
   *
   * @param start the start of the interval.
   * @param size the size of the interval.
   * @param end the end of the interval.
   * @param name the name of the interval variable.
   * @return An IntervalVar object.
   */
  public IntervalVar newIntervalVar(IntVar start, IntVar duration, IntVar end, String name) {
    return new IntervalVar(builder_, start.getIndex(), duration.getIndex(), end.getIndex(), name);
  }

  /**
   * Creates an interval variables with a fixed end.
   *
   * @see CpModel#newIntervalVar(IntVar, IntVar, IntVar, String)
   */
  public IntervalVar newIntervalVar(IntVar start, IntVar duration, long end, String name) {
    return new IntervalVar(
        builder_, start.getIndex(), duration.getIndex(), indexFromConstant(end), name);
  }

  /**
   * Creates an interval variables with a fixed duration.
   *
   * @see CpModel#newIntervalVar(IntVar, IntVar, IntVar, String)
   */
  public IntervalVar newIntervalVar(IntVar start, long duration, IntVar end, String name) {
    return new IntervalVar(
        builder_, start.getIndex(), indexFromConstant(duration), end.getIndex(), name);
  }

  /**
   * Creates an interval variables with a fixed start.
   *
   * @see CpModel#newIntervalVar(IntVar, IntVar, IntVar, String)
   */
  public IntervalVar newIntervalVar(long start, IntVar duration, IntVar end, String name) {
    return new IntervalVar(
        builder_, indexFromConstant(start), duration.getIndex(), end.getIndex(), name);
  }

  /** Creates a fixed interval from its start and its duration. */
  public IntervalVar newFixedInterval(long start, long duration, String name) {
    return new IntervalVar(
        builder_,
        indexFromConstant(start),
        indexFromConstant(duration),
        indexFromConstant(start + duration),
        name);
  }

  /**
   * Creates an optional interval var from start, size, end. and is_present.
   *
   * <p>An optional interval variable is a constraint, that is itself used in other constraints like
   * NoOverlap. This constraint is protected by an is_present literal that indicates if it is active
   * or not.
   *
   * <p>Internally, it ensures that is_present implies start + size == end.
   *
   * @param start the start of the interval. It can be an integer value, or an integer variable.
   * @param size the size of the interval. It can be an integer value, or an integer variable.
   * @param end the end of the interval. It can be an integer value, or an integer variable.
   * @param is_present a literal that indicates if the interval is active or not. A inactive
   *     interval is simply ignored by all constraints.
   * @param name The name of the interval variable.
   * @return an IntervalVar object.
   */
  public IntervalVar newOptionalIntervalVar(
      IntVar start, IntVar duration, IntVar end, ILiteral presence, String name) {
    return new IntervalVar(
        builder_, start.getIndex(), duration.getIndex(), end.getIndex(), presence.getIndex(), name);
  }

  /**
   * Creates an optional interval with a fixed end.
   *
   * @see CpModel#newOptionalIntervalVar(IntVar, IntVar, IntVar, ILiteral, String)
   */
  public IntervalVar newOptionalIntervalVar(
      IntVar start, IntVar duration, long end, ILiteral presence, String name) {
    return new IntervalVar(
        builder_,
        start.getIndex(),
        duration.getIndex(),
        indexFromConstant(end),
        presence.getIndex(),
        name);
  }

  /**
   * Creates an optional interval with a fixed duration.
   *
   * @see CpModel#newOptionalIntervalVar(IntVar, IntVar, IntVar, ILiteral, String)
   */
  public IntervalVar newOptionalIntervalVar(
      IntVar start, long duration, IntVar end, ILiteral presence, String name) {
    return new IntervalVar(
        builder_,
        start.getIndex(),
        indexFromConstant(duration),
        end.getIndex(),
        presence.getIndex(),
        name);
  }

  /** Creates an optional interval with a fixed start. */
  public IntervalVar newOptionalIntervalVar(
      long start, IntVar duration, IntVar end, ILiteral presence, String name) {
    return new IntervalVar(
        builder_,
        indexFromConstant(start),
        duration.getIndex(),
        end.getIndex(),
        presence.getIndex(),
        name);
  }

  /**
   * Creates an optional fixed interval from start and duration.
   *
   * @see CpModel#newOptionalIntervalVar(IntVar, IntVar, IntVar, ILiteral, String)
   */
  public IntervalVar newOptionalFixedInterval(
      long start, long duration, ILiteral presence, String name) {
    return new IntervalVar(
        builder_,
        indexFromConstant(start),
        indexFromConstant(duration),
        indexFromConstant(start + duration),
        presence.getIndex(),
        name);
  }

  /**
   * Adds NoOverlap(intervalVars).
   *
   * <p>A NoOverlap constraint ensures that all present intervals do not overlap in time.
   *
   * @param intervalVars the list of interval variables to constrain.
   * @return an instance of the Constraint class.
   */
  public Constraint addNoOverlap(IntervalVar[] intervalVars) {
    Constraint ct = new Constraint(builder_);
    NoOverlapConstraintProto.Builder noOverlap = ct.builder().getNoOverlapBuilder();
    for (IntervalVar var : intervalVars) {
      noOverlap.addIntervals(var.getIndex());
    }
    return ct;
  }

  /**
   * Adds NoOverlap2D(xIntervals, yIntervals).
   *
   * <p>A NoOverlap2D constraint ensures that all present rectangles do not overlap on a plan. Each
   * rectangle is aligned with the X and Y axis, and is defined by two intervals which represent its
   * projection onto the X and Y axis.
   *
   * @param xIntervals the X coordinates of the rectangles.
   * @param yIntervals the Y coordinates of the rectangles.
   * @return an instance of the Constraint class.
   */
  public Constraint addNoOverlap2D(IntervalVar[] xIntervals, IntervalVar[] yIntervals) {
    Constraint ct = new Constraint(builder_);
    NoOverlap2DConstraintProto.Builder noOverlap2d = ct.builder().getNoOverlap2DBuilder();
    for (IntervalVar x : xIntervals) {
      noOverlap2d.addXIntervals(x.getIndex());
    }
    for (IntervalVar y : yIntervals) {
      noOverlap2d.addXIntervals(y.getIndex());
    }
    return ct;
  }

  /**
   * Adds Cumulative(intervals, demands, capacity).
   *
   * <p>This constraint enforces that: for all t: sum(demands[i] if (start(intervals[t]) <= t <
   * end(intervals[t])) and (t is present)) <= capacity
   *
   * @param intervals the list of intervals.
   * @param demands the list of demands for each interval. Each demand must be a positive integer
   *     variable.
   * @param capacity the maximum capacity of the cumulative constraint. It must be a positive
   *     integer variable.
   * @return An instance of the Constraint class.
   */
  public Constraint addCumulative(IntervalVar[] intervals, IntVar[] demands, IntVar capacity) {
    Constraint ct = new Constraint(builder_);
    CumulativeConstraintProto.Builder cumul = ct.builder().getCumulativeBuilder();
    for (IntervalVar interval : intervals) {
      cumul.addIntervals(interval.getIndex());
    }
    for (IntVar var : demands) {
      cumul.addDemands(var.getIndex());
    }
    cumul.setCapacity(capacity.getIndex());
    return ct;
  }

  /**
   * Adds Cumulative(intervals, demands, capacity). with fixed demands.
   *
   * @see CpModel.AddCumulative(IntervalVar[], IntVar[], IntVar).
   */
  public Constraint addCumulative(IntervalVar[] intervals, long[] demands, IntVar capacity) {
    Constraint ct = new Constraint(builder_);
    CumulativeConstraintProto.Builder cumul = ct.builder().getCumulativeBuilder();
    for (IntervalVar interval : intervals) {
      cumul.addIntervals(interval.getIndex());
    }
    for (long d : demands) {
      cumul.addDemands(indexFromConstant(d));
    }
    cumul.setCapacity(capacity.getIndex());
    return ct;
  }

  /**
   * Adds Cumulative(intervals, demands, capacity). with fixed demands.
   *
   * @see CpModel.AddCumulative(IntervalVar[], IntVar[], IntVar).
   */
  public Constraint addCumulative(IntervalVar[] intervals, int[] demands, IntVar capacity) {
    return addCumulative(intervals, toLongArray(demands), capacity);
  }

  /**
   * Adds Cumulative(intervals, demands, capacity). with fixed capacity.
   *
   * @see CpModel.AddCumulative(IntervalVar[], IntVar[], IntVar).
   */
  public Constraint addCumulative(IntervalVar[] intervals, IntVar[] demands, long capacity) {
    Constraint ct = new Constraint(builder_);
    CumulativeConstraintProto.Builder cumul = ct.builder().getCumulativeBuilder();
    for (IntervalVar interval : intervals) {
      cumul.addIntervals(interval.getIndex());
    }
    for (IntVar var : demands) {
      cumul.addDemands(var.getIndex());
    }
    cumul.setCapacity(indexFromConstant(capacity));
    return ct;
  }

  /**
   * Adds Cumulative(intervals, demands, capacity). with fixed demands and fixed capacity.
   *
   * @see CpModel.AddCumulative(IntervalVar[], IntVar[], IntVar).
   */
  public Constraint addCumulative(IntervalVar[] intervals, long[] demands, long capacity) {
    Constraint ct = new Constraint(builder_);
    CumulativeConstraintProto.Builder cumul = ct.builder().getCumulativeBuilder();
    for (IntervalVar interval : intervals) {
      cumul.addIntervals(interval.getIndex());
    }
    for (long d : demands) {
      cumul.addDemands(indexFromConstant(d));
    }
    cumul.setCapacity(indexFromConstant(capacity));
    return ct;
  }

  /**
   * Adds Cumulative(intervals, demands, capacity). with fixed demands and fixed capacity.
   *
   * @see CpModel.AddCumulative(IntervalVar[], IntVar[], IntVar).
   */
  public Constraint addCumulative(IntervalVar[] intervals, int[] demands, long capacity) {
    return addCumulative(intervals, toLongArray(demands), capacity);
  }

  // AddsReservoir

  // Objective.

  /** Adds a minimization objective of a single variable. */
  public void minimize(IntVar var) {
    CpObjectiveProto.Builder obj = builder_.getObjectiveBuilder();
    obj.addVars(var.getIndex());
    obj.addCoeffs(1);
  }

  /** Adds a minimization objective of a sum of variables. */
  public void minimizeSum(IntVar[] vars) {
    CpObjectiveProto.Builder obj = builder_.getObjectiveBuilder();
    for (IntVar var : vars) {
      obj.addVars(var.getIndex());
      obj.addCoeffs(1);
    }
  }

  /** Adds a minimization objective of a scalar product of variables. */
  public void minimizeScalProd(IntVar[] vars, long[] coeffs) {
    CpObjectiveProto.Builder obj = builder_.getObjectiveBuilder();
    for (IntVar var : vars) {
      obj.addVars(var.getIndex());
    }
    for (long c : coeffs) {
      obj.addCoeffs(c);
    }
  }

  /** Adds a minimization objective of a scalar product of variables. */
  public void minimizeScalProd(IntVar[] vars, int[] coeffs) {
    minimizeScalProd(vars, toLongArray(coeffs));
  }

  /** Adds a maximization objective of a single variable. */
  public void maximize(IntVar var) {
    CpObjectiveProto.Builder obj = builder_.getObjectiveBuilder();
    obj.addVars(negated(var.getIndex()));
    obj.addCoeffs(1);
    obj.setScalingFactor(-1.0);
  }

  /** Adds a maximization objective of a sum of variables. */
  public void maximizeSum(IntVar[] vars) {
    CpObjectiveProto.Builder obj = builder_.getObjectiveBuilder();
    for (IntVar var : vars) {
      obj.addVars(negated(var.getIndex()));
      obj.addCoeffs(1);
    }
    obj.setScalingFactor(-1.0);
  }

  /** Adds a maximization objective of a scalar product of variables. */
  public void maximizeScalProd(IntVar[] vars, long[] coeffs) {
    CpObjectiveProto.Builder obj = builder_.getObjectiveBuilder();
    for (IntVar var : vars) {
      obj.addVars(negated(var.getIndex()));
    }
    for (long c : coeffs) {
      obj.addCoeffs(c);
    }
    obj.setScalingFactor(-1.0);
  }

  /** Adds a maximization objective of a scalar product of variables. */
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

  public CpModelProto model() {
    return builder_.build();
  }

  public int negated(int index) {
    return -index - 1;
  }

  private CpModelProto.Builder builder_;
}
