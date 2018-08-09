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
import com.google.ortools.sat.AutomataConstraintProto;
import com.google.ortools.sat.BoolArgumentProto;
import com.google.ortools.sat.CircuitConstraintProto;
import com.google.ortools.sat.CpModelProto;
import com.google.ortools.sat.CpObjectiveProto;
import com.google.ortools.sat.CumulativeConstraintProto;
import com.google.ortools.sat.ElementConstraintProto;
import com.google.ortools.sat.IntegerArgumentProto;
import com.google.ortools.sat.IntegerVariableProto;
import com.google.ortools.sat.InverseConstraintProto;
import com.google.ortools.sat.LinearConstraintProto;
import com.google.ortools.sat.NoOverlap2DConstraintProto;
import com.google.ortools.sat.NoOverlapConstraintProto;
import com.google.ortools.sat.ReservoirConstraintProto;
import com.google.ortools.sat.TableConstraintProto;

/**
 * Main modeling class.
 *
 * <p>It proposes a factory to create all modeling objects understood by the SAT solver.
 */
public class CpModel {

  static class CpModelException extends Exception {
    public CpModelException(String methodName, String msg) {
      // Call constructor of parent Exception
      super("CpModel." + methodName + ": " + msg);
    }
  }

  static class MismatchedArrayLengths extends CpModelException {
    public MismatchedArrayLengths(String methodName, String array1Name, String array2Name) {
      super(methodName, array1Name + " and " + array2Name + " have mismatched lengths");
    }
  }

  static class WrongLength extends CpModelException {
    public WrongLength(String methodName, String msg) {
      super(methodName, msg);
    }
  }

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

  /** Adds a =&gt; b. */
  public Constraint addImplication(ILiteral a, ILiteral b) {
    return addBoolOr(new ILiteral[] {a.not(), b});
  }

  // Linear constraints.

  /** Adds lb &lt;= sum(vars) &lt;= ub. */
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

  /** Adds lb &lt;= sum(vars[i] * coeffs[i]) &lt;= ub. */
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

  /** Adds lb &lt;= sum(vars[i] * coeffs[i]) &lt;= ub. */
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

  /** Adds var &lt;= value. */
  public Constraint addLessOrEqual(IntVar var, long value) {
    Constraint ct = new Constraint(builder_);
    LinearConstraintProto.Builder lin = ct.builder().getLinearBuilder();
    lin.addVars(var.getIndex());
    lin.addCoeffs(1);
    lin.addDomain(java.lang.Long.MIN_VALUE);
    lin.addDomain(value);
    return ct;
  }

  /** Adds var &gt;= value. */
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
    if (value > java.lang.Long.MIN_VALUE) {
      lin.addDomain(java.lang.Long.MIN_VALUE);
      lin.addDomain(value - 1);
    }
    if (value < java.lang.Long.MAX_VALUE) {
      lin.addDomain(value + 1);
      lin.addDomain(java.lang.Long.MAX_VALUE);
    }
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
    if (offset > java.lang.Long.MIN_VALUE) {
      lin.addDomain(java.lang.Long.MIN_VALUE);
      lin.addDomain(offset - 1);
    }
    if (offset < java.lang.Long.MAX_VALUE) {
      lin.addDomain(offset + 1);
      lin.addDomain(java.lang.Long.MAX_VALUE);
    }
    return ct;
  }

  /** before + offset &lt;= after. */
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

  /** Adds before &lt;= after. */
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
  public Constraint addAllDifferent(IntVar[] variables) {
    Constraint ct = new Constraint(builder_);
    AllDifferentConstraintProto.Builder allDiff = ct.builder().getAllDiffBuilder();
    for (IntVar var : variables) {
      allDiff.addVars(var.getIndex());
    }
    return ct;
  }

  /** Adds the element constraint: variables[index] == target. */
  public Constraint addElement(IntVar index, IntVar[] variables, IntVar target) {
    Constraint ct = new Constraint(builder_);
    ElementConstraintProto.Builder element = ct.builder().getElementBuilder();
    element.setIndex(index.getIndex());
    for (IntVar var : variables) {
      element.addVars(var.getIndex());
    }
    element.setTarget(target.getIndex());
    return ct;
  }

  /** Adds the element constraint: values[index] == target. */
  public Constraint addElement(IntVar index, long[] values, IntVar target) {
    Constraint ct = new Constraint(builder_);
    ElementConstraintProto.Builder element = ct.builder().getElementBuilder();
    element.setIndex(index.getIndex());
    for (long v : values) {
      element.addVars(indexFromConstant(v));
    }
    element.setTarget(target.getIndex());
    return ct;
  }

  /** Adds the element constraint: values[index] == target. */
  public Constraint addElement(IntVar index, int[] values, IntVar target) {
    Constraint ct = new Constraint(builder_);
    ElementConstraintProto.Builder element = ct.builder().getElementBuilder();
    element.setIndex(index.getIndex());
    for (long v : values) {
      element.addVars(indexFromConstant(v));
    }
    element.setTarget(target.getIndex());
    return ct;
  }

  /**
   * Adds Circuit(tails, heads, literals).
   *
   * <p>Adds a circuit constraints from a sparse list of arcs that encode the graph.
   *
   * <p>A circuit is a unique Hamiltonian path in a subgraph of the total graph. In case a node 'i'
   * is not in the path, then there must be a loop arc 'i -&gt; i' associated with a true literal.
   * Otherwise this constraint will fail.
   *
   * @param tails the tails of all arcs.
   * @param heads the heads of all arcs.
   * @param literals the literals that control whether an arc is selected or not.
   * @return an instance of the Constraint class.
   * @throws MismatchedArrayLengths if the arrays have different sizes.
   */
  public Constraint addCircuit(int[] tails, int[] heads, ILiteral[] literals)
      throws MismatchedArrayLengths {
    if (tails.length != heads.length) {
      throw new MismatchedArrayLengths("addCircuit", "tails", "heads");
    }
    if (tails.length != literals.length) {
      throw new MismatchedArrayLengths("addCircuit", "tails", "literals");
    }

    Constraint ct = new Constraint(builder_);
    CircuitConstraintProto.Builder circuit = ct.builder().getCircuitBuilder();
    for (int t : tails) {
      circuit.addTails(t);
    }
    for (int h : heads) {
      circuit.addHeads(h);
    }
    for (ILiteral lit : literals) {
      circuit.addLiterals(lit.getIndex());
    }
    return ct;
  }

  /**
   * Adds AllowedAssignments(variables, tuplesList).
   *
   * <p>An AllowedAssignments constraint is a constraint on an array of variables that forces, when
   * all variables are fixed to a single value, that the corresponding list of values is equal to
   * one of the tuple of the tupleList.
   *
   * @param variables a list of variables.
   * @param tuplesList a list of admissible tuples. Each tuple must have the same length as the
   *     variables, and the ith value of a tuple corresponds to the ith variable.
   * @return an instance of the Constraint class.
   * @throws WrongLength if one tuple does not have the same length as the variables.
   */
  public Constraint addAllowedAssignments(IntVar[] variables, long[][] tuplesList)
      throws WrongLength {
    Constraint ct = new Constraint(builder_);
    TableConstraintProto.Builder table = ct.builder().getTableBuilder();
    for (IntVar var : variables) {
      table.addVars(var.getIndex());
    }
    int numVars = variables.length;
    for (int t = 0; t < tuplesList.length; ++t) {
      if (tuplesList[t].length != numVars) {
        throw new WrongLength(
            "addAllowedAssignments",
            "tuple " + t + " does not have the same length as the variables");
      }
      for (int i = 0; i < tuplesList[t].length; ++i) {
        table.addValues(tuplesList[t][i]);
      }
    }
    return ct;
  }

  /**
   * Adds AllowedAssignments(variables, tuplesList).
   *
   * @see #addAllowedAssignments(IntVar[], long[][]) addAllowedAssignments
   */
  public Constraint addAllowedAssignments(IntVar[] variables, int[][] tuplesList)
      throws WrongLength {
    Constraint ct = new Constraint(builder_);
    TableConstraintProto.Builder table = ct.builder().getTableBuilder();
    for (IntVar var : variables) {
      table.addVars(var.getIndex());
    }
    int numVars = variables.length;
    for (int t = 0; t < tuplesList.length; ++t) {
      if (tuplesList[t].length != numVars) {
        throw new WrongLength(
            "addAllowedAssignments",
            "tuple " + t + " does not have the same length as the variables");
      }
      for (int i = 0; i < tuplesList[t].length; ++i) {
        table.addValues(tuplesList[t][i]);
      }
    }
    return ct;
  }

  /**
   * Adds ForbiddenAssignments(variables, tuplesList).
   *
   * <p>A ForbiddenAssignments constraint is a constraint on an array of variables where the list of
   * impossible combinations is provided in the tuples list.
   *
   * @param variables a list of variables.
   * @param tuplesList a list of forbidden tuples. Each tuple must have the same length as the
   *     variables, and the ith value of a tuple corresponds to the ith variable.
   * @return an instance of the Constraint class.
   * @throws WrongLength if one tuple does not have the same length as the variables.
   */
  public Constraint addForbiddenAssignments(IntVar[] variables, long[][] tuplesList)
      throws WrongLength {
    Constraint ct = addAllowedAssignments(variables, tuplesList);
    // Reverse the flag.
    ct.builder().getTableBuilder().setNegated(true);
    return ct;
  }

  /**
   * Adds ForbiddenAssignments(variables, tuplesList).
   *
   * @see #addForbiddenAssignments(IntVar[], long[][]) addForbiddenAssignments
   */
  public Constraint addForbiddenAssignments(IntVar[] variables, int[][] tuplesList)
      throws WrongLength {
    Constraint ct = addAllowedAssignments(variables, tuplesList);
    // Reverse the flag.
    ct.builder().getTableBuilder().setNegated(true);
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
   * <p>This automata will be unrolled into a flow with n + 1 phases. Each phase contains the
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
   *     of the arcs traversed by the automata.
   * @param startingState the initial state of the automata.
   * @param finalStates a non empty list of admissible final states.
   * @param transitions a list of transition for the automata, in the following format
   *     (currentState, variableValue, nextState).
   * @return an instance of the Constraint class.
   * @throws WrongLength if one transition does not have a length of 3.
   */
  public Constraint addAutomaton(
      IntVar[] transitionVariables, long startingState, long[] finalStates, long[][] transitions)
      throws WrongLength {
    Constraint ct = new Constraint(builder_);
    AutomataConstraintProto.Builder automaton = ct.builder().getAutomataBuilder();
    for (IntVar var : transitionVariables) {
      automaton.addVars(var.getIndex());
    }
    automaton.setStartingState(startingState);
    for (long c : finalStates) {
      automaton.addFinalStates(c);
    }
    for (long[] t : transitions) {
      if (t.length != 3) {
        throw new WrongLength("addAutomaton", "transition does not have length 3");
      }
      automaton.addTransitionTail(t[0]);
      automaton.addTransitionLabel(t[1]);
      automaton.addTransitionHead(t[2]);
    }
    return ct;
  }

  /**
   * Adds Inverse(variables, inverseVariables).
   *
   * <p>An inverse constraint enforces that if 'variables[i]' is assigned a value 'j', then
   * inverseVariables[j] is assigned a value 'i'. And vice versa.
   *
   * @param variables an array of integer variables.
   * @param inverseVariables an array of integer variables.
   * @return an instance of the Constraint class.
   * @throws MismatchedArrayLengths if variables and inverseVariables have different length.
   */
  public Constraint addInverse(IntVar[] variables, IntVar[] inverseVariables)
      throws MismatchedArrayLengths {
    if (variables.length != inverseVariables.length) {
      throw new MismatchedArrayLengths("addInverse", "variables", "inverseVariables");
    }
    Constraint ct = new Constraint(builder_);
    InverseConstraintProto.Builder inverse = ct.builder().getInverseBuilder();
    for (IntVar var : variables) {
      inverse.addFDirect(var.getIndex());
    }
    for (IntVar var : inverseVariables) {
      inverse.addFInverse(var.getIndex());
    }
    return ct;
  }

  /**
   * Adds Reservoir(times, demands, minLevel, maxLevel).
   *
   * <p>Maintains a reservoir level within bounds. The water level starts at 0, and at any time
   * &gt;= 0, it must be between minLevel and maxLevel. Furthermore, this constraints expect all
   * times variables to be &gt;= 0. If the variable times[i] is assigned a value t, then the current
   * level changes by demands[i] (which is constant) at the time t.
   *
   * <p>Note that level min can be &gt; 0, or level max can be &lt; 0. It just forces some demands
   * to be executed at time 0 to make sure that we are within those bounds with the executed
   * demands. Therefore, at any time t &gt;= 0: {@code sum(demands[i] if times[i] &lt;= t) in
   * [minLevel, maxLevel]}
   *
   * @param times a list of positive integer variables which specify the time of the filling or
   *     emptying the reservoir.
   * @param demands a list of integer values that specifies the amount of the emptying or feeling.
   * @param minLevel at any time &gt;= 0, the level of the reservoir must be greater of equal than
   *     the min level.
   * @param maxLevel at any time &gt;= 0, the level of the reservoir must be less or equal than the
   *     max level.
   * @return an instance of the Constraint class.
   * @throws MismatchedArrayLengths if times and demands have different length.
   */
  public Constraint addReservoirConstraint(
      IntVar[] times, long[] demands, long minLevel, long maxLevel) throws MismatchedArrayLengths {
    if (times.length != demands.length) {
      throw new MismatchedArrayLengths("addReservoirConstraint", "times", "demands");
    }
    Constraint ct = new Constraint(builder_);
    ReservoirConstraintProto.Builder reservoir = ct.builder().getReservoirBuilder();
    for (IntVar var : times) {
      reservoir.addTimes(var.getIndex());
    }
    for (long d : demands) {
      reservoir.addDemands(d);
    }
    reservoir.setMinLevel(minLevel);
    reservoir.setMaxLevel(maxLevel);
    return ct;
  }

  /**
   * Adds Reservoir(times, demands, minLevel, maxLevel).
   *
   * @see #addReservoirConstraint(IntVar[], long[], long, long) Reservoir
   */
  public Constraint addReservoirConstraint(
      IntVar[] times, int[] demands, long minLevel, long maxLevel) throws MismatchedArrayLengths {
    return addReservoirConstraint(times, toLongArray(demands), minLevel, maxLevel);
  }

  /** Adds var == i + offset &lt;=&gt; booleans[i] == true for all i in [0, booleans.length). */
  public void addMapDomain(IntVar var, ILiteral[] booleans, long offset) {
    for (int i = 0; i < booleans.length; ++i) {
      addEquality(var, offset + i).onlyEnforceIf(booleans[i]);
      addDifferent(var, offset + i).onlyEnforceIf(booleans[i].not());
    }
  }

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

  /** Adds target == var % mod. */
  public Constraint addModuloEquality(IntVar target, IntVar var, IntVar mod) {
    Constraint ct = new Constraint(builder_);
    IntegerArgumentProto.Builder intMod = ct.builder().getIntModBuilder();
    intMod.setTarget(target.getIndex());
    intMod.addVars(var.getIndex());
    intMod.addVars(mod.getIndex());
    return ct;
  }

  /** Adds target == var % mod. */
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
  public IntervalVar newIntervalVar(IntVar start, IntVar size, IntVar end, String name) {
    return new IntervalVar(builder_, start.getIndex(), size.getIndex(), end.getIndex(), name);
  }

  /**
   * Creates an interval variables with a fixed end.
   *
   * @see #newIntervalVar(IntVar, IntVar, IntVar, String) newIntervalVar
   */
  public IntervalVar newIntervalVar(IntVar start, IntVar size, long end, String name) {
    return new IntervalVar(
        builder_, start.getIndex(), size.getIndex(), indexFromConstant(end), name);
  }

  /**
   * Creates an interval variables with a fixed size.
   *
   * @see #newIntervalVar(IntVar, IntVar, IntVar, String) newIntervalVar
   */
  public IntervalVar newIntervalVar(IntVar start, long size, IntVar end, String name) {
    return new IntervalVar(
        builder_, start.getIndex(), indexFromConstant(size), end.getIndex(), name);
  }

  /**
   * Creates an interval variables with a fixed start.
   *
   * @see #newIntervalVar(IntVar, IntVar, IntVar, String) newIntervalVar
   */
  public IntervalVar newIntervalVar(long start, IntVar size, IntVar end, String name) {
    return new IntervalVar(
        builder_, indexFromConstant(start), size.getIndex(), end.getIndex(), name);
  }

  /** Creates a fixed interval from its start and its size. */
  public IntervalVar newFixedInterval(long start, long size, String name) {
    return new IntervalVar(
        builder_,
        indexFromConstant(start),
        indexFromConstant(size),
        indexFromConstant(start + size),
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
   * @param isPresent a literal that indicates if the interval is active or not. A inactive interval
   *     is simply ignored by all constraints.
   * @param name The name of the interval variable.
   * @return an IntervalVar object.
   */
  public IntervalVar newOptionalIntervalVar(
      IntVar start, IntVar size, IntVar end, ILiteral isPresent, String name) {
    return new IntervalVar(
        builder_, start.getIndex(), size.getIndex(), end.getIndex(), isPresent.getIndex(), name);
  }

  /**
   * Creates an optional interval with a fixed end.
   *
   * @see #newOptionalIntervalVar(IntVar, IntVar, IntVar, ILiteral, String) newOptionalIntervalVar
   */
  public IntervalVar newOptionalIntervalVar(
      IntVar start, IntVar size, long end, ILiteral isPresent, String name) {
    return new IntervalVar(
        builder_,
        start.getIndex(),
        size.getIndex(),
        indexFromConstant(end),
        isPresent.getIndex(),
        name);
  }

  /**
   * Creates an optional interval with a fixed size.
   *
   * @see #newOptionalIntervalVar(IntVar, IntVar, IntVar, ILiteral, String) newOptionalIntervalVar
   */
  public IntervalVar newOptionalIntervalVar(
      IntVar start, long size, IntVar end, ILiteral isPresent, String name) {
    return new IntervalVar(
        builder_,
        start.getIndex(),
        indexFromConstant(size),
        end.getIndex(),
        isPresent.getIndex(),
        name);
  }

  /** Creates an optional interval with a fixed start. */
  public IntervalVar newOptionalIntervalVar(
      long start, IntVar size, IntVar end, ILiteral isPresent, String name) {
    return new IntervalVar(
        builder_,
        indexFromConstant(start),
        size.getIndex(),
        end.getIndex(),
        isPresent.getIndex(),
        name);
  }

  /**
   * Creates an optional fixed interval from start and size.
   *
   * @see #newOptionalIntervalVar(IntVar, IntVar, IntVar, ILiteral, String) newOptionalIntervalVar
   */
  public IntervalVar newOptionalFixedInterval(
      long start, long size, ILiteral isPresent, String name) {
    return new IntervalVar(
        builder_,
        indexFromConstant(start),
        indexFromConstant(size),
        indexFromConstant(start + size),
        isPresent.getIndex(),
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
   * <p>This constraint enforces that: for all t: sum(demands[i] if (start(intervals[t]) &lt;= t
   * &lt; end(intervals[t])) and (t is present)) &lt;= capacity
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
   * @see #addCumulative(IntervalVar[], IntVar[], IntVar) AddCumulative
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
   * @see #addCumulative(IntervalVar[], IntVar[], IntVar) AddCumulative
   */
  public Constraint addCumulative(IntervalVar[] intervals, int[] demands, IntVar capacity) {
    return addCumulative(intervals, toLongArray(demands), capacity);
  }

  /**
   * Adds Cumulative(intervals, demands, capacity). with fixed capacity.
   *
   * @see #addCumulative(IntervalVar[], IntVar[], IntVar) AddCumulative
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
   * @see #addCumulative(IntervalVar[], IntVar[], IntVar) AddCumulative
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
   * @see #addCumulative(IntervalVar[], IntVar[], IntVar) AddCumulative
   */
  public Constraint addCumulative(IntervalVar[] intervals, int[] demands, long capacity) {
    return addCumulative(intervals, toLongArray(demands), capacity);
  }

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
