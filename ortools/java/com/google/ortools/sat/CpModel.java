// Copyright 2010-2021 Google LLC
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
import com.google.ortools.sat.AutomatonConstraintProto;
import com.google.ortools.sat.BoolArgumentProto;
import com.google.ortools.sat.CpModelProto;
import com.google.ortools.sat.CpObjectiveProto;
import com.google.ortools.sat.CumulativeConstraintProto;
import com.google.ortools.sat.DecisionStrategyProto;
import com.google.ortools.sat.ElementConstraintProto;
import com.google.ortools.sat.FloatObjectiveProto;
import com.google.ortools.sat.InverseConstraintProto;
import com.google.ortools.sat.LinearArgumentProto;
import com.google.ortools.sat.LinearConstraintProto;
import com.google.ortools.sat.LinearExpressionProto;
import com.google.ortools.sat.NoOverlapConstraintProto;
import com.google.ortools.sat.ReservoirConstraintProto;
import com.google.ortools.sat.TableConstraintProto;
import com.google.ortools.util.Domain;
import java.util.LinkedHashMap;
import java.util.Map;

// TODO(user): Rewrite API to be closer to the C++ CpModel class.
/**
 * Main modeling class.
 *
 * <p>Proposes a factory to create all modeling objects understood by the SAT solver.
 */
public final class CpModel {
  static class CpModelException extends RuntimeException {
    public CpModelException(String methodName, String msg) {
      // Call constructor of parent Exception
      super(methodName + ": " + msg);
    }
  }

  /** Exception thrown when parallel arrays have mismatched lengths. */
  public static class MismatchedArrayLengths extends CpModelException {
    public MismatchedArrayLengths(String methodName, String array1Name, String array2Name) {
      super(methodName, array1Name + " and " + array2Name + " have mismatched lengths");
    }
  }

  /** Exception thrown when an array has a wrong length. */
  public static class WrongLength extends CpModelException {
    public WrongLength(String methodName, String msg) {
      super(methodName, msg);
    }
  }

  public CpModel() {
    modelBuilder = CpModelProto.newBuilder();
    constantMap = new LinkedHashMap<>();
  }

  // Integer variables.

  /** Creates an integer variable with domain [lb, ub]. */
  public IntVar newIntVar(long lb, long ub, String name) {
    return new IntVar(modelBuilder, new Domain(lb, ub), name);
  }

  /**
   * Creates an integer variable with given domain.
   *
   * @param domain an instance of the Domain class.
   * @param name the name of the variable
   * @return a variable with the given domain.
   */
  public IntVar newIntVarFromDomain(Domain domain, String name) {
    return new IntVar(modelBuilder, domain, name);
  }

  /** Creates a Boolean variable with the given name. */
  public IntVar newBoolVar(String name) {
    return new IntVar(modelBuilder, new Domain(0, 1), name);
  }

  /** Creates a constant variable. */
  public IntVar newConstant(long value) {
    if (constantMap.containsKey(value)) {
      return constantMap.get(value);
    }
    IntVar cste = new IntVar(modelBuilder, new Domain(value), ""); // bounds and name.
    constantMap.put(value, cste);
    return cste;
  }

  /** Returns the true literal. */
  public Literal trueLiteral() {
    return newConstant(1);
  }

  /** Returns the false literal. */
  public Literal falseLiteral() {
    return newConstant(0);
  }

  // Boolean Constraints.

  /** Adds {@code Or(literals) == true}. */
  public Constraint addBoolOr(Literal[] literals) {
    Constraint ct = new Constraint(modelBuilder);
    BoolArgumentProto.Builder boolOr = ct.getBuilder().getBoolOrBuilder();
    for (Literal lit : literals) {
      boolOr.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /** Same as addBoolOr. {@code Sum(literals) >= 1}. */
  public Constraint addAtLeastOne(Literal[] literals) {
    Constraint ct = new Constraint(modelBuilder);
    BoolArgumentProto.Builder boolOr = ct.getBuilder().getBoolOrBuilder();
    for (Literal lit : literals) {
      boolOr.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /** Adds {@code AtMostOne(literals): Sum(literals) <= 1}. */
  public Constraint addAtMostOne(Literal[] literals) {
    Constraint ct = new Constraint(modelBuilder);
    BoolArgumentProto.Builder atMostOne = ct.getBuilder().getAtMostOneBuilder();
    for (Literal lit : literals) {
      atMostOne.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /** Adds {@code ExactlyOne(literals): Sum(literals) == 1}. */
  public Constraint addExactlyOne(Literal[] literals) {
    Constraint ct = new Constraint(modelBuilder);
    BoolArgumentProto.Builder exactlyOne = ct.getBuilder().getExactlyOneBuilder();
    for (Literal lit : literals) {
      exactlyOne.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /** Adds {@code And(literals) == true}. */
  public Constraint addBoolAnd(Literal[] literals) {
    Constraint ct = new Constraint(modelBuilder);
    BoolArgumentProto.Builder boolAnd = ct.getBuilder().getBoolAndBuilder();
    for (Literal lit : literals) {
      boolAnd.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /** Adds {@code XOr(literals) == true}. */
  public Constraint addBoolXor(Literal[] literals) {
    Constraint ct = new Constraint(modelBuilder);
    BoolArgumentProto.Builder boolXOr = ct.getBuilder().getBoolXorBuilder();
    for (Literal lit : literals) {
      boolXOr.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /** Adds {@code a => b}. */
  public Constraint addImplication(Literal a, Literal b) {
    return addBoolOr(new Literal[] {a.not(), b});
  }

  // Linear constraints.

  /** Adds {@code expr in domain}. */
  public Constraint addLinearExpressionInDomain(LinearExpr expr, Domain domain) {
    Constraint ct = new Constraint(modelBuilder);
    LinearConstraintProto.Builder lin = ct.getBuilder().getLinearBuilder();
    for (int i = 0; i < expr.numElements(); ++i) {
      lin.addVars(expr.getVariable(i).getIndex()).addCoeffs(expr.getCoefficient(i));
    }
    long offset = expr.getOffset();
    if (offset != 0) {
      lin.addVars(newConstant(1).getIndex()).addCoeffs(offset);
    }
    for (long b : domain.flattenedIntervals()) {
      lin.addDomain(b);
    }
    return ct;
  }

  /** Adds {@code lb <= expr <= ub}. */
  public Constraint addLinearConstraint(LinearExpr expr, long lb, long ub) {
    return addLinearExpressionInDomain(expr, new Domain(lb, ub));
  }

  /** Adds {@code expr == value}. */
  public Constraint addEquality(LinearExpr expr, long value) {
    return addLinearExpressionInDomain(expr, new Domain(value));
  }

  /** Adds {@code left == right}. */
  public Constraint addEquality(LinearExpr left, LinearExpr right) {
    return addLinearExpressionInDomain(new Difference(left, right), new Domain(0));
  }

  /** Adds {@code left + offset == right}. */
  public Constraint addEqualityWithOffset(LinearExpr left, LinearExpr right, long offset) {
    return addLinearExpressionInDomain(new Difference(left, right), new Domain(-offset));
  }

  /** Adds {@code expr <= value}. */
  public Constraint addLessOrEqual(LinearExpr expr, long value) {
    return addLinearExpressionInDomain(expr, new Domain(Long.MIN_VALUE, value));
  }

  /** Adds {@code left <= right}. */
  public Constraint addLessOrEqual(LinearExpr left, LinearExpr right) {
    return addLinearExpressionInDomain(new Difference(left, right), new Domain(Long.MIN_VALUE, 0));
  }

  /** Adds {@code expr < value}. */
  public Constraint addLessThan(LinearExpr expr, long value) {
    return addLinearExpressionInDomain(expr, new Domain(Long.MIN_VALUE, value - 1));
  }

  /** Adds {@code left < right}. */
  public Constraint addLessThan(LinearExpr left, LinearExpr right) {
    return addLinearExpressionInDomain(new Difference(left, right), new Domain(Long.MIN_VALUE, -1));
  }

  /** Adds {@code left + offset <= right}. */
  public Constraint addLessOrEqualWithOffset(LinearExpr left, LinearExpr right, long offset) {
    return addLinearExpressionInDomain(
        new Difference(left, right), new Domain(Long.MIN_VALUE, -offset));
  }

  /** Adds {@code expr >= value}. */
  public Constraint addGreaterOrEqual(LinearExpr expr, long value) {
    return addLinearExpressionInDomain(expr, new Domain(value, Long.MAX_VALUE));
  }

  /** Adds {@code left >= right}. */
  public Constraint addGreaterOrEqual(LinearExpr left, LinearExpr right) {
    return addLinearExpressionInDomain(new Difference(left, right), new Domain(0, Long.MAX_VALUE));
  }

  /** Adds {@code expr > value}. */
  public Constraint addGreaterThan(LinearExpr expr, long value) {
    return addLinearExpressionInDomain(expr, new Domain(value + 1, Long.MAX_VALUE));
  }

  /** Adds {@code left > right}. */
  public Constraint addGreaterThan(LinearExpr left, LinearExpr right) {
    return addLinearExpressionInDomain(new Difference(left, right), new Domain(1, Long.MAX_VALUE));
  }

  /** Adds {@code left + offset >= right}. */
  public Constraint addGreaterOrEqualWithOffset(LinearExpr left, LinearExpr right, long offset) {
    return addLinearExpressionInDomain(
        new Difference(left, right), new Domain(-offset, Long.MAX_VALUE));
  }

  /** Adds {@code expr != value}. */
  public Constraint addDifferent(LinearExpr expr, long value) {
    return addLinearExpressionInDomain(expr,
        Domain.fromFlatIntervals(
            new long[] {Long.MIN_VALUE, value - 1, value + 1, Long.MAX_VALUE}));
  }

  /** Adds {@code left != right}. */
  public Constraint addDifferent(LinearExpr left, LinearExpr right) {
    return addLinearExpressionInDomain(new Difference(left, right),
        Domain.fromFlatIntervals(new long[] {Long.MIN_VALUE, -1, 1, Long.MAX_VALUE}));
  }

  /** Adds {@code left + offset != right}. */
  public Constraint addDifferentWithOffset(LinearExpr left, LinearExpr right, long offset) {
    return addLinearExpressionInDomain(new Difference(left, right),
        Domain.fromFlatIntervals(
            new long[] {Long.MIN_VALUE, -offset - 1, -offset + 1, Long.MAX_VALUE}));
  }

  // Integer constraints.

  /**
   * Adds {@code AllDifferent(variables)}.
   *
   * <p>This constraint forces all variables to have different values.
   *
   * @param variables a list of integer variables
   * @return an instance of the Constraint class
   */
  public Constraint addAllDifferent(IntVar[] variables) {
    Constraint ct = new Constraint(modelBuilder);
    AllDifferentConstraintProto.Builder allDiff = ct.getBuilder().getAllDiffBuilder();
    for (IntVar var : variables) {
      allDiff.addExprs(getLinearExpressionProtoBuilderFromLinearExpr(var, /*negate=*/false));
    }
    return ct;
  }

  /**
   * Adds {@code AllDifferent(expressions)}.
   *
   * <p>This constraint forces all affine expressions to have different values.
   *
   * @param expressions a list of affine integer expressions
   * @return an instance of the Constraint class
   */
  public Constraint addAllDifferent(LinearExpr[] expressions) {
    Constraint ct = new Constraint(modelBuilder);
    AllDifferentConstraintProto.Builder allDiff = ct.getBuilder().getAllDiffBuilder();
    for (LinearExpr expr : expressions) {
      allDiff.addExprs(getLinearExpressionProtoBuilderFromLinearExpr(expr, /*negate=*/false));
    }
    return ct;
  }

  /** Adds the element constraint: {@code variables[index] == target}. */
  public Constraint addElement(IntVar index, IntVar[] variables, IntVar target) {
    Constraint ct = new Constraint(modelBuilder);
    ElementConstraintProto.Builder element =
        ct.getBuilder().getElementBuilder().setIndex(index.getIndex());
    for (IntVar var : variables) {
      element.addVars(var.getIndex());
    }
    element.setTarget(target.getIndex());
    return ct;
  }

  /** Adds the element constraint: {@code values[index] == target}. */
  public Constraint addElement(IntVar index, long[] values, IntVar target) {
    Constraint ct = new Constraint(modelBuilder);
    ElementConstraintProto.Builder element =
        ct.getBuilder().getElementBuilder().setIndex(index.getIndex());
    for (long v : values) {
      element.addVars(indexFromConstant(v));
    }
    element.setTarget(target.getIndex());
    return ct;
  }

  /** Adds the element constraint: {@code values[index] == target}. */
  public Constraint addElement(IntVar index, int[] values, IntVar target) {
    Constraint ct = new Constraint(modelBuilder);
    ElementConstraintProto.Builder element =
        ct.getBuilder().getElementBuilder().setIndex(index.getIndex());
    for (long v : values) {
      element.addVars(indexFromConstant(v));
    }
    element.setTarget(target.getIndex());
    return ct;
  }

  /**
   * Adds {@code Circuit()}.
   *
   * <p>Adds an empty circuit constraint.
   *
   * <p>A circuit is a unique Hamiltonian path in a subgraph of the total graph. In case a node 'i'
   * is not in the path, then there must be a loop arc {@code 'i -> i'} associated with a true
   * literal. Otherwise this constraint will fail.
   */
  public CircuitConstraint addCircuit() {
    return new CircuitConstraint(modelBuilder);
  }

  /**
   * Adds {@code MultipleCircuit()}.
   *
   * <p>Adds an empty multiple circuit constraint.
   *
   * <p>A multiple circuit is set of cycles in a subgraph of the total graph. The node index by 0
   * must be part of all cycles of length > 1. Each node with index > 0 belongs to exactly one
   * cycle. If such node does not belong in any cycle of length > 1, then there must be a looping
   * arc on this node attached to a literal that will be true. Otherwise, the constraint will fail.
   */
  public MultipleCircuitConstraint addMultipleCircuit() {
    return new MultipleCircuitConstraint(modelBuilder);
  }

  /**
   * Adds {@code AllowedAssignments(variables)}.
   *
   * <p>An AllowedAssignments constraint is a constraint on an array of variables that forces, when
   * all variables are fixed to a single value, that the corresponding list of values is equal to
   * one of the tuples of the tupleList.
   *
   * @param variables a list of variables
   * @return an instance of the TableConstraint class without any tuples. Tuples can be added
   *     directly to the table constraint.
   */
  public TableConstraint addAllowedAssignments(IntVar[] variables) {
    TableConstraint ct = new TableConstraint(modelBuilder);
    TableConstraintProto.Builder table = ct.getBuilder().getTableBuilder();
    for (IntVar var : variables) {
      table.addVars(var.getIndex());
    }
    table.setNegated(false);
    return ct;
  }

  /**
   * Adds {@code ForbiddenAssignments(variables)}.
   *
   * <p>A ForbiddenAssignments constraint is a constraint on an array of variables where the list of
   * impossible combinations is provided in the tuples list.
   *
   * @param variables a list of variables
   * @return an instance of the TableConstraint class without any tuples. Tuples can be added
   *     directly to the table constraint.
   */
  public TableConstraint addForbiddenAssignments(IntVar[] variables) {
    TableConstraint ct = new TableConstraint(modelBuilder);
    TableConstraintProto.Builder table = ct.getBuilder().getTableBuilder();
    for (IntVar var : variables) {
      table.addVars(var.getIndex());
    }
    table.setNegated(true);
    return ct;
  }

  /**
   * Adds an automaton constraint.
   *
   * <p>An automaton constraint takes a list of variables (of size n), an initial state, a set of
   * final states, and a set of transitions that will be added incrementally directly on the
   * returned AutomatonConstraint instance. A transition is a triplet ('tail', 'transition',
   * 'head'), where 'tail' and 'head' are states, and 'transition' is the label of an arc from
   * 'head' to 'tail', corresponding to the value of one variable in the list of variables.
   *
   * <p>This automaton will be unrolled into a flow with n + 1 phases. Each phase contains the
   * possible states of the automaton. The first state contains the initial state. The last phase
   * contains the final states.
   *
   * <p>Between two consecutive phases i and i + 1, the automaton creates a set of arcs. For each
   * transition (tail, label, head), it will add an arc from the state 'tail' of phase i and the
   * state 'head' of phase i + 1. This arc labeled by the value 'label' of the variables
   * 'variables[i]'. That is, this arc can only be selected if 'variables[i]' is assigned the value
   * 'label'.
   *
   * <p>A feasible solution of this constraint is an assignment of variables such that, starting
   * from the initial state in phase 0, there is a path labeled by the values of the variables that
   * ends in one of the final states in the final phase.
   *
   * @param transitionVariables a non empty list of variables whose values correspond to the labels
   *     of the arcs traversed by the automaton
   * @param startingState the initial state of the automaton
   * @param finalStates a non empty list of admissible final states
   * @return an instance of the Constraint class
   */
  public AutomatonConstraint addAutomaton(
      IntVar[] transitionVariables, long startingState, long[] finalStates) {
    AutomatonConstraint ct = new AutomatonConstraint(modelBuilder);
    AutomatonConstraintProto.Builder automaton = ct.getBuilder().getAutomatonBuilder();
    for (IntVar var : transitionVariables) {
      automaton.addVars(var.getIndex());
    }
    automaton.setStartingState(startingState);
    for (long c : finalStates) {
      automaton.addFinalStates(c);
    }
    return ct;
  }

  /**
   * Adds {@code Inverse(variables, inverseVariables)}.
   *
   * <p>An inverse constraint enforces that if 'variables[i]' is assigned a value 'j', then
   * inverseVariables[j] is assigned a value 'i'. And vice versa.
   *
   * @param variables an array of integer variables
   * @param inverseVariables an array of integer variables
   * @return an instance of the Constraint class
   * @throws MismatchedArrayLengths if variables and inverseVariables have different length
   */
  public Constraint addInverse(IntVar[] variables, IntVar[] inverseVariables) {
    if (variables.length != inverseVariables.length) {
      throw new MismatchedArrayLengths("CpModel.addInverse", "variables", "inverseVariables");
    }
    Constraint ct = new Constraint(modelBuilder);
    InverseConstraintProto.Builder inverse = ct.getBuilder().getInverseBuilder();
    for (IntVar var : variables) {
      inverse.addFDirect(var.getIndex());
    }
    for (IntVar var : inverseVariables) {
      inverse.addFInverse(var.getIndex());
    }
    return ct;
  }

  /**
   * Adds a reservoir constraint with optional refill/emptying events.
   *
   * <p>Maintain a reservoir level within bounds. The water level starts at 0, and at any time, it
   * must be within [min_level, max_level].
   *
   * <p>Given an event (time, levelChange, active), if active is true, and if time is assigned a
   * value t, then the level of the reservoir changes by levelChange (which is constant) at time t.
   * Therefore, at any time t:
   *
   * <p>sum(levelChanges[i] * actives[i] if times[i] <= t) in [min_level, max_level]
   *
   * <p>Note that min level must be <= 0, and the max level must be >= 0. Please use fixed
   * level_changes to simulate an initial state.
   *
   * @param minLevel at any time, the level of the reservoir must be greater of equal than the min
   *     level. minLevel must me <= 0.
   * @param maxLevel at any time, the level of the reservoir must be less or equal than the max
   *     level. maxLevel must be >= 0.
   * @return an instance of the ReservoirConstraint class
   * @throws IllegalArgumentException if minLevel > 0
   * @throws IllegalArgumentException if maxLevel < 0
   */
  public ReservoirConstraint addReservoirConstraint(long minLevel, long maxLevel) {
    if (minLevel > 0) {
      throw new IllegalArgumentException("CpModel.addReservoirConstraint: minLevel must be <= 0");
    }
    if (maxLevel < 0) {
      throw new IllegalArgumentException("CpModel.addReservoirConstraint: maxLevel must be >= 0");
    }
    ReservoirConstraint ct = new ReservoirConstraint(this);
    ReservoirConstraintProto.Builder reservoir = ct.getBuilder().getReservoirBuilder();
    reservoir.setMinLevel(minLevel).setMaxLevel(maxLevel);
    return ct;
  }

  /** Adds {@code var == i + offset <=> booleans[i] == true for all i in [0, booleans.length)}. */
  public void addMapDomain(IntVar var, Literal[] booleans, long offset) {
    for (int i = 0; i < booleans.length; ++i) {
      addEquality(var, offset + i).onlyEnforceIf(booleans[i]);
      addDifferent(var, offset + i).onlyEnforceIf(booleans[i].not());
    }
  }

  /** Adds {@code target == Min(vars)}. */
  public Constraint addMinEquality(LinearExpr target, IntVar[] vars) {
    Constraint ct = new Constraint(modelBuilder);
    LinearArgumentProto.Builder linMax = ct.getBuilder().getLinMaxBuilder();
    linMax.setTarget(getLinearExpressionProtoBuilderFromLinearExpr(target, /*negate=*/true));
    for (IntVar var : vars) {
      linMax.addExprs(getLinearExpressionProtoBuilderFromLinearExpr(var, /*negate=*/true));
    }
    return ct;
  }

  /** Adds {@code target == Min(exprs)}. */
  public Constraint addMinEquality(LinearExpr target, LinearExpr[] exprs) {
    Constraint ct = new Constraint(modelBuilder);
    LinearArgumentProto.Builder linMax = ct.getBuilder().getLinMaxBuilder();
    linMax.setTarget(getLinearExpressionProtoBuilderFromLinearExpr(target, /*negate=*/true));
    for (LinearExpr expr : exprs) {
      linMax.addExprs(getLinearExpressionProtoBuilderFromLinearExpr(expr, /*negate=*/true));
    }
    return ct;
  }

  /** Adds {@code target == Max(vars)}. */
  public Constraint addMaxEquality(LinearExpr target, IntVar[] vars) {
    Constraint ct = new Constraint(modelBuilder);
    LinearArgumentProto.Builder linMax = ct.getBuilder().getLinMaxBuilder();
    linMax.setTarget(getLinearExpressionProtoBuilderFromLinearExpr(target, /*negate=*/false));
    for (IntVar var : vars) {
      linMax.addExprs(getLinearExpressionProtoBuilderFromLinearExpr(var, /*negate=*/false));
    }
    return ct;
  }

  /** Adds {@code target == Max(exprs)}. */
  public Constraint addMaxEquality(LinearExpr target, LinearExpr[] exprs) {
    Constraint ct = new Constraint(modelBuilder);
    LinearArgumentProto.Builder linMax = ct.getBuilder().getLinMaxBuilder();
    linMax.setTarget(getLinearExpressionProtoBuilderFromLinearExpr(target, /*negate=*/false));
    for (LinearExpr expr : exprs) {
      linMax.addExprs(getLinearExpressionProtoBuilderFromLinearExpr(expr, /*negate=*/false));
    }
    return ct;
  }

  /** Adds {@code target == num / denom}, rounded towards 0. */
  public Constraint addDivisionEquality(LinearExpr target, LinearExpr num, LinearExpr denom) {
    Constraint ct = new Constraint(modelBuilder);
    ct.getBuilder()
        .getIntDivBuilder()
        .setTarget(getLinearExpressionProtoBuilderFromLinearExpr(target, /*negate=*/false))
        .addExprs(getLinearExpressionProtoBuilderFromLinearExpr(num, /*negate=*/false))
        .addExprs(getLinearExpressionProtoBuilderFromLinearExpr(denom, /*negate=*/false));
    return ct;
  }

  /** Adds {@code target == Abs(expr)}. */
  public Constraint addAbsEquality(LinearExpr target, LinearExpr expr) {
    Constraint ct = new Constraint(modelBuilder);
    LinearArgumentProto.Builder linMax = ct.getBuilder().getLinMaxBuilder();
    linMax.setTarget(getLinearExpressionProtoBuilderFromLinearExpr(target, /*negate=*/false));
    linMax.addExprs(getLinearExpressionProtoBuilderFromLinearExpr(expr, /*negate=*/false));
    linMax.addExprs(getLinearExpressionProtoBuilderFromLinearExpr(expr, /*negate=*/true));
    return ct;
  }

  /** Adds {@code target == var % mod}. */
  public Constraint addModuloEquality(LinearExpr target, LinearExpr var, LinearExpr mod) {
    Constraint ct = new Constraint(modelBuilder);
    ct.getBuilder()
        .getIntModBuilder()
        .setTarget(getLinearExpressionProtoBuilderFromLinearExpr(target, /*negate=*/false))
        .addExprs(getLinearExpressionProtoBuilderFromLinearExpr(var, /*negate=*/false))
        .addExprs(getLinearExpressionProtoBuilderFromLinearExpr(mod, /*negate=*/false));
    return ct;
  }

  /** Adds {@code target == var % mod}. */
  public Constraint addModuloEquality(LinearExpr target, LinearExpr var, long mod) {
    Constraint ct = new Constraint(modelBuilder);
    ct.getBuilder()
        .getIntModBuilder()
        .setTarget(getLinearExpressionProtoBuilderFromLinearExpr(target, /*negate=*/false))
        .addExprs(getLinearExpressionProtoBuilderFromLinearExpr(var, /*negate=*/false))
        .addExprs(getLinearExpressionProtoBuilderFromLong(mod));
    return ct;
  }

  /** Adds {@code target == Product(vars)}. */
  public Constraint addMultiplicationEquality(LinearExpr target, IntVar[] vars) {
    Constraint ct = new Constraint(modelBuilder);
    LinearArgumentProto.Builder intProd = ct.getBuilder().getIntProdBuilder();
    intProd.setTarget(getLinearExpressionProtoBuilderFromLinearExpr(target, /*negate=*/false));
    for (IntVar var : vars) {
      intProd.addExprs(getLinearExpressionProtoBuilderFromLinearExpr(var, /*negate=*/false));
    }
    return ct;
  }

  /** Adds {@code target == Product(exprs)}. */
  public Constraint addMultiplicationEquality(LinearExpr target, LinearExpr[] exprs) {
    Constraint ct = new Constraint(modelBuilder);
    LinearArgumentProto.Builder intProd = ct.getBuilder().getIntProdBuilder();
    intProd.setTarget(getLinearExpressionProtoBuilderFromLinearExpr(target, /*negate=*/false));
    for (LinearExpr expr : exprs) {
      intProd.addExprs(getLinearExpressionProtoBuilderFromLinearExpr(expr, /*negate=*/false));
    }
    return ct;
  }

  /** Adds {@code target == left * right}. */
  public Constraint addMultiplicationEquality(
      LinearExpr target, LinearExpr left, LinearExpr right) {
    Constraint ct = new Constraint(modelBuilder);
    LinearArgumentProto.Builder intProd = ct.getBuilder().getIntProdBuilder();
    intProd.setTarget(getLinearExpressionProtoBuilderFromLinearExpr(target, /*negate=*/false));
    intProd.addExprs(getLinearExpressionProtoBuilderFromLinearExpr(left, /*negate=*/false));
    intProd.addExprs(getLinearExpressionProtoBuilderFromLinearExpr(right, /*negate=*/false));
    return ct;
  }

  // Scheduling support.

  /**
   * Creates an interval variable from three affine expressions start, size, and end.
   *
   * <p>An interval variable is a constraint, that is itself used in other constraints like
   * NoOverlap.
   *
   * <p>Internally, it ensures that {@code start + size == end}.
   *
   * @param start the start of the interval. It needs to be an affine or constant expression.
   * @param size the size of the interval. It needs to be an affine or constant expression.
   * @param end the end of the interval. It needs to be an affine or constant expression.
   * @param name the name of the interval variable
   * @return An IntervalVar object
   */
  public IntervalVar newIntervalVar(
      LinearExpr start, LinearExpr size, LinearExpr end, String name) {
    addEquality(new Sum(start, size), end);
    return new IntervalVar(modelBuilder,
        getLinearExpressionProtoBuilderFromLinearExpr(start, /*negate=*/false),
        getLinearExpressionProtoBuilderFromLinearExpr(size, /*negate=*/false),
        getLinearExpressionProtoBuilderFromLinearExpr(end, /*negate=*/false), name);
  }

  /**
   * Creates an interval variable from an affine expression start, and a fixed size.
   *
   * <p>An interval variable is a constraint, that is itself used in other constraints like
   * NoOverlap.
   *
   * @param start the start of the interval. It needs to be an affine or constant expression.
   * @param size the fixed size of the interval.
   * @param name the name of the interval variable.
   * @return An IntervalVar object
   */
  public IntervalVar newFixedSizeIntervalVar(LinearExpr start, long size, String name) {
    return new IntervalVar(modelBuilder,
        getLinearExpressionProtoBuilderFromLinearExpr(start, /*negate=*/false),
        getLinearExpressionProtoBuilderFromLong(size),
        getLinearExpressionProtoBuilderFromLinearExpr(new Sum(start, size), /*negate=*/false),
        name);
  }

  /** Creates a fixed interval from its start and its size. */
  public IntervalVar newFixedInterval(long start, long size, String name) {
    return new IntervalVar(modelBuilder, getLinearExpressionProtoBuilderFromLong(start),
        getLinearExpressionProtoBuilderFromLong(size),
        getLinearExpressionProtoBuilderFromLong(start + size), name);
  }

  /**
   * Creates an optional interval variable from three affine expressions start, size, end, and
   * isPresent.
   *
   * <p>An optional interval variable is a constraint, that is itself used in other constraints like
   * NoOverlap. This constraint is protected by an {@code isPresent} literal that indicates if it is
   * active or not.
   *
   * <p>Internally, it ensures that {@code isPresent => start + size == end}.
   *
   * @param start the start of the interval. It needs to be an affine or constant expression.
   * @param size the size of the interval. It needs to be an affine or constant expression.
   * @param end the end of the interval. It needs to be an affine or constant expression.
   * @param isPresent a literal that indicates if the interval is active or not. A inactive interval
   *     is simply ignored by all constraints.
   * @param name The name of the interval variable
   * @return an IntervalVar object
   */
  public IntervalVar newOptionalIntervalVar(
      LinearExpr start, LinearExpr size, LinearExpr end, Literal isPresent, String name) {
    addEquality(new Sum(start, size), end).onlyEnforceIf(isPresent);
    return new IntervalVar(modelBuilder,
        getLinearExpressionProtoBuilderFromLinearExpr(start, /*negate=*/false),
        getLinearExpressionProtoBuilderFromLinearExpr(size, /*negate=*/false),
        getLinearExpressionProtoBuilderFromLinearExpr(end, /*negate=*/false), isPresent.getIndex(),
        name);
  }

  /**
   * Creates an optional interval variable from an affine expression start, and a fixed size.
   *
   * <p>An interval variable is a constraint, that is itself used in other constraints like
   * NoOverlap.
   *
   * @param start the start of the interval. It needs to be an affine or constant expression.
   * @param size the fixed size of the interval.
   * @param isPresent a literal that indicates if the interval is active or not. A inactive interval
   *     is simply ignored by all constraints.
   * @param name the name of the interval variable.
   * @return An IntervalVar object
   */
  public IntervalVar newOptionalFixedSizeIntervalVar(
      LinearExpr start, long size, Literal isPresent, String name) {
    return new IntervalVar(modelBuilder,
        getLinearExpressionProtoBuilderFromLinearExpr(start, /*negate=*/false),
        getLinearExpressionProtoBuilderFromLong(size),
        getLinearExpressionProtoBuilderFromLinearExpr(new Sum(start, size), /*negate=*/false),
        isPresent.getIndex(), name);
  }

  /** Creates an optional fixed interval from start and size, and an isPresent literal. */
  public IntervalVar newOptionalFixedInterval(
      long start, long size, Literal isPresent, String name) {
    return new IntervalVar(modelBuilder, getLinearExpressionProtoBuilderFromLong(start),
        getLinearExpressionProtoBuilderFromLong(size),
        getLinearExpressionProtoBuilderFromLong(start + size), isPresent.getIndex(), name);
  }

  /**
   * Adds {@code NoOverlap(intervalVars)}.
   *
   * <p>A NoOverlap constraint ensures that all present intervals do not overlap in time.
   *
   * @param intervalVars the list of interval variables to constrain
   * @return an instance of the Constraint class
   */
  public Constraint addNoOverlap(IntervalVar[] intervalVars) {
    Constraint ct = new Constraint(modelBuilder);
    NoOverlapConstraintProto.Builder noOverlap = ct.getBuilder().getNoOverlapBuilder();
    for (IntervalVar var : intervalVars) {
      noOverlap.addIntervals(var.getIndex());
    }
    return ct;
  }

  /**
   * Adds {@code NoOverlap2D(xIntervals, yIntervals)}.
   *
   * <p>A NoOverlap2D constraint ensures that all present rectangles do not overlap on a plan. Each
   * rectangle is aligned with the X and Y axis, and is defined by two intervals which represent its
   * projection onto the X and Y axis.
   *
   * <p>Furthermore, one box is optional if at least one of the x or y interval is optional.
   *
   * @return an instance of the NoOverlap2dConstraint class. This class allows adding rectangles
   *     incrementally.
   */
  public NoOverlap2dConstraint addNoOverlap2D() {
    return new NoOverlap2dConstraint(modelBuilder);
  }

  /**
   * Adds {@code Cumulative(capacity)}.
   *
   * <p>This constraint enforces that:
   *
   * <p>{@code forall t: sum(demands[i] if (start(intervals[t]) <= t < end(intervals[t])) and (t is
   * present)) <= capacity}.
   *
   * @param capacity the maximum capacity of the cumulative constraint. It must be a positive affine
   *     expression.
   * @return an instance of the CumulativeConstraint class. this class allows adding (interval,
   *     demand) pairs incrementally.
   */
  public CumulativeConstraint addCumulative(LinearExpr capacity) {
    CumulativeConstraint ct = new CumulativeConstraint(this);
    CumulativeConstraintProto.Builder cumul = ct.getBuilder().getCumulativeBuilder();
    cumul.setCapacity(getLinearExpressionProtoBuilderFromLinearExpr(capacity, false));
    return ct;
  }

  /**
   * Adds {@code Cumulative(capacity)}.
   *
   * @see #addCumulative(LinearExpr capacity)
   */
  public CumulativeConstraint addCumulative(long capacity) {
    CumulativeConstraint ct = new CumulativeConstraint(this);
    CumulativeConstraintProto.Builder cumul = ct.getBuilder().getCumulativeBuilder();
    cumul.setCapacity(getLinearExpressionProtoBuilderFromLong(capacity));
    return ct;
  }

  /** Adds hinting to a variable */
  public void addHint(IntVar var, long value) {
    modelBuilder.getSolutionHintBuilder().addVars(var.getIndex());
    modelBuilder.getSolutionHintBuilder().addValues(value);
  }

  /** Remove all solution hints */
  public void clearHints() {
    modelBuilder.clearSolutionHint();
  }

  /** Adds a literal to the model as assumption */
  public void addAssumption(Literal lit) {
    modelBuilder.addAssumptions(lit.getIndex());
  }

  /** Adds multiple literals to the model as assumptions */
  public void addAssumptions(Literal[] literals) {
    for (Literal lit : literals) {
      addAssumption(lit);
    }
  }

  /** Remove all assumptions from the model */
  public void clearAssumptions() {
    modelBuilder.clearAssumptions();
  }

  // Objective.

  /** Adds a minimization objective of a linear expression. */
  public void minimize(LinearExpr expr) {
    CpObjectiveProto.Builder obj = modelBuilder.getObjectiveBuilder();
    for (int i = 0; i < expr.numElements(); ++i) {
      obj.addVars(expr.getVariable(i).getIndex()).addCoeffs(expr.getCoefficient(i));
    }
    obj.setOffset(expr.getOffset());
  }

  public void minimize(DoubleLinearExpr expr) {
    FloatObjectiveProto.Builder obj = modelBuilder.getFloatingPointObjectiveBuilder();
    for (int i = 0; i < expr.numElements(); ++i) {
      obj.addVars(expr.getVariable(i).getIndex()).addCoeffs(expr.getCoefficient(i));
    }
    obj.setOffset(expr.getOffset()).setMaximize(false);
  }

  /** Adds a maximization objective of a linear expression. */
  public void maximize(LinearExpr expr) {
    CpObjectiveProto.Builder obj = modelBuilder.getObjectiveBuilder();
    for (int i = 0; i < expr.numElements(); ++i) {
      obj.addVars(expr.getVariable(i).getIndex()).addCoeffs(-expr.getCoefficient(i));
    }
    obj.setOffset(-expr.getOffset());
    obj.setScalingFactor(-1.0);
  }

  public void maximize(DoubleLinearExpr expr) {
    FloatObjectiveProto.Builder obj = modelBuilder.getFloatingPointObjectiveBuilder();
    for (int i = 0; i < expr.numElements(); ++i) {
      obj.addVars(expr.getVariable(i).getIndex()).addCoeffs(expr.getCoefficient(i));
    }
    obj.setOffset(expr.getOffset()).setMaximize(true);
  }

  // DecisionStrategy

  /** Adds {@code DecisionStrategy(variables, varStr, domStr)}. */
  public void addDecisionStrategy(IntVar[] variables,
      DecisionStrategyProto.VariableSelectionStrategy varStr,
      DecisionStrategyProto.DomainReductionStrategy domStr) {
    DecisionStrategyProto.Builder ds = modelBuilder.addSearchStrategyBuilder();
    for (IntVar var : variables) {
      ds.addVariables(var.getIndex());
    }
    ds.setVariableSelectionStrategy(varStr).setDomainReductionStrategy(domStr);
  }

  /** Returns some statistics on model as a string. */
  public String modelStats() {
    return CpSatHelper.modelStats(model());
  }

  /** Returns a non empty string explaining the issue if the model is invalid. */
  public String validate() {
    return CpSatHelper.validateModel(model());
  }

  /**
   * Write the model as a protocol buffer to 'file'.
   *
   * @param file file to write the model to. If the filename ends with 'txt', the
   *    model will be written as a text file, otherwise, the binary format will be used.
   *
   * @return true if the model was correctly written.
   */
  public Boolean exportToFile(String file) {
    return CpSatHelper.writeModelToFile(model(), file);
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
    IntVar constVar = newConstant(constant);
    return constVar.getIndex();
  }

  LinearExpressionProto.Builder getLinearExpressionProtoBuilderFromLinearExpr(
      LinearExpr expr, boolean negate) {
    LinearExpressionProto.Builder builder = LinearExpressionProto.newBuilder();
    final int numVariables = expr.numElements();
    final long mult = negate ? -1 : 1;
    for (int i = 0; i < numVariables; ++i) {
      builder.addVars(expr.getVariable(i).getIndex());
      builder.addCoeffs(expr.getCoefficient(i) * mult);
    }
    builder.setOffset(expr.getOffset() * mult);
    return builder;
  }

  LinearExpressionProto.Builder getLinearExpressionProtoBuilderFromLong(long value) {
    LinearExpressionProto.Builder builder = LinearExpressionProto.newBuilder();
    builder.setOffset(value);
    return builder;
  }

  // Getters.

  public CpModelProto model() {
    return modelBuilder.build();
  }

  public int negated(int index) {
    return -index - 1;
  }

  /** Returns the model builder. */
  public CpModelProto.Builder getBuilder() {
    return modelBuilder;
  }

  private final CpModelProto.Builder modelBuilder;
  private final Map<Long, IntVar> constantMap;
}
