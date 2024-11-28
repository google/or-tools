// Copyright 2010-2024 Google LLC
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
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.Map;

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

  public CpModel getClone() {
    CpModel clone = new CpModel();
    clone.modelBuilder.mergeFrom(modelBuilder.build());
    clone.constantMap.clear();
    clone.constantMap.putAll(constantMap);
    return clone;
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
  public BoolVar newBoolVar(String name) {
    return new BoolVar(modelBuilder, new Domain(0, 1), name);
  }

  /** Creates a constant variable. */
  public IntVar newConstant(long value) {
    if (constantMap.containsKey(value)) {
      return new IntVar(modelBuilder, constantMap.get(value));
    }
    IntVar cste = new IntVar(modelBuilder, new Domain(value), ""); // bounds and name.
    constantMap.put(value, cste.getIndex());
    return cste;
  }

  /** Returns the true literal. */
  public Literal trueLiteral() {
    if (constantMap.containsKey(1L)) {
      return new BoolVar(modelBuilder, constantMap.get(1L));
    }
    BoolVar cste = new BoolVar(modelBuilder, new Domain(1), ""); // bounds and name.
    constantMap.put(1L, cste.getIndex());
    return cste;
  }

  /** Returns the false literal. */
  public Literal falseLiteral() {
    if (constantMap.containsKey(0L)) {
      return new BoolVar(modelBuilder, constantMap.get(0L));
    }
    BoolVar cste = new BoolVar(modelBuilder, new Domain(0), ""); // bounds and name.
    constantMap.put(0L, cste.getIndex());
    return cste;
  }

  /** Rebuilds a Boolean variable from an index. Useful after cloning a model. */
  public BoolVar getBoolVarFromProtoIndex(int index) {
    return new BoolVar(modelBuilder, index);
  }

  /** Rebuilds an integer variable from an index. Useful after cloning a model. */
  public IntVar getIntVarFromProtoIndex(int index) {
    return new IntVar(modelBuilder, index);
  }

  // Boolean Constraints.

  /** Adds {@code Or(literals) == true}. */
  public Constraint addBoolOr(Literal[] literals) {
    return addBoolOr(Arrays.asList(literals));
  }

  /** Adds {@code Or(literals) == true}. */
  public Constraint addBoolOr(Iterable<Literal> literals) {
    Constraint ct = new Constraint(modelBuilder);
    BoolArgumentProto.Builder boolOr = ct.getBuilder().getBoolOrBuilder();
    for (Literal lit : literals) {
      boolOr.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /** Same as addBoolOr. {@code Sum(literals) >= 1}. */
  public Constraint addAtLeastOne(Literal[] literals) {
    return addBoolOr(Arrays.asList(literals));
  }

  /** Same as addBoolOr. {@code Sum(literals) >= 1}. */
  public Constraint addAtLeastOne(Iterable<Literal> literals) {
    return addBoolOr(literals);
  }

  /** Adds {@code AtMostOne(literals): Sum(literals) <= 1}. */
  public Constraint addAtMostOne(Literal[] literals) {
    return addAtMostOne(Arrays.asList(literals));
  }

  /** Adds {@code AtMostOne(literals): Sum(literals) <= 1}. */
  public Constraint addAtMostOne(Iterable<Literal> literals) {
    Constraint ct = new Constraint(modelBuilder);
    BoolArgumentProto.Builder atMostOne = ct.getBuilder().getAtMostOneBuilder();
    for (Literal lit : literals) {
      atMostOne.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /** Adds {@code ExactlyOne(literals): Sum(literals) == 1}. */
  public Constraint addExactlyOne(Literal[] literals) {
    return addExactlyOne(Arrays.asList(literals));
  }

  /** Adds {@code ExactlyOne(literals): Sum(literals) == 1}. */
  public Constraint addExactlyOne(Iterable<Literal> literals) {
    Constraint ct = new Constraint(modelBuilder);
    BoolArgumentProto.Builder exactlyOne = ct.getBuilder().getExactlyOneBuilder();
    for (Literal lit : literals) {
      exactlyOne.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /** Adds {@code And(literals) == true}. */
  public Constraint addBoolAnd(Literal[] literals) {
    return addBoolAnd(Arrays.asList(literals));
  }

  /** Adds {@code And(literals) == true}. */
  public Constraint addBoolAnd(Iterable<Literal> literals) {
    Constraint ct = new Constraint(modelBuilder);
    BoolArgumentProto.Builder boolAnd = ct.getBuilder().getBoolAndBuilder();
    for (Literal lit : literals) {
      boolAnd.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /** Adds {@code XOr(literals) == true}. */
  public Constraint addBoolXor(Literal[] literals) {
    return addBoolXor(Arrays.asList(literals));
  }

  /** Adds {@code XOr(literals) == true}. */
  public Constraint addBoolXor(Iterable<Literal> literals) {
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
  public Constraint addLinearExpressionInDomain(LinearArgument expr, Domain domain) {
    Constraint ct = new Constraint(modelBuilder);
    LinearConstraintProto.Builder lin = ct.getBuilder().getLinearBuilder();
    final LinearExpr e = expr.build();
    for (int i = 0; i < e.numElements(); ++i) {
      lin.addVars(e.getVariableIndex(i)).addCoeffs(e.getCoefficient(i));
    }
    long offset = e.getOffset();
    for (long b : domain.flattenedIntervals()) {
      if (b == Long.MIN_VALUE || b == Long.MAX_VALUE) {
        lin.addDomain(b);
      } else {
        lin.addDomain(b - offset);
      }
    }
    return ct;
  }

  /** Adds {@code lb <= expr <= ub}. */
  public Constraint addLinearConstraint(LinearArgument expr, long lb, long ub) {
    return addLinearExpressionInDomain(expr, new Domain(lb, ub));
  }

  /** Adds {@code expr == value}. */
  public Constraint addEquality(LinearArgument expr, long value) {
    return addLinearExpressionInDomain(expr, new Domain(value));
  }

  /** Adds {@code left == right}. */
  public Constraint addEquality(LinearArgument left, LinearArgument right) {
    LinearExprBuilder difference = LinearExpr.newBuilder();
    difference.addTerm(left, 1);
    difference.addTerm(right, -1);
    return addLinearExpressionInDomain(difference, new Domain(0));
  }

  /** Adds {@code expr <= value}. */
  public Constraint addLessOrEqual(LinearArgument expr, long value) {
    return addLinearExpressionInDomain(expr, new Domain(Long.MIN_VALUE, value));
  }

  /** Adds {@code left <= right}. */
  public Constraint addLessOrEqual(LinearArgument left, LinearArgument right) {
    LinearExprBuilder difference = LinearExpr.newBuilder();
    difference.addTerm(left, 1);
    difference.addTerm(right, -1);
    return addLinearExpressionInDomain(difference, new Domain(Long.MIN_VALUE, 0));
  }

  /** Adds {@code expr < value}. */
  public Constraint addLessThan(LinearArgument expr, long value) {
    return addLinearExpressionInDomain(expr, new Domain(Long.MIN_VALUE, value - 1));
  }

  /** Adds {@code left < right}. */
  public Constraint addLessThan(LinearArgument left, LinearArgument right) {
    LinearExprBuilder difference = LinearExpr.newBuilder();
    difference.addTerm(left, 1);
    difference.addTerm(right, -1);
    return addLinearExpressionInDomain(difference, new Domain(Long.MIN_VALUE, -1));
  }

  /** Adds {@code expr >= value}. */
  public Constraint addGreaterOrEqual(LinearArgument expr, long value) {
    return addLinearExpressionInDomain(expr, new Domain(value, Long.MAX_VALUE));
  }

  /** Adds {@code left >= right}. */
  public Constraint addGreaterOrEqual(LinearArgument left, LinearArgument right) {
    LinearExprBuilder difference = LinearExpr.newBuilder();
    difference.addTerm(left, 1);
    difference.addTerm(right, -1);
    return addLinearExpressionInDomain(difference, new Domain(0, Long.MAX_VALUE));
  }

  /** Adds {@code expr > value}. */
  public Constraint addGreaterThan(LinearArgument expr, long value) {
    return addLinearExpressionInDomain(expr, new Domain(value + 1, Long.MAX_VALUE));
  }

  /** Adds {@code left > right}. */
  public Constraint addGreaterThan(LinearArgument left, LinearArgument right) {
    LinearExprBuilder difference = LinearExpr.newBuilder();
    difference.addTerm(left, 1);
    difference.addTerm(right, -1);
    return addLinearExpressionInDomain(difference, new Domain(1, Long.MAX_VALUE));
  }

  /** Adds {@code expr != value}. */
  public Constraint addDifferent(LinearArgument expr, long value) {
    return addLinearExpressionInDomain(expr,
        Domain.fromFlatIntervals(
            new long[] {Long.MIN_VALUE, value - 1, value + 1, Long.MAX_VALUE}));
  }

  /** Adds {@code left != right}. */
  public Constraint addDifferent(LinearArgument left, LinearArgument right) {
    LinearExprBuilder difference = LinearExpr.newBuilder();
    difference.addTerm(left, 1);
    difference.addTerm(right, -1);
    return addLinearExpressionInDomain(
        difference, Domain.fromFlatIntervals(new long[] {Long.MIN_VALUE, -1, 1, Long.MAX_VALUE}));
  }

  // Integer constraints.

  /**
   * Adds {@code AllDifferent(expressions)}.
   *
   * <p>This constraint forces all affine expressions to have different values.
   *
   * @param expressions a list of affine integer expressions
   * @return an instance of the Constraint class
   */
  public Constraint addAllDifferent(LinearArgument[] expressions) {
    return addAllDifferent(Arrays.asList(expressions));
  }

  /**
   * Adds {@code AllDifferent(expressions)}.
   *
   * @see addAllDifferent(LinearArgument[]).
   */
  public Constraint addAllDifferent(Iterable<? extends LinearArgument> expressions) {
    Constraint ct = new Constraint(modelBuilder);
    AllDifferentConstraintProto.Builder allDiff = ct.getBuilder().getAllDiffBuilder();
    for (LinearArgument expr : expressions) {
      allDiff.addExprs(getLinearExpressionProtoBuilderFromLinearArgument(expr, /*negate=*/false));
    }
    return ct;
  }

  /** Adds the element constraint: {@code expressions[index] == target}. */
  public Constraint addElement(
      LinearArgument index, LinearArgument[] expressions, LinearArgument target) {
    Constraint ct = new Constraint(modelBuilder);
    ElementConstraintProto.Builder element = ct.getBuilder().getElementBuilder().setLinearIndex(
        getLinearExpressionProtoBuilderFromLinearArgument(index, /* negate= */ false));
    for (LinearArgument expr : expressions) {
      element.addExprs(
          getLinearExpressionProtoBuilderFromLinearArgument(expr, /* negate= */ false));
    }
    element.setLinearTarget(
        getLinearExpressionProtoBuilderFromLinearArgument(target, /* negate= */ false));
    return ct;
  }

  /** Adds the element constraint: {@code expressions[index] == target}. */
  public Constraint addElement(
      LinearArgument index, Iterable<? extends LinearArgument> expressions, LinearArgument target) {
    Constraint ct = new Constraint(modelBuilder);
    ElementConstraintProto.Builder element = ct.getBuilder().getElementBuilder().setLinearIndex(
        getLinearExpressionProtoBuilderFromLinearArgument(index, /* negate= */ false));
    for (LinearArgument expr : expressions) {
      element.addExprs(
          getLinearExpressionProtoBuilderFromLinearArgument(expr, /* negate= */ false));
    }
    element.setLinearTarget(
        getLinearExpressionProtoBuilderFromLinearArgument(target, /* negate= */ false));
    return ct;
  }

  /** Adds the element constraint: {@code values[index] == target}. */
  public Constraint addElement(LinearArgument index, long[] values, LinearArgument target) {
    Constraint ct = new Constraint(modelBuilder);
    ElementConstraintProto.Builder element = ct.getBuilder().getElementBuilder().setLinearIndex(
        getLinearExpressionProtoBuilderFromLinearArgument(index, /* negate= */ false));
    for (long v : values) {
      element.addExprs(LinearExpressionProto.newBuilder().setOffset(v));
    }
    element.setLinearTarget(
        getLinearExpressionProtoBuilderFromLinearArgument(target, /* negate= */ false));
    return ct;
  }

  /** Adds the element constraint: {@code values[index] == target}. */
  public Constraint addElement(LinearArgument index, int[] values, LinearArgument target) {
    Constraint ct = new Constraint(modelBuilder);
    ElementConstraintProto.Builder element = ct.getBuilder().getElementBuilder().setLinearIndex(
        getLinearExpressionProtoBuilderFromLinearArgument(index, /* negate= */ false));
    for (long v : values) {
      element.addExprs(LinearExpressionProto.newBuilder().setOffset(v));
    }
    element.setLinearTarget(
        getLinearExpressionProtoBuilderFromLinearArgument(target, /* negate= */ false));
    return ct;
  }

  /**
   * Adds {@code Circuit()}.
   *
   * <p>Adds an empty circuit constraint.
   *
   * <p>A circuit is a unique Hamiltonian circuit in a subgraph of the total graph. In case a node
   * 'i' is not in the circuit, then there must be a loop arc {@code 'i -> i'} associated with a
   * true literal. Otherwise this constraint will fail.
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
   * Adds {@code AllowedAssignments(expressions)}.
   *
   * <p>An AllowedAssignments constraint is a constraint on an array of affine expressions that
   * forces, when all expressions are fixed to a single value, that the corresponding list of values
   * is equal to one of the tuples of the tupleList.
   *
   * @param expressions a list of affine expressions (a * var + b)
   * @return an instance of the TableConstraint class without any tuples. Tuples can be added
   *     directly to the table constraint.
   */
  public TableConstraint addAllowedAssignments(LinearArgument[] expressions) {
    return addAllowedAssignments(Arrays.asList(expressions));
  }

  /**
   * Adds {@code AllowedAssignments(expressions)}.
   *
   * @see addAllowedAssignments(LinearArgument[] expressions)
   */
  public TableConstraint addAllowedAssignments(Iterable<? extends LinearArgument> expressions) {
    TableConstraint ct = new TableConstraint(modelBuilder);
    TableConstraintProto.Builder table = ct.getBuilder().getTableBuilder();
    for (LinearArgument expr : expressions) {
      table.addExprs(getLinearExpressionProtoBuilderFromLinearArgument(expr, /* negate= */ false));
    }
    table.setNegated(false);
    return ct;
  }

  /**
   * Adds {@code ForbiddenAssignments(expressions)}.
   *
   * <p>A ForbiddenAssignments constraint is a constraint on an array of affine expressions where
   * the list of impossible combinations is provided in the tuples list.
   *
   * @param expressions a list of affine expressions (a * var + b)
   * @return an instance of the TableConstraint class without any tuples. Tuples can be added
   *     directly to the table constraint.
   */
  public TableConstraint addForbiddenAssignments(LinearArgument[] expressions) {
    return addForbiddenAssignments(Arrays.asList(expressions));
  }

  /**
   * Adds {@code ForbiddenAssignments(expressions)}.
   *
   * @see addForbiddenAssignments(LinearArgument[] expressions)
   */
  public TableConstraint addForbiddenAssignments(Iterable<? extends LinearArgument> expressions) {
    TableConstraint ct = new TableConstraint(modelBuilder);
    TableConstraintProto.Builder table = ct.getBuilder().getTableBuilder();
    for (LinearArgument expr : expressions) {
      table.addExprs(getLinearExpressionProtoBuilderFromLinearArgument(expr, /* negate= */ false));
    }
    table.setNegated(true);
    return ct;
  }

  /**
   * Adds an automaton constraint.
   *
   * <p>An automaton constraint takes a list of affine expressions (of size n), an initial state, a
   * set of final states, and a set of transitions that will be added incrementally directly on the
   * returned AutomatonConstraint instance. A transition is a triplet ('tail', 'transition',
   * 'head'), where 'tail' and 'head' are states, and 'transition' is the label of an arc from
   * 'head' to 'tail', corresponding to the value of one expression in the list of expressions.
   *
   * <p>This automaton will be unrolled into a flow with n + 1 phases. Each phase contains the
   * possible states of the automaton. The first state contains the initial state. The last phase
   * contains the final states.
   *
   * <p>Between two consecutive phases i and i + 1, the automaton creates a set of arcs. For each
   * transition (tail, label, head), it will add an arc from the state 'tail' of phase i and the
   * state 'head' of phase i + 1. This arc labeled by the value 'label' of the expression
   * 'expressions[i]'. That is, this arc can only be selected if 'expressions[i]' is assigned the
   * value 'label'.
   *
   * <p>A feasible solution of this constraint is an assignment of expressions such that, starting
   * from the initial state in phase 0, there is a path labeled by the values of the expressions
   * that ends in one of the final states in the final phase.
   *
   * @param transitionExpressions a non empty list of affine expressions (a * var + b) whose values
   *     correspond to the labels of the arcs traversed by the automaton
   * @param startingState the initial state of the automaton
   * @param finalStates a non empty list of admissible final states
   * @return an instance of the Constraint class
   */
  public AutomatonConstraint addAutomaton(
      LinearArgument[] transitionExpressions, long startingState, long[] finalStates) {
    AutomatonConstraint ct = new AutomatonConstraint(modelBuilder);
    AutomatonConstraintProto.Builder automaton = ct.getBuilder().getAutomatonBuilder();
    for (LinearArgument expr : transitionExpressions) {
      automaton.addExprs(
          getLinearExpressionProtoBuilderFromLinearArgument(expr, /* negate= */ false));
    }
    automaton.setStartingState(startingState);
    for (long c : finalStates) {
      automaton.addFinalStates(c);
    }
    return ct;
  }

  /**
   * Adds an automaton constraint.
   *
   * @see addAutomaton(LinearArgument[] transitionExpressions, long startingState, long[]
   *     finalStates)
   */
  public AutomatonConstraint addAutomaton(Iterable<? extends LinearArgument> transitionExpressions,
      long startingState, long[] finalStates) {
    AutomatonConstraint ct = new AutomatonConstraint(modelBuilder);
    AutomatonConstraintProto.Builder automaton = ct.getBuilder().getAutomatonBuilder();
    for (LinearArgument expr : transitionExpressions) {
      automaton.addExprs(
          getLinearExpressionProtoBuilderFromLinearArgument(expr, /* negate= */ false));
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
  public Constraint addMinEquality(LinearArgument target, LinearArgument[] exprs) {
    Constraint ct = new Constraint(modelBuilder);
    LinearArgumentProto.Builder linMax = ct.getBuilder().getLinMaxBuilder();
    linMax.setTarget(getLinearExpressionProtoBuilderFromLinearArgument(target, /*negate=*/true));
    for (LinearArgument expr : exprs) {
      linMax.addExprs(getLinearExpressionProtoBuilderFromLinearArgument(expr, /*negate=*/true));
    }
    return ct;
  }

  /** Adds {@code target == Min(exprs)}. */
  public Constraint addMinEquality(
      LinearArgument target, Iterable<? extends LinearArgument> exprs) {
    Constraint ct = new Constraint(modelBuilder);
    LinearArgumentProto.Builder linMax = ct.getBuilder().getLinMaxBuilder();
    linMax.setTarget(getLinearExpressionProtoBuilderFromLinearArgument(target, /*negate=*/true));
    for (LinearArgument expr : exprs) {
      linMax.addExprs(getLinearExpressionProtoBuilderFromLinearArgument(expr, /*negate=*/true));
    }
    return ct;
  }

  /** Adds {@code target == Max(vars)}. */
  public Constraint addMaxEquality(LinearArgument target, LinearArgument[] exprs) {
    Constraint ct = new Constraint(modelBuilder);
    LinearArgumentProto.Builder linMax = ct.getBuilder().getLinMaxBuilder();
    linMax.setTarget(getLinearExpressionProtoBuilderFromLinearArgument(target, /*negate=*/false));
    for (LinearArgument expr : exprs) {
      linMax.addExprs(getLinearExpressionProtoBuilderFromLinearArgument(expr, /*negate=*/false));
    }
    return ct;
  }

  /** Adds {@code target == Max(exprs)}. */
  public Constraint addMaxEquality(
      LinearArgument target, Iterable<? extends LinearArgument> exprs) {
    Constraint ct = new Constraint(modelBuilder);
    LinearArgumentProto.Builder linMax = ct.getBuilder().getLinMaxBuilder();
    linMax.setTarget(getLinearExpressionProtoBuilderFromLinearArgument(target, /*negate=*/false));
    for (LinearArgument expr : exprs) {
      linMax.addExprs(getLinearExpressionProtoBuilderFromLinearArgument(expr, /*negate=*/false));
    }
    return ct;
  }

  /** Adds {@code target == num / denom}, rounded towards 0. */
  public Constraint addDivisionEquality(
      LinearArgument target, LinearArgument num, LinearArgument denom) {
    Constraint ct = new Constraint(modelBuilder);
    ct.getBuilder()
        .getIntDivBuilder()
        .setTarget(getLinearExpressionProtoBuilderFromLinearArgument(target, /*negate=*/false))
        .addExprs(getLinearExpressionProtoBuilderFromLinearArgument(num, /*negate=*/false))
        .addExprs(getLinearExpressionProtoBuilderFromLinearArgument(denom, /*negate=*/false));
    return ct;
  }

  /** Adds {@code target == Abs(expr)}. */
  public Constraint addAbsEquality(LinearArgument target, LinearArgument expr) {
    Constraint ct = new Constraint(modelBuilder);
    LinearArgumentProto.Builder linMax = ct.getBuilder().getLinMaxBuilder();
    linMax.setTarget(getLinearExpressionProtoBuilderFromLinearArgument(target, /*negate=*/false));
    linMax.addExprs(getLinearExpressionProtoBuilderFromLinearArgument(expr, /*negate=*/false));
    linMax.addExprs(getLinearExpressionProtoBuilderFromLinearArgument(expr, /*negate=*/true));
    return ct;
  }

  /** Adds {@code target == var % mod}. */
  public Constraint addModuloEquality(
      LinearArgument target, LinearArgument var, LinearArgument mod) {
    Constraint ct = new Constraint(modelBuilder);
    ct.getBuilder()
        .getIntModBuilder()
        .setTarget(getLinearExpressionProtoBuilderFromLinearArgument(target, /*negate=*/false))
        .addExprs(getLinearExpressionProtoBuilderFromLinearArgument(var, /*negate=*/false))
        .addExprs(getLinearExpressionProtoBuilderFromLinearArgument(mod, /*negate=*/false));
    return ct;
  }

  /** Adds {@code target == var % mod}. */
  public Constraint addModuloEquality(LinearArgument target, LinearArgument var, long mod) {
    Constraint ct = new Constraint(modelBuilder);
    ct.getBuilder()
        .getIntModBuilder()
        .setTarget(getLinearExpressionProtoBuilderFromLinearArgument(target, /*negate=*/false))
        .addExprs(getLinearExpressionProtoBuilderFromLinearArgument(var, /*negate=*/false))
        .addExprs(getLinearExpressionProtoBuilderFromLong(mod));
    return ct;
  }

  /** Adds {@code target == Product(exprs)}. */
  public Constraint addMultiplicationEquality(LinearArgument target, LinearArgument[] exprs) {
    Constraint ct = new Constraint(modelBuilder);
    LinearArgumentProto.Builder intProd = ct.getBuilder().getIntProdBuilder();
    intProd.setTarget(getLinearExpressionProtoBuilderFromLinearArgument(target, /*negate=*/false));
    for (LinearArgument expr : exprs) {
      intProd.addExprs(getLinearExpressionProtoBuilderFromLinearArgument(expr, /*negate=*/false));
    }
    return ct;
  }

  /** Adds {@code target == left * right}. */
  public Constraint addMultiplicationEquality(
      LinearArgument target, LinearArgument left, LinearArgument right) {
    Constraint ct = new Constraint(modelBuilder);
    LinearArgumentProto.Builder intProd = ct.getBuilder().getIntProdBuilder();
    intProd.setTarget(getLinearExpressionProtoBuilderFromLinearArgument(target, /*negate=*/false));
    intProd.addExprs(getLinearExpressionProtoBuilderFromLinearArgument(left, /*negate=*/false));
    intProd.addExprs(getLinearExpressionProtoBuilderFromLinearArgument(right, /*negate=*/false));
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
      LinearArgument start, LinearArgument size, LinearArgument end, String name) {
    return new IntervalVar(modelBuilder,
        getLinearExpressionProtoBuilderFromLinearArgument(start, /*negate=*/false),
        getLinearExpressionProtoBuilderFromLinearArgument(size, /*negate=*/false),
        getLinearExpressionProtoBuilderFromLinearArgument(end, /*negate=*/false), name);
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
  public IntervalVar newFixedSizeIntervalVar(LinearArgument start, long size, String name) {
    return new IntervalVar(modelBuilder,
        getLinearExpressionProtoBuilderFromLinearArgument(start, /*negate=*/false),
        getLinearExpressionProtoBuilderFromLong(size),
        getLinearExpressionProtoBuilderFromLinearArgument(
            LinearExpr.newBuilder().add(start).add(size), /*negate=*/false),
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
  public IntervalVar newOptionalIntervalVar(LinearArgument start, LinearArgument size,
      LinearArgument end, Literal isPresent, String name) {
    return new IntervalVar(modelBuilder,
        getLinearExpressionProtoBuilderFromLinearArgument(start, /*negate=*/false),
        getLinearExpressionProtoBuilderFromLinearArgument(size, /*negate=*/false),
        getLinearExpressionProtoBuilderFromLinearArgument(end, /*negate=*/false),
        isPresent.getIndex(), name);
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
      LinearArgument start, long size, Literal isPresent, String name) {
    return new IntervalVar(modelBuilder,
        getLinearExpressionProtoBuilderFromLinearArgument(start, /*negate=*/false),
        getLinearExpressionProtoBuilderFromLong(size),
        getLinearExpressionProtoBuilderFromLinearArgument(
            LinearExpr.newBuilder().add(start).add(size), /*negate=*/false),
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
    return addNoOverlap(Arrays.asList(intervalVars));
  }

  /**
   * Adds {@code NoOverlap(intervalVars)}.
   *
   * @see addNoOverlap(IntervalVar[]).
   */
  public Constraint addNoOverlap(Iterable<IntervalVar> intervalVars) {
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
  public CumulativeConstraint addCumulative(LinearArgument capacity) {
    CumulativeConstraint ct = new CumulativeConstraint(this);
    CumulativeConstraintProto.Builder cumul = ct.getBuilder().getCumulativeBuilder();
    cumul.setCapacity(getLinearExpressionProtoBuilderFromLinearArgument(capacity, false));
    return ct;
  }

  /**
   * Adds {@code Cumulative(capacity)}.
   *
   * @see #addCumulative(LinearArgument capacity)
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

  /** Adds hinting to a literal */
  public void addHint(Literal lit, boolean value) {
    if (isPositive(lit)) {
      modelBuilder.getSolutionHintBuilder().addVars(lit.getIndex());
      modelBuilder.getSolutionHintBuilder().addValues(value ? 1 : 0);
    } else {
      modelBuilder.getSolutionHintBuilder().addVars(negated(lit.getIndex()));
      modelBuilder.getSolutionHintBuilder().addValues(value ? 0 : 1);
    }
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
  public void minimize(LinearArgument expr) {
    clearObjective();
    CpObjectiveProto.Builder obj = modelBuilder.getObjectiveBuilder();
    final LinearExpr e = expr.build();
    for (int i = 0; i < e.numElements(); ++i) {
      obj.addVars(e.getVariableIndex(i)).addCoeffs(e.getCoefficient(i));
    }
    obj.setOffset((double) e.getOffset());
  }

  /** Adds a minimization objective of a linear expression. */
  public void minimize(DoubleLinearExpr expr) {
    clearObjective();
    FloatObjectiveProto.Builder obj = modelBuilder.getFloatingPointObjectiveBuilder();
    for (int i = 0; i < expr.numElements(); ++i) {
      obj.addVars(expr.getVariableIndex(i)).addCoeffs(expr.getCoefficient(i));
    }
    obj.setOffset(expr.getOffset()).setMaximize(false);
  }

  /** Adds a maximization objective of a linear expression. */
  public void maximize(LinearArgument expr) {
    clearObjective();
    CpObjectiveProto.Builder obj = modelBuilder.getObjectiveBuilder();
    final LinearExpr e = expr.build();
    for (int i = 0; i < e.numElements(); ++i) {
      obj.addVars(e.getVariableIndex(i)).addCoeffs(-e.getCoefficient(i));
    }
    obj.setOffset((double) -e.getOffset());
    obj.setScalingFactor(-1.0);
  }

  /** Adds a maximization objective of a linear expression. */
  public void maximize(DoubleLinearExpr expr) {
    clearObjective();
    FloatObjectiveProto.Builder obj = modelBuilder.getFloatingPointObjectiveBuilder();
    for (int i = 0; i < expr.numElements(); ++i) {
      obj.addVars(expr.getVariableIndex(i)).addCoeffs(expr.getCoefficient(i));
    }
    obj.setOffset(expr.getOffset()).setMaximize(true);
  }

  /** Clears the objective. */
  public void clearObjective() {
    modelBuilder.clearObjective();
    modelBuilder.clearFloatingPointObjective();
  }

  /** Checks if the model contains an objective. */
  public boolean hasObjective() {
    return modelBuilder.hasObjective() || modelBuilder.hasFloatingPointObjective();
  }

  // DecisionStrategy

  /** Adds {@code DecisionStrategy(expressions, varStr, domStr)}. */
  public void addDecisionStrategy(LinearArgument[] expressions,
      DecisionStrategyProto.VariableSelectionStrategy varStr,
      DecisionStrategyProto.DomainReductionStrategy domStr) {
    DecisionStrategyProto.Builder ds = modelBuilder.addSearchStrategyBuilder();
    for (LinearArgument arg : expressions) {
      ds.addExprs(getLinearExpressionProtoBuilderFromLinearArgument(arg, /* negate= */ false));
    }
    ds.setVariableSelectionStrategy(varStr).setDomainReductionStrategy(domStr);
  }

  /** Adds {@code DecisionStrategy(expressions, varStr, domStr)}. */
  public void addDecisionStrategy(Iterable<? extends LinearArgument> expressions,
      DecisionStrategyProto.VariableSelectionStrategy varStr,
      DecisionStrategyProto.DomainReductionStrategy domStr) {
    DecisionStrategyProto.Builder ds = modelBuilder.addSearchStrategyBuilder();
    for (LinearArgument arg : expressions) {
      ds.addExprs(getLinearExpressionProtoBuilderFromLinearArgument(arg, /* negate= */ false));
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
  LinearExpressionProto.Builder getLinearExpressionProtoBuilderFromLinearArgument(
      LinearArgument arg, boolean negate) {
    LinearExpressionProto.Builder builder = LinearExpressionProto.newBuilder();
    final LinearExpr expr = arg.build();
    final int numVariables = expr.numElements();
    final long mult = negate ? -1 : 1;
    for (int i = 0; i < numVariables; ++i) {
      builder.addVars(expr.getVariableIndex(i));
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

  public boolean isPositive(Literal lit) {
    return lit.getIndex() >= 0;
  }

  /** Returns the model builder. */
  public CpModelProto.Builder getBuilder() {
    return modelBuilder;
  }

  private final CpModelProto.Builder modelBuilder;
  private final Map<Long, Integer> constantMap;
}
