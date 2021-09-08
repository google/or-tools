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
import com.google.ortools.sat.CircuitConstraintProto;
import com.google.ortools.sat.CpModelProto;
import com.google.ortools.sat.CpObjectiveProto;
import com.google.ortools.sat.CumulativeConstraintProto;
import com.google.ortools.sat.DecisionStrategyProto;
import com.google.ortools.sat.ElementConstraintProto;
import com.google.ortools.sat.IntegerArgumentProto;
import com.google.ortools.sat.InverseConstraintProto;
import com.google.ortools.sat.LinearConstraintProto;
import com.google.ortools.sat.LinearExpressionProto;
import com.google.ortools.sat.NoOverlap2DConstraintProto;
import com.google.ortools.sat.NoOverlapConstraintProto;
import com.google.ortools.sat.ReservoirConstraintProto;
import com.google.ortools.sat.TableConstraintProto;
import com.google.ortools.util.Domain;
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

  /** Adds {@code And(literals) == true}. */
  public Constraint addBoolAnd(Literal[] literals) {
    Constraint ct = new Constraint(modelBuilder);
    BoolArgumentProto.Builder boolOr = ct.getBuilder().getBoolAndBuilder();
    for (Literal lit : literals) {
      boolOr.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /** Adds {@code XOr(literals) == true}. */
  public Constraint addBoolXor(Literal[] literals) {
    Constraint ct = new Constraint(modelBuilder);
    BoolArgumentProto.Builder boolOr = ct.getBuilder().getBoolXorBuilder();
    for (Literal lit : literals) {
      boolOr.addLiterals(lit.getIndex());
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
      allDiff.addVars(var.getIndex());
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
   * Adds {@code Circuit(tails, heads, literals)}.
   *
   * <p>Adds a circuit constraint from a sparse list of arcs that encode the graph.
   *
   * <p>A circuit is a unique Hamiltonian path in a subgraph of the total graph. In case a node 'i'
   * is not in the path, then there must be a loop arc {@code 'i -> i'} associated with a true
   * literal. Otherwise this constraint will fail.
   *
   * @param tails the tails of all arcs
   * @param heads the heads of all arcs
   * @param literals the literals that control whether an arc is selected or not
   * @return an instance of the Constraint class
   * @throws MismatchedArrayLengths if the arrays have different sizes
   */
  public Constraint addCircuit(int[] tails, int[] heads, Literal[] literals) {
    if (tails.length != heads.length) {
      throw new MismatchedArrayLengths("CpModel.addCircuit", "tails", "heads");
    }
    if (tails.length != literals.length) {
      throw new MismatchedArrayLengths("CpModel.addCircuit", "tails", "literals");
    }

    Constraint ct = new Constraint(modelBuilder);
    CircuitConstraintProto.Builder circuit = ct.getBuilder().getCircuitBuilder();
    for (int t : tails) {
      circuit.addTails(t);
    }
    for (int h : heads) {
      circuit.addHeads(h);
    }
    for (Literal lit : literals) {
      circuit.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /**
   * Adds {@code AllowedAssignments(variables, tuplesList)}.
   *
   * <p>An AllowedAssignments constraint is a constraint on an array of variables that forces, when
   * all variables are fixed to a single value, that the corresponding list of values is equal to
   * one of the tuples of the tupleList.
   *
   * @param variables a list of variables
   * @param tuplesList a list of admissible tuples. Each tuple must have the same length as the
   *     variables, and the ith value of a tuple corresponds to the ith variable.
   * @return an instance of the Constraint class
   * @throws WrongLength if one tuple does not have the same length as the variables
   */
  public Constraint addAllowedAssignments(IntVar[] variables, long[][] tuplesList) {
    Constraint ct = new Constraint(modelBuilder);
    TableConstraintProto.Builder table = ct.getBuilder().getTableBuilder();
    for (IntVar var : variables) {
      table.addVars(var.getIndex());
    }
    int numVars = variables.length;
    for (int t = 0; t < tuplesList.length; ++t) {
      if (tuplesList[t].length != numVars) {
        throw new WrongLength("CpModel.addAllowedAssignments",
            "tuple " + t + " does not have the same length as the variables");
      }
      for (int i = 0; i < tuplesList[t].length; ++i) {
        table.addValues(tuplesList[t][i]);
      }
    }
    return ct;
  }

  /**
   * Adds {@code AllowedAssignments(variables, tuplesList)}.
   *
   * @see #addAllowedAssignments(IntVar[], long[][]) addAllowedAssignments
   */
  public Constraint addAllowedAssignments(IntVar[] variables, int[][] tuplesList) {
    Constraint ct = new Constraint(modelBuilder);
    TableConstraintProto.Builder table = ct.getBuilder().getTableBuilder();
    for (IntVar var : variables) {
      table.addVars(var.getIndex());
    }
    int numVars = variables.length;
    for (int t = 0; t < tuplesList.length; ++t) {
      if (tuplesList[t].length != numVars) {
        throw new WrongLength("CpModel.addAllowedAssignments",
            "tuple " + t + " does not have the same length as the variables");
      }
      for (int i = 0; i < tuplesList[t].length; ++i) {
        table.addValues(tuplesList[t][i]);
      }
    }
    return ct;
  }

  /**
   * Adds {@code ForbiddenAssignments(variables, tuplesList)}.
   *
   * <p>A ForbiddenAssignments constraint is a constraint on an array of variables where the list of
   * impossible combinations is provided in the tuples list.
   *
   * @param variables a list of variables
   * @param tuplesList a list of forbidden tuples. Each tuple must have the same length as the
   *     variables, and the ith value of a tuple corresponds to the ith variable.
   * @return an instance of the Constraint class
   * @throws WrongLength if one tuple does not have the same length as the variables
   */
  public Constraint addForbiddenAssignments(IntVar[] variables, long[][] tuplesList) {
    Constraint ct = addAllowedAssignments(variables, tuplesList);
    // Reverse the flag.
    ct.getBuilder().getTableBuilder().setNegated(true);
    return ct;
  }

  /**
   * Adds {@code ForbiddenAssignments(variables, tuplesList)}.
   *
   * @see #addForbiddenAssignments(IntVar[], long[][]) addForbiddenAssignments
   */
  public Constraint addForbiddenAssignments(IntVar[] variables, int[][] tuplesList) {
    Constraint ct = addAllowedAssignments(variables, tuplesList);
    // Reverse the flag.
    ct.getBuilder().getTableBuilder().setNegated(true);
    return ct;
  }

  /**
   * Adds an automaton constraint.
   *
   * <p>An automaton constraint takes a list of variables (of size n), an initial state, a set of
   * final states, and a set of transitions. A transition is a triplet ('tail', 'transition',
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
   * @param transitions a list of transition for the automaton, in the following format
   *     (currentState, variableValue, nextState)
   * @return an instance of the Constraint class
   * @throws WrongLength if one transition does not have a length of 3
   */
  public Constraint addAutomaton(
      IntVar[] transitionVariables, long startingState, long[] finalStates, long[][] transitions) {
    Constraint ct = new Constraint(modelBuilder);
    AutomatonConstraintProto.Builder automaton = ct.getBuilder().getAutomatonBuilder();
    for (IntVar var : transitionVariables) {
      automaton.addVars(var.getIndex());
    }
    automaton.setStartingState(startingState);
    for (long c : finalStates) {
      automaton.addFinalStates(c);
    }
    for (long[] t : transitions) {
      if (t.length != 3) {
        throw new WrongLength("CpModel.addAutomaton", "transition does not have length 3");
      }
      automaton.addTransitionTail(t[0]).addTransitionLabel(t[1]).addTransitionHead(t[2]);
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
   * Adds {@code Reservoir(times, demands, minLevel, maxLevel)}.
   *
   * <p>Maintains a reservoir level within bounds. The water level starts at 0, and at any times, it
   * must be between minLevel and maxLevel. If the variable {@code times[i]} is assigned a value t,
   * and if {@code actives[i]} is true, then the current level changes by {@code demands[i]} (which
   * is constant) at the time t.
   *
   * <p>Note that {@code minLevel} must be less than 0, and {@code maxLevel} must be greater than 0.
   * Therefore, {@code forall t : minLevel <= sum(demands[i] if times[i] <= t) <= maxLevel}.
   *
   * @param times a list of integer variables which specify the time of the filling or emptying the
   *     reservoir
   * @param demands a list of integer values that specifies the amount of the emptying or feeling
   * @param minLevel at any time, the level of the reservoir must be greater of equal than the min
   *     level. minLevel must me <= 0.
   * @param maxLevel at any time, the level of the reservoir must be less or equal than the max
   *     level. maxLevel must be >= 0.
   * @return an instance of the Constraint class
   * @throws MismatchedArrayLengths if times and demands have different length
   * @throws IllegalArgumentException if minLevel > 0
   * @throws IllegalArgumentException if maxLevel < 0
   */
  public Constraint addReservoirConstraint(
      IntVar[] times, long[] demands, long minLevel, long maxLevel) {
    if (times.length != demands.length) {
      throw new MismatchedArrayLengths("CpModel.addReservoirConstraint", "times", "demands");
    }
    if (minLevel > 0) {
      throw new IllegalArgumentException("CpModel.addReservoirConstraint: minLevel must be <= 0");
    }
    if (maxLevel < 0) {
      throw new IllegalArgumentException("CpModel.addReservoirConstraint: maxLevel must be >= 0");
    }
    Constraint ct = new Constraint(modelBuilder);
    ReservoirConstraintProto.Builder reservoir = ct.getBuilder().getReservoirBuilder();
    for (IntVar var : times) {
      reservoir.addTimes(var.getIndex());
    }
    for (long d : demands) {
      reservoir.addDemands(d);
    }
    reservoir.setMinLevel(minLevel).setMaxLevel(maxLevel);
    return ct;
  }

  /**
   * Adds {@code Reservoir(times, demands, minLevel, maxLevel)}.
   *
   * @see #addReservoirConstraint(IntVar[], long[], long, long) Reservoir
   */
  public Constraint addReservoirConstraint(
      IntVar[] times, int[] demands, long minLevel, long maxLevel) {
    return addReservoirConstraint(times, toLongArray(demands), minLevel, maxLevel);
  }

  /**
   * Adds {@code Reservoir(times, demands, actives, minLevel, maxLevel)}.
   *
   * <p>Maintains a reservoir level within bounds. The water level starts at 0, and at any time, it
   * must be between minLevel and maxLevel. If the variable {@code times[i]} is assigned a value t,
   * then the current level changes by {@code demands[i]} (which is constant) at the time t.
   *
   * <p>Note that {@code minLevel} must be less than 0, and {@code maxLevel} must be greater than 0.
   * Therefore, {@code forall t : minLevel <= sum(demands[i] * actives[i] if times[i] <= t) <=
   * maxLevel}.
   *
   * @param times a list of integer variables which specify the time of the filling or emptying the
   *     reservoir
   * @param demands a list of integer values that specifies the amount of the emptying or feeling
   * @param minLevel at any time, the level of the reservoir must be greater of equal than the min
   *     level. minLevel must me <= 0.
   * @param maxLevel at any time, the level of the reservoir must be less or equal than the max
   *     level. maxLevel must be >= 0.
   * @return an instance of the Constraint class
   * @throws MismatchedArrayLengths if times, demands, or actives have different length
   * @throws IllegalArgumentException if minLevel > 0
   * @throws IllegalArgumentException if maxLevel < 0
   */
  public Constraint addReservoirConstraintWithActive(
      IntVar[] times, long[] demands, IntVar[] actives, long minLevel, long maxLevel) {
    if (times.length != demands.length) {
      throw new MismatchedArrayLengths(
          "CpModel.addReservoirConstraintWithActive", "times", "demands");
    }
    if (times.length != actives.length) {
      throw new MismatchedArrayLengths(
          "CpModel.addReservoirConstraintWithActive", "times", "actives");
    }
    if (minLevel > 0) {
      throw new IllegalArgumentException(
          "CpModel.addReservoirConstraintWithActive: minLevel must be <= 0");
    }
    if (maxLevel < 0) {
      throw new IllegalArgumentException(
          "CpModel.addReservoirConstraintWithActive: maxLevel must be >= 0");
    }

    Constraint ct = new Constraint(modelBuilder);
    ReservoirConstraintProto.Builder reservoir = ct.getBuilder().getReservoirBuilder();
    for (IntVar var : times) {
      reservoir.addTimes(var.getIndex());
    }
    for (long d : demands) {
      reservoir.addDemands(d);
    }
    for (IntVar var : actives) {
      reservoir.addActives(var.getIndex());
    }
    reservoir.setMinLevel(minLevel).setMaxLevel(maxLevel);
    return ct;
  }

  /**
   * Adds {@code Reservoir(times, demands, actives, minLevel, maxLevel)}.
   *
   * @see #addReservoirConstraintWithActive(IntVar[], long[], IntVar[], long, long) Reservoir
   */
  public Constraint addReservoirConstraintWithActive(
      IntVar[] times, int[] demands, IntVar[] actives, long minLevel, long maxLevel) {
    return addReservoirConstraintWithActive(
        times, toLongArray(demands), actives, minLevel, maxLevel);
  }

  /** Adds {@code var == i + offset <=> booleans[i] == true for all i in [0, booleans.length)}. */
  public void addMapDomain(IntVar var, Literal[] booleans, long offset) {
    for (int i = 0; i < booleans.length; ++i) {
      addEquality(var, offset + i).onlyEnforceIf(booleans[i]);
      addDifferent(var, offset + i).onlyEnforceIf(booleans[i].not());
    }
  }

  /** Adds {@code target == Min(vars)}. */
  public Constraint addMinEquality(IntVar target, IntVar[] vars) {
    Constraint ct = new Constraint(modelBuilder);
    IntegerArgumentProto.Builder intMin =
        ct.getBuilder().getIntMinBuilder().setTarget(target.getIndex());
    for (IntVar var : vars) {
      intMin.addVars(var.getIndex());
    }
    return ct;
  }

  /** Adds {@code target == Max(vars)}. */
  public Constraint addMaxEquality(IntVar target, IntVar[] vars) {
    Constraint ct = new Constraint(modelBuilder);
    IntegerArgumentProto.Builder intMax =
        ct.getBuilder().getIntMaxBuilder().setTarget(target.getIndex());
    for (IntVar var : vars) {
      intMax.addVars(var.getIndex());
    }
    return ct;
  }

  /** Adds {@code target == num / denom}, rounded towards 0. */
  public Constraint addDivisionEquality(IntVar target, IntVar num, IntVar denom) {
    Constraint ct = new Constraint(modelBuilder);
    ct.getBuilder()
        .getIntDivBuilder()
        .setTarget(target.getIndex())
        .addVars(num.getIndex())
        .addVars(denom.getIndex());
    return ct;
  }

  /** Adds {@code target == Abs(var)}. */
  public Constraint addAbsEquality(IntVar target, IntVar var) {
    Constraint ct = new Constraint(modelBuilder);
    ct.getBuilder()
        .getIntMaxBuilder()
        .setTarget(target.getIndex())
        .addVars(var.getIndex())
        .addVars(-var.getIndex() - 1);
    return ct;
  }

  /** Adds {@code target == var % mod}. */
  public Constraint addModuloEquality(IntVar target, IntVar var, IntVar mod) {
    Constraint ct = new Constraint(modelBuilder);
    ct.getBuilder()
        .getIntModBuilder()
        .setTarget(target.getIndex())
        .addVars(var.getIndex())
        .addVars(mod.getIndex());
    return ct;
  }

  /** Adds {@code target == var % mod}. */
  public Constraint addModuloEquality(IntVar target, IntVar var, long mod) {
    Constraint ct = new Constraint(modelBuilder);
    ct.getBuilder()
        .getIntModBuilder()
        .setTarget(target.getIndex())
        .addVars(var.getIndex())
        .addVars(indexFromConstant(mod));
    return ct;
  }

  /** Adds {@code target == Product(vars)}. */
  public Constraint addProductEquality(IntVar target, IntVar[] vars) {
    Constraint ct = new Constraint(modelBuilder);
    IntegerArgumentProto.Builder intProd =
        ct.getBuilder().getIntProdBuilder().setTarget(target.getIndex());
    for (IntVar var : vars) {
      intProd.addVars(var.getIndex());
    }
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
    return new IntervalVar(modelBuilder, getLinearExpressionProtoBuilderFromLinearExpr(start),
        getLinearExpressionProtoBuilderFromLinearExpr(size),
        getLinearExpressionProtoBuilderFromLinearExpr(end), name);
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
    return new IntervalVar(modelBuilder, getLinearExpressionProtoBuilderFromLinearExpr(start),
        getLinearExpressionProtoBuilderFromLong(size),
        getLinearExpressionProtoBuilderFromLinearExpr(new Sum(start, size)), name);
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
    return new IntervalVar(modelBuilder, getLinearExpressionProtoBuilderFromLinearExpr(start),
        getLinearExpressionProtoBuilderFromLinearExpr(size),
        getLinearExpressionProtoBuilderFromLinearExpr(end), isPresent.getIndex(), name);
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
    return new IntervalVar(modelBuilder, getLinearExpressionProtoBuilderFromLinearExpr(start),
        getLinearExpressionProtoBuilderFromLong(size),
        getLinearExpressionProtoBuilderFromLinearExpr(new Sum(start, size)), isPresent.getIndex(),
        name);
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
   * @param xIntervals the X coordinates of the rectangles
   * @param yIntervals the Y coordinates of the rectangles
   * @return an instance of the Constraint class
   */
  public Constraint addNoOverlap2D(IntervalVar[] xIntervals, IntervalVar[] yIntervals) {
    Constraint ct = new Constraint(modelBuilder);
    NoOverlap2DConstraintProto.Builder noOverlap2d = ct.getBuilder().getNoOverlap2DBuilder();
    for (IntervalVar x : xIntervals) {
      noOverlap2d.addXIntervals(x.getIndex());
    }
    for (IntervalVar y : yIntervals) {
      noOverlap2d.addYIntervals(y.getIndex());
    }
    return ct;
  }

  /**
   * Adds {@code Cumulative(intervals, demands, capacity)}.
   *
   * <p>This constraint enforces that:
   *
   * <p>{@code forall t: sum(demands[i] if (start(intervals[t]) <= t < end(intervals[t])) and (t is
   * present)) <= capacity}.
   *
   * @param intervals the list of intervals
   * @param demands the list of demands for each interval. Each demand must be a positive integer
   *     variable.
   * @param capacity the maximum capacity of the cumulative constraint. It must be a positive
   *     integer variable.
   * @return an instance of the Constraint class
   */
  public Constraint addCumulative(IntervalVar[] intervals, IntVar[] demands, IntVar capacity) {
    Constraint ct = new Constraint(modelBuilder);
    CumulativeConstraintProto.Builder cumul = ct.getBuilder().getCumulativeBuilder();
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
   * Adds {@code Cumulative(intervals, demands, capacity)} with fixed demands.
   *
   * @see #addCumulative(IntervalVar[], IntVar[], IntVar) AddCumulative
   */
  public Constraint addCumulative(IntervalVar[] intervals, long[] demands, IntVar capacity) {
    Constraint ct = new Constraint(modelBuilder);
    CumulativeConstraintProto.Builder cumul = ct.getBuilder().getCumulativeBuilder();
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
   * Adds {@code Cumulative(intervals, demands, capacity)} with fixed demands.
   *
   * @see #addCumulative(IntervalVar[], IntVar[], IntVar) AddCumulative
   */
  public Constraint addCumulative(IntervalVar[] intervals, int[] demands, IntVar capacity) {
    return addCumulative(intervals, toLongArray(demands), capacity);
  }

  /**
   * Adds {@code Cumulative(intervals, demands, capacity)} with fixed capacity.
   *
   * @see #addCumulative(IntervalVar[], IntVar[], IntVar) AddCumulative
   */
  public Constraint addCumulative(IntervalVar[] intervals, IntVar[] demands, long capacity) {
    Constraint ct = new Constraint(modelBuilder);
    CumulativeConstraintProto.Builder cumul = ct.getBuilder().getCumulativeBuilder();
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
   * Adds {@code Cumulative(intervals, demands, capacity)} with fixed demands and fixed capacity.
   *
   * @see #addCumulative(IntervalVar[], IntVar[], IntVar) AddCumulative
   */
  public Constraint addCumulative(IntervalVar[] intervals, long[] demands, long capacity) {
    Constraint ct = new Constraint(modelBuilder);
    CumulativeConstraintProto.Builder cumul = ct.getBuilder().getCumulativeBuilder();
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
   * Adds {@code Cumulative(intervals, demands, capacity)} with fixed demands and fixed capacity.
   *
   * @see #addCumulative(IntervalVar[], IntVar[], IntVar) AddCumulative
   */
  public Constraint addCumulative(IntervalVar[] intervals, int[] demands, long capacity) {
    return addCumulative(intervals, toLongArray(demands), capacity);
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

  /** Adds a maximization objective of a linear expression. */
  public void maximize(LinearExpr expr) {
    CpObjectiveProto.Builder obj = modelBuilder.getObjectiveBuilder();
    for (int i = 0; i < expr.numElements(); ++i) {
      obj.addVars(expr.getVariable(i).getIndex()).addCoeffs(-expr.getCoefficient(i));
    }
    obj.setOffset(-expr.getOffset());
    obj.setScalingFactor(-1.0);
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

  LinearExpressionProto.Builder getLinearExpressionProtoBuilderFromLinearExpr(LinearExpr expr) {
    LinearExpressionProto.Builder builder = LinearExpressionProto.newBuilder();
    final int numVariables = expr.numElements();
    for (int i = 0; i < numVariables; ++i) {
      builder.addVars(expr.getVariable(i).getIndex());
      builder.addCoeffs(expr.getCoefficient(i));
    }
    builder.setOffset(expr.getOffset());
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
