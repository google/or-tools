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

namespace Google.OrTools.Sat
{
using System;
using System.Collections.Generic;
using Google.OrTools.Util;

/// <summary>
///  Wrapper class around the cp_model proto.
/// </summary>
public class CpModel
{
    public CpModel()
    {
        model_ = new CpModelProto();
        constant_map_ = new Dictionary<long, int>();
        var_value_map_ = new Dictionary<int, long>(10);
        terms_ = new Queue<Term>(10);
    }

    // Getters.

    /**
     * <summary>
     * The underlying CpModelProto.
     * </summary>
     */
    public CpModelProto Model
    {
        get {
            return model_;
        }
    }

    int Negated(int index)
    {
        return -index - 1;
    }

    // Integer variables and constraints.

    /**
     * <summary>
     * Creates an integer variable with domain [lb, ub].
     * </summary>
     *
     * <param name="lb"> the lower bound of the domain</param>
     * <param name="ub"> the upper bound of the domain</param>
     * <param name="name"> the name of the variable</param>
     * <returns> a variable with the given domain</returns>
     */
    public IntVar NewIntVar(long lb, long ub, string name)
    {
        return new IntVar(model_, lb, ub, name);
    }

    /**
     * <summary>
     * Creates an integer variable with given domain.
     * </summary>
     *
     * <param name="domain"> an instance of the Domain class</param>
     * <param name="name"> the name of the variable</param>
     * <returns> a variable with the given domain</returns>
     */
    public IntVar NewIntVarFromDomain(Domain domain, string name)
    {
        return new IntVar(model_, domain, name);
    }

    /**
     * <summary>
     * Creates a constant variable.
     * </summary>
     */
    public IntVar NewConstant(long value)
    {
        return new IntVar(model_, ConvertConstant(value));
    }

    /**
     * <summary>
     * Creates a Boolean variable with given domain.
     * </summary>
     */
    public BoolVar NewBoolVar(string name)
    {
        return new BoolVar(model_, name);
    }

    /**
     * <summary>
     * Returns a constant true literal.
     * </summary>
     */
    public ILiteral TrueLiteral()
    {
        return true_literal_ ??= new BoolVar(model_, ConvertConstantWithName(1, "true"));
    }

    /**
     * <summary>
     * Returns a constant false literal.
     * </summary>
     */
    public ILiteral FalseLiteral()
    {
        return TrueLiteral().Not();
    }

    private long FillLinearConstraint(LinearExpr expr, out LinearConstraintProto linear)
    {
        var dict = var_value_map_;
        dict.Clear();
        long constant = LinearExpr.GetVarValueMap(expr, dict, terms_);
        var count = dict.Count;
        linear = new LinearConstraintProto();
        linear.Vars.Capacity = count;
        linear.Vars.AddRange(dict.Keys);
        linear.Coeffs.Capacity = count;
        linear.Coeffs.AddRange(dict.Values);
        return constant;
    }
    /**
     * <summary>
     * Adds <c>lb ≤ expr ≤ ub</c>.
     * </summary>
     */
    public Constraint AddLinearConstraint(LinearExpr expr, long lb, long ub)
    {
        long constant = FillLinearConstraint(expr, out var linear);
        linear.Domain.Capacity = 2;
        linear.Domain.Add(lb is Int64.MinValue or Int64.MaxValue ? lb : lb - constant);
        linear.Domain.Add(ub is Int64.MinValue or Int64.MaxValue ? ub : ub - constant);

        Constraint ct = new Constraint(model_);
        ct.Proto.Linear = linear;
        return ct;
    }

    /**
     * <summary>
     * Adds <c>expr ∈ domain</c>.
     * </summary>
     */
    public Constraint AddLinearExpressionInDomain(LinearExpr expr, Domain domain)
    {
        long constant = FillLinearConstraint(expr, out var linear);
        var array = domain.FlattenedIntervals();
        linear.Domain.Capacity = array.Length;
        foreach (long value in array)
        {
            linear.Domain.Add(value is Int64.MinValue or Int64.MaxValue ? value : value - constant);
        }

        Constraint ct = new Constraint(model_);
        ct.Proto.Linear = linear;
        return ct;
    }

    private Constraint AddLinearExpressionNotEqualCst(LinearExpr expr, long value)
    {
        long constant = FillLinearConstraint(expr, out var linear);
        linear.Domain.Capacity = 4;
        linear.Domain.Add(Int64.MinValue);
        linear.Domain.Add(value - constant - 1);
        linear.Domain.Add(value - constant + 1);
        linear.Domain.Add(Int64.MaxValue);

        Constraint ct = new Constraint(model_);
        ct.Proto.Linear = linear;
        return ct;
    }

    /**
     * <summary>
     * Adds a linear constraint to the model.
     * </summary>
     */
    public Constraint Add(BoundedLinearExpression lin)
    {
        switch (lin.CtType)
        {
        case BoundedLinearExpression.Type.BoundExpression: {
            return AddLinearConstraint(lin.Left, lin.Lb, lin.Ub);
        }
        case BoundedLinearExpression.Type.VarEqVar: {
            return AddLinearConstraint(lin.Left - lin.Right, 0, 0);
        }
        case BoundedLinearExpression.Type.VarDiffVar: {
            return AddLinearExpressionNotEqualCst(lin.Left - lin.Right, 0);
        }
        case BoundedLinearExpression.Type.VarEqCst: {
            return AddLinearConstraint(lin.Left, lin.Lb, lin.Lb);
        }
        case BoundedLinearExpression.Type.VarDiffCst: {
            return AddLinearExpressionNotEqualCst(lin.Left, lin.Lb);
        }
        }
        return null;
    }

    /**
     * <summary>
     * Adds the constraint <c>AllDifferent(exprs)</c>.
     * </summary>
     */
    public Constraint AddAllDifferent(IEnumerable<LinearExpr> exprs)
    {
        AllDifferentConstraintProto alldiff = new AllDifferentConstraintProto();
        alldiff.Exprs.TrySetCapacity(exprs);
        foreach (LinearExpr expr in exprs)
        {
            alldiff.Exprs.Add(GetLinearExpressionProto(expr));
        }

        Constraint ct = new Constraint(model_);
        ct.Proto.AllDiff = alldiff;
        return ct;
    }

    /**
     * <summary>
     * Adds the element constraint: <c>exprs[index] == target</c>.
     * </summary>
     */
    public Constraint AddElement(LinearExpr index, IEnumerable<LinearExpr> exprs, LinearExpr target)
    {
        ElementConstraintProto element = new ElementConstraintProto();
        element.LinearIndex = GetLinearExpressionProto(index);
        element.Exprs.TrySetCapacity(exprs);
        foreach (LinearExpr expr in exprs)
        {
            element.Exprs.Add(GetLinearExpressionProto(expr));
        }
        element.LinearTarget = GetLinearExpressionProto(target);

        Constraint ct = new Constraint(model_);
        ct.Proto.Element = element;
        return ct;
    }

    /**
     * <summary>
     * Adds the element constraint: <c> values[index] == target</c>.
     * </summary>
     */
    public Constraint AddElement(LinearExpr index, IEnumerable<long> values, LinearExpr target)
    {
        ElementConstraintProto element = new ElementConstraintProto();
        element.LinearIndex = GetLinearExpressionProto(index);
        element.Exprs.TrySetCapacity(values);
        foreach (long value in values)
        {
            element.Exprs.Add(GetLinearExpressionProto(value));
        }
        element.LinearTarget = GetLinearExpressionProto(target);

        Constraint ct = new Constraint(model_);
        ct.Proto.Element = element;
        return ct;
    }

    /**
     * <summary>
     * Adds the element constraint: <c> values[index] == target</c>.
     * </summary>
     */
    public Constraint AddElement(LinearExpr index, IEnumerable<int> values, LinearExpr target)
    {
        ElementConstraintProto element = new ElementConstraintProto();
        element.LinearIndex = GetLinearExpressionProto(index);
        element.Exprs.TrySetCapacity(values);
        foreach (int value in values)
        {
            element.Exprs.Add(GetLinearExpressionProto(value));
        }
        element.LinearTarget = GetLinearExpressionProto(target);

        Constraint ct = new Constraint(model_);
        ct.Proto.Element = element;
        return ct;
    }

    /**
     * <summary>
     * Adds and returns an empty circuit constraint.
     * </summary>
     *
     * <remarks> A circuit is a unique Hamiltonian path in a subgraph of the total graph. In case a node <c>i</c>
     * is not in the path, then there must be a loop arc <c> i -> i</c> associated with a true
     * literal. Otherwise this constraint will fail.
     * </remarks>
     */
    public CircuitConstraint AddCircuit()
    {
        CircuitConstraint ct = new CircuitConstraint(model_);
        ct.Proto.Circuit = new CircuitConstraintProto();
        return ct;
    }

    /**
     * <summary>
     * Adds and returns an empty multiple circuit constraint.
     * </summary>
     *
     * <remarks> A multiple circuit is set of cycles in a subgraph of the total graph. The node index by 0
     * must be part of all cycles of length > 1. Each node with index > 0 belongs to exactly one
     * cycle. If such node does not belong in any cycle of length > 1, then there must be a looping
     * arc on this node attached to a literal that will be true. Otherwise, the constraint will fail.
     * </remarks>
     */
    public MultipleCircuitConstraint AddMultipleCircuit()
    {
        MultipleCircuitConstraint ct = new MultipleCircuitConstraint(model_);
        ct.Proto.Routes = new RoutesConstraintProto();
        return ct;
    }

    /**
     * <summary>
     * Adds <c> AllowedAssignments(expressions)</c>.
     * </summary>
     *
     * <remarks>An AllowedAssignments constraint is a constraint on an array of affine
     * expressions (a * var + b) that forces, when all expressions are fixed to a single
     * value, that the corresponding list of values is equal to one of the tuples of the
     * tupleList.
     * </remarks>
     *
     * <param name="exprs"> a list of affine expressions (a * var + b)</param>
     * <returns> an instance of the TableConstraint class without any tuples. Tuples can be added
     * directly to the table constraint
     * </returns>
     */
    public TableConstraint AddAllowedAssignments(IEnumerable<LinearExpr> exprs)
    {
        TableConstraintProto table = new TableConstraintProto();
        table.Vars.TrySetCapacity(exprs);
        foreach (LinearExpr expr in exprs)
        {
            table.Exprs.Add(GetLinearExpressionProto(expr));
        }

        TableConstraint ct = new TableConstraint(model_);
        ct.Proto.Table = table;
        return ct;
    }

    /**
     * <summary>
     * Adds <c> ForbiddenAssignments(variables)</c>.
     * </summary>
     *
     * <remarks>A ForbiddenAssignments constraint is a constraint on an array of affine
     * expressions (a * var + b) where the list of impossible combinations is provided
     * in the tuples list.
     * </remarks>
     *
     * <param name="exprs"> a list of affine expressions (a * var + b)</param>
     * <returns> an instance of the TableConstraint class without any tuples. Tuples can be added
     * directly to the table constraint
     * </returns>
     */
    public TableConstraint AddForbiddenAssignments(IEnumerable<LinearExpr> exprs)
    {
        TableConstraint ct = AddAllowedAssignments(exprs);
        ct.Proto.Table.Negated = true;
        return ct;
    }

    /**
     * <summary>
     * Adds an automaton constraint.
     * </summary>
     *
     * <remarks>
     * <para>An automaton constraint takes a list of affine expressions (of size n), an initial
     * state, a set of final states, and a set of transitions that will be added incrementally
     * directly on the returned AutomatonConstraint instance. A transition is a triplet
     * (<c>tail</c>, <c>transition</c>, <c>head</c>), where <c>tail</c> and <c>head</c> are states,
     * and <c>transition</c> is the label of an arc from <c>head</c> to <c>tail</c>, corresponding
     * to the value of one expression in the list of expressions. </para>
     *
     * <para>This automaton will be unrolled into a flow with n + 1 phases. Each phase contains the
     * possible states of the automaton. The first state contains the initial state. The last phase
     * contains the final states. </para>
     *
     * <para>Between two consecutive phases i and i + 1, the automaton creates a set of arcs. For
     * each transition (<c>tail</c>, <c>label</c>, <c>head</c>), it will add an arc from the state
     * <c>tail</c> of phase i and the state <c>head</c> of phase i + 1. This arc labeled by the
     * value <c>label</c> of the expression <c>expressions[i]</c>. That is, this arc can only be
     * selected <c>expressions[i]</c>a is assigned the value <c>label</c>. </para>
     *
     * <para>A feasible solution of this constraint is an assignment of expression such that,
     * starting from the initial state in phase 0, there is a path labeled by the values of the
     * expressions that ends in one of the final states in the final phase. </para>
     * </remarks>
     *
     * <param name="expressions"> a non empty list of affine expressions (a * var + b) whose values
     * correspond to the labels of the arcs traversed by the automaton</param> <param
     * name="starting_state"> the initial state of the automaton</param> <param name="final_states">
     * a non empty list of admissible final states </param> <returns> an instance of the
     * AutomatonConstraint class </returns>
     */
    public AutomatonConstraint AddAutomaton(IEnumerable<LinearExpr> expressions, long starting_state,
                                            IEnumerable<long> final_states)
    {
        AutomatonConstraintProto automaton = new AutomatonConstraintProto();
        automaton.Vars.TrySetCapacity(expressions);
        foreach (LinearExpr expr in exprs)
        {
            automaton.Exprs.Add(GetLinearExpressionProto(expr));
        }

        automaton.StartingState = starting_state;
        automaton.FinalStates.AddRange(final_states);

        AutomatonConstraint ct = new AutomatonConstraint(model_);
        ct.Proto.Automaton = automaton;
        return ct;
    }

    /**
     * <summary>
     * Adds <c> Inverse(variables, inverseVariables)</c>.
     * </summary>
     *
     * <remarks>An inverse constraint enforces that if <c>direct[i] == j</c>, then
     * <c>reverse[j] == i</c>, and vice versa.
     * </remarks>
     *
     * <param name="direct"> an array of integer variables</param>
     * <param name="reverse"> an array of integer variables</param>
     * <returns> an instance of the Constraint class </returns>
     */
    public Constraint AddInverse(IEnumerable<IntVar> direct, IEnumerable<IntVar> reverse)
    {
        InverseConstraintProto inverse = new InverseConstraintProto();
        inverse.FDirect.TrySetCapacity(direct);
        foreach (IntVar var in direct)
        {
            inverse.FDirect.Add(var.Index);
        }

        inverse.FInverse.TrySetCapacity(reverse);
        foreach (IntVar var in reverse)
        {
            inverse.FInverse.Add(var.Index);
        }

        Constraint ct = new Constraint(model_);
        ct.Proto.Inverse = inverse;
        return ct;
    }

    /**
     * <summary>
     * Adds a reservoir constraint with optional refill/emptying events.
     * </summary>
     *
     * <remarks>
     * <para>Maintain a reservoir level within bounds. The water level starts at 0, and at any time, it
     * must be within [min_level, max_level].
     * </para>
     *
     * <para>Given an event (time, levelChange, active), if active is true, and if time is assigned a
     * value t, then the level of the reservoir changes by levelChange (which is constant) at time t.
     * Therefore, at any time t:
     * <code>∑(levelChanges[i] * actives[i] if times[i] ≤ t) in [min_level, max_level]</code>
     * </para>
     *
     * <para>Note that min level must be ≤ 0, and the max level must be ≥ 0. Please use fixed
     * level_changes to simulate an initial state. </para>
     * </remarks>
     *
     * <param name="minLevel"> at any time, the level of the reservoir must be greater of equal than the min
     *     level. minLevel must me ≤ 0 </param>
     * <param name="maxLevel"> at any time, the level of the reservoir must be less or equal than the max
     *     level. maxLevel must be ≥ 0 </param>
     * <returns> an instance of the ReservoirConstraint class </returns>
     */
    public ReservoirConstraint AddReservoirConstraint(long minLevel, long maxLevel)
    {
        ReservoirConstraintProto res = new ReservoirConstraintProto();

        res.MinLevel = minLevel;
        res.MaxLevel = maxLevel;

        ReservoirConstraint ct = new ReservoirConstraint(this, model_);
        ct.Proto.Reservoir = res;
        return ct;
    }

    /**
     * <summary>
     * Adds <c>var == i + offset ⇔ bool_vars[i] == true for all i</c>.
     * </summary>
     */
    public void AddMapDomain(IntVar var, IEnumerable<IntVar> bool_vars, long offset = 0)
    {
        int i = 0;
        int var_index = var.Index;
        foreach (IntVar bool_var in bool_vars)
        {
            int b_index = bool_var.Index;

            LinearConstraintProto lin1 = new LinearConstraintProto();
            lin1.Vars.Capacity = 1;
            lin1.Vars.Add(var_index);
            lin1.Coeffs.Capacity = 1;
            lin1.Coeffs.Add(1L);
            lin1.Domain.Capacity = 2;
            lin1.Domain.Add(offset + i);
            lin1.Domain.Add(offset + i);
            ConstraintProto ct1 = new ConstraintProto();
            ct1.Linear = lin1;
            ct1.EnforcementLiteral.Add(b_index);
            model_.Constraints.Add(ct1);

            LinearConstraintProto lin2 = new LinearConstraintProto();
            lin2.Vars.Capacity = 1;
            lin2.Vars.Add(var_index);
            lin2.Coeffs.Capacity = 1;
            lin2.Coeffs.Add(1L);
            lin2.Domain.Capacity = 4;
            lin2.Domain.Add(Int64.MinValue);
            lin2.Domain.Add(offset + i - 1);
            lin2.Domain.Add(offset + i + 1);
            lin2.Domain.Add(Int64.MaxValue);
            ConstraintProto ct2 = new ConstraintProto();
            ct2.Linear = lin2;
            ct2.EnforcementLiteral.Add(-b_index - 1);
            model_.Constraints.Add(ct2);

            i++;
        }
    }

    /**
     * <summary>
     * Adds <c>a ⇒ b</c>.
     * </summary>
     */
    public Constraint AddImplication(ILiteral a, ILiteral b)
    {
        BoolArgumentProto or = new BoolArgumentProto();
        or.Literals.Capacity = 2;
        or.Literals.Add(a.Not().GetIndex());
        or.Literals.Add(b.GetIndex());

        Constraint ct = new Constraint(model_);
        ct.Proto.BoolOr = or;
        return ct;
    }

    /**
     * <summary>
     * Adds <c>Or(literals) == true</c>.
     * </summary>
     */
    public Constraint AddBoolOr(IEnumerable<ILiteral> literals)
    {
        BoolArgumentProto bool_argument = new BoolArgumentProto();
        bool_argument.Literals.TrySetCapacity(literals);
        foreach (ILiteral lit in literals)
        {
            bool_argument.Literals.Add(lit.GetIndex());
        }

        Constraint ct = new Constraint(model_);
        ct.Proto.BoolOr = bool_argument;
        return ct;
    }

    /**
     * <summary>
     * Same as AddBoolOr: <c>∑(literals) ≥ 1</c>.
     * </summary>
     */
    public Constraint AddAtLeastOne(IEnumerable<ILiteral> literals)
    {
        return AddBoolOr(literals);
    }

    /**
     * <summary>
     * Adds <c> AtMostOne(literals): ∑(literals) ≤ 1</c>.
     * </summary>
     */
    public Constraint AddAtMostOne(IEnumerable<ILiteral> literals)
    {
        BoolArgumentProto bool_argument = new BoolArgumentProto();
        bool_argument.Literals.TrySetCapacity(literals);
        foreach (ILiteral lit in literals)
        {
            bool_argument.Literals.Add(lit.GetIndex());
        }

        Constraint ct = new Constraint(model_);
        ct.Proto.AtMostOne = bool_argument;
        return ct;
    }

    /**
     * <summary>
     * Adds <c> ExactlyOne(literals): ∑(literals) == 1</c>.
     * </summary>
     */
    public Constraint AddExactlyOne(IEnumerable<ILiteral> literals)
    {
        BoolArgumentProto bool_argument = new BoolArgumentProto();
        bool_argument.Literals.TrySetCapacity(literals);
        foreach (ILiteral lit in literals)
        {
            bool_argument.Literals.Add(lit.GetIndex());
        }

        Constraint ct = new Constraint(model_);
        ct.Proto.ExactlyOne = bool_argument;
        return ct;
    }

    /**
     * <summary>
     * Adds <c>And(literals) == true</c>.
     * </summary>
     */
    public Constraint AddBoolAnd(IEnumerable<ILiteral> literals)
    {
        BoolArgumentProto bool_argument = new BoolArgumentProto();
        bool_argument.Literals.TrySetCapacity(literals);
        foreach (ILiteral lit in literals)
        {
            bool_argument.Literals.Add(lit.GetIndex());
        }

        Constraint ct = new Constraint(model_);
        ct.Proto.BoolAnd = bool_argument;
        return ct;
    }

    /**
     * <summary>
     * Adds <c>XOr(literals) == true</c>.
     * </summary>
     */
    public Constraint AddBoolXor(IEnumerable<ILiteral> literals)
    {
        BoolArgumentProto bool_argument = new BoolArgumentProto();
        bool_argument.Literals.TrySetCapacity(literals);
        foreach (ILiteral lit in literals)
        {
            bool_argument.Literals.Add(lit.GetIndex());
        }

        Constraint ct = new Constraint(model_);
        ct.Proto.BoolXor = bool_argument;
        return ct;
    }

    /**
     * <summary>
     * Adds <c>target == Min(exprs)</c>.
     * </summary>
     */
    public Constraint AddMinEquality(LinearExpr target, IEnumerable<LinearExpr> exprs)
    {
        LinearArgumentProto lin = new LinearArgumentProto();
        lin.Exprs.TrySetCapacity(exprs);
        foreach (LinearExpr expr in exprs)
        {
            lin.Exprs.Add(GetLinearExpressionProto(expr, /*negate=*/true));
        }
        lin.Target = GetLinearExpressionProto(target, /*negate=*/true);

        Constraint ct = new Constraint(model_);
        ct.Proto.LinMax = lin;
        return ct;
    }

    /**
     * <summary>
     * Adds <c>target == Max(exprs)</c>.
     * </summary>
     */
    public Constraint AddMaxEquality(LinearExpr target, IEnumerable<LinearExpr> exprs)
    {
        LinearArgumentProto lin = new LinearArgumentProto();
        lin.Exprs.TrySetCapacity(exprs);
        foreach (LinearExpr expr in exprs)
        {
            lin.Exprs.Add(GetLinearExpressionProto(expr));
        }
        lin.Target = GetLinearExpressionProto(target);

        Constraint ct = new Constraint(model_);
        ct.Proto.LinMax = lin;
        return ct;
    }

    /**
     * <summary>
     * Adds <c>target == num / denom</c> (integer division rounded towards 0).
     * </summary>
     */
    public Constraint AddDivisionEquality<T, N, D>(T target, N num, D denom)
    {
        LinearArgumentProto div = new LinearArgumentProto();
        div.Exprs.Capacity = 2;
        div.Exprs.Add(GetLinearExpressionProto(GetLinearExpr(num)));
        div.Exprs.Add(GetLinearExpressionProto(GetLinearExpr(denom)));
        div.Target = GetLinearExpressionProto(GetLinearExpr(target));

        Constraint ct = new Constraint(model_);
        ct.Proto.IntDiv = div;
        return ct;
    }

    /**
     * <summary>
     * Adds <c>target == abs(expr)</c>.
     * </summary>
     */
    public Constraint AddAbsEquality(LinearExpr target, LinearExpr expr)
    {
        LinearArgumentProto abs = new LinearArgumentProto();
        abs.Exprs.Capacity = 2;
        abs.Exprs.Add(GetLinearExpressionProto(expr));
        abs.Exprs.Add(GetLinearExpressionProto(expr, /*negate=*/true));
        abs.Target = GetLinearExpressionProto(target);

        Constraint ct = new Constraint(model_);
        ct.Proto.LinMax = abs;
        return ct;
    }

    /**
     * <summary>
     * Adds <c>target == v % m</c>.
     * </summary>
     */
    public Constraint AddModuloEquality<T, V, M>(T target, V v, M m)
    {
        LinearArgumentProto mod = new LinearArgumentProto();
        mod.Exprs.Capacity = 2;
        mod.Exprs.Add(GetLinearExpressionProto(GetLinearExpr(v)));
        mod.Exprs.Add(GetLinearExpressionProto(GetLinearExpr(m)));
        mod.Target = GetLinearExpressionProto(GetLinearExpr(target));

        Constraint ct = new Constraint(model_);
        ct.Proto.IntMod = mod;
        return ct;
    }

    /**
     * <summary>
     * Adds <c>target == ∏(exprs)</c>.
     * </summary>
     */
    public Constraint AddMultiplicationEquality(LinearExpr target, IEnumerable<LinearExpr> exprs)
    {
        LinearArgumentProto prod = new LinearArgumentProto();
        prod.Target = GetLinearExpressionProto(target);
        prod.Exprs.TrySetCapacity(exprs);
        foreach (LinearExpr expr in exprs)
        {
            prod.Exprs.Add(GetLinearExpressionProto(expr));
        }

        Constraint ct = new Constraint(model_);
        ct.Proto.IntProd = prod;
        return ct;
    }

    /**
     * <summary>
     * Adds <c>target == left * right</c>.
     * </summary>
     */
    public Constraint AddMultiplicationEquality(LinearExpr target, LinearExpr left, LinearExpr right)
    {
        LinearArgumentProto prod = new LinearArgumentProto();
        prod.Target = GetLinearExpressionProto(target);
        prod.Exprs.Capacity = 2;
        prod.Exprs.Add(GetLinearExpressionProto(left));
        prod.Exprs.Add(GetLinearExpressionProto(right));

        Constraint ct = new Constraint(model_);
        ct.Proto.IntProd = prod;
        return ct;
    }

    // Scheduling support

    /**
     * <summary>
     * Creates an interval variable from three affine expressions start, size, and end.
     * </summary>
     *
     * <remarks>An interval variable is a constraint, that is itself used in other constraints like
     * NoOverlap. Internally, it ensures that <c>start + size == end</c>.
     * </remarks>
     *
     * <param name="start"> the start of the interval. It needs to be an affine or constant expression. </param>
     * <param name="size"> the size of the interval. It needs to be an affine or constant expression. </param>
     * <param name="end"> the end of the interval. It needs to be an affine or constant expression. </param>
     * <param name="name"> the name of the interval variable </param>
     * <returns> An IntervalVar object</returns>
     */
    public IntervalVar NewIntervalVar<S, D, E>(S start, D size, E end, string name)
    {
        LinearExpr startExpr = GetLinearExpr(start);
        LinearExpr sizeExpr = GetLinearExpr(size);
        LinearExpr endExpr = GetLinearExpr(end);

        LinearExpressionProto startProto = GetLinearExpressionProto(startExpr);
        LinearExpressionProto sizeProto = GetLinearExpressionProto(sizeExpr);
        LinearExpressionProto endProto = GetLinearExpressionProto(endExpr);
        return new IntervalVar(model_, startProto, sizeProto, endProto, name);
    }

    /**
     * <summary>
     * Creates an interval variable from an affine expression start, and a fixed size.
     * </summary>
     *
     * <remarks>An interval variable is a constraint, that is itself used in other constraints like
     * NoOverlap.
     * </remarks>
     *
     * <param name="start"> the start of the interval. It needs to be an affine or constant expression.         *
     * </param>
     * <param name="size"> the fixed size of the interval </param>
     * <param name="name"> the name of the interval variable </param>
     * <returns> An IntervalVar object</returns>
     */
    public IntervalVar NewFixedSizeIntervalVar<S>(S start, long size, string name)
    {
        LinearExpr startExpr = GetLinearExpr(start);
        LinearExpr sizeExpr = GetLinearExpr(size);
        LinearExpr endExpr = LinearExpr.Sum(new LinearExpr[] { startExpr, sizeExpr });

        LinearExpressionProto startProto = GetLinearExpressionProto(startExpr);
        LinearExpressionProto sizeProto = GetLinearExpressionProto(sizeExpr);
        LinearExpressionProto endProto = GetLinearExpressionProto(endExpr);
        return new IntervalVar(model_, startProto, sizeProto, endProto, name);
    }

    /**
     * <summary>
     * Creates an optional interval variable from three affine expressions start, size, and end, and a literal
     * is_present.
     * </summary>
     *
     * <remarks>An interval variable is a constraint, that is itself used in other constraints like
     * NoOverlap. Internally, it ensures that <c>is_present ⇒ start + size == end</c>.
     * </remarks>
     *
     * <param name="start"> the start of the interval. It needs to be an affine or constant expression. </param>
     * <param name="size"> the size of the interval. It needs to be an affine or constant expression. </param>
     * <param name="end"> the end of the interval. It needs to be an affine or constant expression. </param>
     * <param name="is_present"> a literal that indicates if the interval is active or not. A inactive interval
     *     is simply ignored by all constraints.</param>
     * <param name="name"> the name of the interval variable </param>
     * <returns> An IntervalVar object</returns>
     */
    public IntervalVar NewOptionalIntervalVar<S, D, E>(S start, D size, E end, ILiteral is_present, string name)
    {
        LinearExpr startExpr = GetLinearExpr(start);
        LinearExpr sizeExpr = GetLinearExpr(size);
        LinearExpr endExpr = GetLinearExpr(end);

        LinearExpressionProto startProto = GetLinearExpressionProto(startExpr);
        LinearExpressionProto sizeProto = GetLinearExpressionProto(sizeExpr);
        LinearExpressionProto endProto = GetLinearExpressionProto(endExpr);
        return new IntervalVar(model_, startProto, sizeProto, endProto, is_present.GetIndex(), name);
    }

    /**
     * <summary>
     * Creates an optional interval variable from an affine expression start, a fixed size, and a literal
     * is_present.
     * </summary>
     *
     * <remarks>An interval variable is a constraint, that is itself used in other constraints like
     * NoOverlap.
     * </remarks>
     *
     * <param name="start"> the start of the interval. It needs to be an affine or constant expression.         *
     * </param>
     * <param name="size"> the fixed size of the interval </param>
     * <param name="is_present"> a literal that indicates if the interval is active or not. A inactive interval
     *     is simply ignored by all constraints.</param>
     * <param name="name"> the name of the interval variable </param>
     * <returns> An IntervalVar object</returns>
     */
    public IntervalVar NewOptionalFixedSizeIntervalVar<S>(S start, long size, ILiteral is_present, string name)
    {
        LinearExpr startExpr = GetLinearExpr(start);
        LinearExpr sizeExpr = GetLinearExpr(size);
        LinearExpr endExpr = LinearExpr.Sum(new LinearExpr[] { startExpr, sizeExpr });

        LinearExpressionProto startProto = GetLinearExpressionProto(startExpr);
        LinearExpressionProto sizeProto = GetLinearExpressionProto(sizeExpr);
        LinearExpressionProto endProto = GetLinearExpressionProto(endExpr);
        return new IntervalVar(model_, startProto, sizeProto, endProto, is_present.GetIndex(), name);
    }

    /**
     * Adds <c> NoOverlap(intervalVars)</c>.
     *
     * <remarks>A NoOverlap constraint ensures that all present intervals do not overlap in time.
     * </remarks>
     *
     * <param name="intervals"> the list of interval variables to constrain</param>
     * <returns> an instance of the Constraint class</returns>
     */
    public Constraint AddNoOverlap(IEnumerable<IntervalVar> intervals)
    {
        NoOverlapConstraintProto no_overlap = new NoOverlapConstraintProto();
        no_overlap.Intervals.TrySetCapacity(intervals);
        foreach (IntervalVar var in intervals)
        {
            no_overlap.Intervals.Add(var.GetIndex());
        }

        Constraint ct = new Constraint(model_);
        ct.Proto.NoOverlap = no_overlap;
        return ct;
    }

    /**
     * <summary>
     * Adds <c>NoOverlap2D()</c>.
     * </summary>
     *
     * <remarks>
     * <para>A NoOverlap2D constraint ensures that all present rectangles do not overlap on a plan. Each
     * rectangle is aligned with the X and Y axis, and is defined by two intervals which represent its
     * projection onto the X and Y axis. </para>
     *
     * <para>Furthermore, one box is optional if at least one of the x or y interval is optional. </para>
     * </remarks>
     *
     * <returns> an instance of the NoOverlap2dConstraint class. This class allows adding rectangles
     *     incrementally</returns>
     */
    public NoOverlap2dConstraint AddNoOverlap2D()
    {
        NoOverlap2dConstraint ct = new NoOverlap2dConstraint(model_);
        ct.Proto.NoOverlap2D = new NoOverlap2DConstraintProto();
        return ct;
    }

    /**
     * <summary>
     * Adds <c>Cumulative(capacity)</c>.
     * </summary>
     *
     * <remarks>
     * This constraint enforces that:
     * <code>
     * forall t:
     *      ∑( demands[i] if (start(intervals[t]) ≤ t &lt; end(intervals[t])) and
     *                       (t is present) ) ≤ capacity
     * </code>
     * </remarks>
     *
     * <param name="capacity"> the maximum capacity of the cumulative constraint. It must be a positive affine
     *     expression </param>
     * <returns> an instance of the CumulativeConstraint class. this class allows adding (interval,
     *     demand) pairs incrementally </returns>
     */
    public CumulativeConstraint AddCumulative<C>(C capacity)
    {
        CumulativeConstraintProto cumul = new CumulativeConstraintProto();
        LinearExpr capacityExpr = GetLinearExpr(capacity);
        cumul.Capacity = GetLinearExpressionProto(capacityExpr);

        CumulativeConstraint ct = new CumulativeConstraint(this, model_);
        ct.Proto.Cumulative = cumul;
        return ct;
    }

    // Objective.

    /** <summary>Adds a minimization objective of a linear expression.</summary>*/
    public void Minimize(LinearExpr obj)
    {
        SetObjective(obj, true);
    }

    /** <summary>Adds a maximization objective of a linear expression.</summary>*/
    public void Maximize(LinearExpr obj)
    {
        SetObjective(obj, false);
    }

    /** <summary>Clears the objective.</summary>*/
    void ClearObjective()
    {
        model_.Objective = null;
    }

    /** <summary>Returns whether the model contains an objective.</summary>*/
    bool HasObjective()
    {
        return model_.Objective is not null || model_.FloatingPointObjective is not null;
    }

    // Search Decision.

    /** <summary>Adds <c> DecisionStrategy(variables, var_str, dom_str)</c>.</summary>. */
    public void AddDecisionStrategy(IEnumerable<IntVar> vars,
                                    DecisionStrategyProto.Types.VariableSelectionStrategy var_str,
                                    DecisionStrategyProto.Types.DomainReductionStrategy dom_str)
    {
        DecisionStrategyProto ds = new DecisionStrategyProto();
        ds.Variables.TrySetCapacity(vars);
        foreach (IntVar var in vars)
        {
            LinearExpressionProto expr = new LinearExpressionProto();
            expr.Vars.Add(var.Index);
            expr.Coeffs.Add(1);
            ds.Exprs.Add(expr);
        }
        ds.VariableSelectionStrategy = var_str;
        ds.DomainReductionStrategy = dom_str;
        model_.SearchStrategy.Add(ds);
    }

    /** <summary>Adds variable hinting to the model.</summary>*/
    public void AddHint(IntVar var, long value)
    {
        model_.SolutionHint ??= new PartialVariableAssignment();
        model_.SolutionHint.Vars.Add(var.GetIndex());
        model_.SolutionHint.Values.Add(value);
    }

    /** <summary>Adds variable hinting to the model.</summary>*/
    public void AddHint(ILiteral lit, bool value)
    {
        model_.SolutionHint ??= new PartialVariableAssignment();
        int index = lit.GetIndex();
        if (index >= 0)
        {
            model_.SolutionHint.Vars.Add(index);
            model_.SolutionHint.Values.Add(value ? 1 : 0);
        }
        else
        {
            model_.SolutionHint.Vars.Add(Negated(index));
            model_.SolutionHint.Values.Add(value ? 0 : 1);
        }
    }

    /** <summary>Clears all hinting from the model.</summary>*/
    public void ClearHints()
    {
        model_.SolutionHint = null;
    }

    /** <summary>Adds a literal to the model as assumption.</summary>*/
    public void AddAssumption(ILiteral lit)
    {
        model_.Assumptions.Add(lit.GetIndex());
    }

    /** <summary>Adds multiple literals to the model as assumptions.</summary>*/
    public void AddAssumptions(IEnumerable<ILiteral> literals)
    {
        foreach (ILiteral lit in literals)
        {
            AddAssumption(lit);
        }
    }

    /** <summary>Clears all assumptions from the model.</summary>*/
    public void ClearAssumptions()
    {
        model_.Assumptions.Clear();
    }

    // Internal methods.

    void SetObjective(LinearExpr obj, bool minimize)
    {
        CpObjectiveProto objective = new CpObjectiveProto();
        if (obj is null)
        {
            objective.Offset = 0L;
            objective.ScalingFactor = minimize ? 1L : -1;
        }
        else if (obj is IntVar intVar)
        {
            objective.Offset = 0L;
            objective.Vars.Capacity = 1;
            objective.Vars.Add(intVar.Index);

            objective.Coeffs.Capacity = 1;
            if (minimize)
            {
                objective.Coeffs.Add(1L);
                objective.ScalingFactor = 1L;
            }
            else
            {
                objective.Coeffs.Add(-1L);
                objective.ScalingFactor = -1L;
            }
        }
        else
        {
            var dict = var_value_map_;
            dict.Clear();
            long constant = LinearExpr.GetVarValueMap(obj, dict, terms_);
            var dictCount = dict.Count;
            objective.Vars.Capacity = dictCount;
            objective.Vars.AddRange(dict.Keys);
            objective.Coeffs.Capacity = dictCount;
            if (minimize)
            {
                objective.Coeffs.AddRange(dict.Values);
                objective.ScalingFactor = 1L;
                objective.Offset = constant;
            }
            else
            {
                foreach (var coeff in dict.Values)
                {
                    objective.Coeffs.Add(-coeff);
                }
                objective.ScalingFactor = -1L;
                objective.Offset = -constant;
            }
        }
        model_.Objective = objective;
    }

    /** <summary>Returns some statistics on model as a string. </summary>*/
    public String ModelStats()
    {
        return CpSatHelper.ModelStats(model_);
    }

    /**
     * <summary>
     * Write the model as a protocol buffer to <c>file</c>.
     * </summary>
     *
     * <param name="file"> file to write the model to. If the filename ends with <c>txt</c>, the
     *    model will be written as a text file, otherwise, the binary format will be used. </param>
     *
     * <returns> true if the model was correctly written </returns>
     */
    public Boolean ExportToFile(String file)
    {
        return CpSatHelper.WriteModelToFile(model_, file);
    }

    /**
     * <summary>
     * Returns a non empty string explaining the issue if the model is invalid.
     * </summary>
     */
    public String Validate()
    {
        return CpSatHelper.ValidateModel(model_);
    }

    private int ConvertConstant(long value)
    {
        if (constant_map_.TryGetValue(value, out var index))
        {
            return index;
        }

        index = model_.Variables.Count;
        IntegerVariableProto var = new IntegerVariableProto();
        var.Domain.Capacity = 2;
        var.Domain.Add(value);
        var.Domain.Add(value);
        constant_map_.Add(value, index);
        model_.Variables.Add(var);
        return index;
    }

    private int ConvertConstantWithName(long value, string name)
    {
        if (constant_map_.TryGetValue(value, out var index))
        {
            return index;
        }

        index = model_.Variables.Count;
        IntegerVariableProto var = new IntegerVariableProto();
        var.Domain.Capacity = 2;
        var.Domain.Add(value);
        var.Domain.Add(value);
        var.Name = name;
        constant_map_.Add(value, index);
        model_.Variables.Add(var);
        return index;
    }

    internal LinearExpr GetLinearExpr<X>(X x)
    {
        if (typeof(X) == typeof(IntVar))
        {
            return (IntVar)(Object)x;
        }
        if (typeof(X) == typeof(long) || typeof(X) == typeof(int) || typeof(X) == typeof(short))
        {
            return LinearExpr.Constant(Convert.ToInt64(x));
        }
        if (typeof(X) == typeof(LinearExpr))
        {
            return (LinearExpr)(Object)x;
        }
        throw new ArgumentException("Cannot convert argument to LinearExpr");
    }

    internal LinearExpressionProto GetLinearExpressionProto(LinearExpr expr, bool negate = false)
    {
        var dict = var_value_map_;
        dict.Clear();
        long constant = LinearExpr.GetVarValueMap(expr, dict, terms_);

        LinearExpressionProto linear = new LinearExpressionProto();
        var dictCount = dict.Count;
        linear.Vars.Capacity = dictCount;
        linear.Vars.AddRange(dict.Keys);
        linear.Coeffs.Capacity = dictCount;
        if (negate)
        {
            foreach (var coeff in dict.Values)
            {
                linear.Coeffs.Add(-coeff);
            }
            linear.Offset = -constant;
        }
        else
        {
            linear.Coeffs.AddRange(dict.Values);
            linear.Offset = constant;
        }

        return linear;
    }

    internal LinearExpressionProto GetLinearExpressionProto(long value)
    {
        LinearExpressionProto linear = new LinearExpressionProto();
        linear.Offset = value;
        return linear;
    }

    private CpModelProto model_;
    private Dictionary<long, int> constant_map_;
    private Dictionary<int, long> var_value_map_;
    private BoolVar true_literal_;
    private Queue<Term> terms_;
}

} // namespace Google.OrTools.Sat
