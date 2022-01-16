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
     * Creates an Boolean variable with given domain.
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
        return true_literal_ ??= new BoolVar(model_, ConvertConstant(1));
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
        linear = new LinearConstraintProto();
        Dictionary<IntVar, long> dict = new Dictionary<IntVar, long>();
        long constant = LinearExpr.GetVarValueMap(expr, 1L, dict);
        foreach (KeyValuePair<IntVar, long> term in dict)
        {
            linear.Vars.Add(term.Key.Index);
            linear.Coeffs.Add(term.Value);
        }
        return constant;
    }
    /**
     * <summary>
     * Adds <code>lb \lt;= expr \lt;= ub</code>
     * </summary>
     */
    public Constraint AddLinearConstraint(LinearExpr expr, long lb, long ub)
    {
        long constant = FillLinearConstraint(expr, out var linear);
        linear.Domain.Add(lb is Int64.MinValue or Int64.MaxValue ? lb : lb - constant);
        linear.Domain.Add(ub is Int64.MinValue or Int64.MaxValue ? ub : ub - constant);

        Constraint ct = new Constraint(model_);
        ct.Proto.Linear = linear;
        return ct;
    }

    /**
     * <summary>
     * Adds <code>expr ∈ domain</code>
     * </summary>
     */
    public Constraint AddLinearExpressionInDomain(LinearExpr expr, Domain domain)
    {
        long constant = FillLinearConstraint(expr, out var linear);
        foreach (long value in domain.FlattenedIntervals())
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
     * Adds the constraint <code>AllDifferent(exprs)</code>
     * </summary>
     */
    public Constraint AddAllDifferent(IEnumerable<LinearExpr> exprs)
    {
        Constraint ct = new Constraint(model_);
        AllDifferentConstraintProto alldiff = new AllDifferentConstraintProto();
        foreach (LinearExpr expr in exprs)
        {
            alldiff.Exprs.Add(GetLinearExpressionProto(expr));
        }

        ct.Proto.AllDiff = alldiff;
        return ct;
    }

    /**
     * <summary>
     * Adds the element constraint: <code> variables[index] == target</code>
     * </summary>
     */
    public Constraint AddElement(IntVar index, IEnumerable<IntVar> vars, IntVar target)
    {
        Constraint ct = new Constraint(model_);
        ElementConstraintProto element = new ElementConstraintProto();
        element.Index = index.Index;
        foreach (IntVar var in vars)
        {
            element.Vars.Add(var.Index);
        }
        element.Target = target.Index;
        ct.Proto.Element = element;
        return ct;
    }

    /**
     * <summary>
     * Adds the element constraint: <code> values[index] == target</code>
     * </summary>
     */
    public Constraint AddElement(IntVar index, IEnumerable<long> values, IntVar target)
    {
        Constraint ct = new Constraint(model_);
        ElementConstraintProto element = new ElementConstraintProto();
        element.Index = index.Index;
        foreach (long value in values)
        {
            element.Vars.Add(ConvertConstant(value));
        }
        element.Target = target.Index;
        ct.Proto.Element = element;
        return ct;
    }

    /**
     * <summary>
     * Adds the element constraint: <code> values[index] == target</code>
     * </summary>
     */
    public Constraint AddElement(IntVar index, IEnumerable<int> values, IntVar target)
    {
        Constraint ct = new Constraint(model_);
        ElementConstraintProto element = new ElementConstraintProto();
        element.Index = index.Index;
        foreach (int value in values)
        {
            element.Vars.Add(ConvertConstant(value));
        }
        element.Target = target.Index;
        ct.Proto.Element = element;
        return ct;
    }

    /**
     * <summary>
     * Adds and returns an empty circuit constraint.
     * </summary>
     *
     * <remarks> A circuit is a unique Hamiltonian path in a subgraph of the total graph. In case a node 'i'
     * is not in the path, then there must be a loop arc <code> i -> i</code> associated with a true
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
     * Adds <code> AllowedAssignments(variables)</code>
     * </summary>
     *
     * <remarks>An AllowedAssignments constraint is a constraint on an array of variables that forces, when
     * all variables are fixed to a single value, that the corresponding list of values is equal to
     * one of the tuples of the tupleList.
     * </remarks>
     *
     * <param name="vars"> a list of variables</param>
     * <returns> an instance of the TableConstraint class without any tuples. Tuples can be added
     * directly to the table constraint
     * </returns>
     */
    public TableConstraint AddAllowedAssignments(IEnumerable<IntVar> vars)
    {
        TableConstraint ct = new TableConstraint(model_);
        TableConstraintProto table = new TableConstraintProto();
        foreach (IntVar var in vars)
        {
            table.Vars.Add(var.Index);
        }
        ct.Proto.Table = table;
        return ct;
    }

    /**
     * <summary>
     * Adds <code> ForbiddenAssignments(variables)</code>
     * </summary>
     *
     * <remarks>A ForbiddenAssignments constraint is a constraint on an array of variables where the list of
     * impossible combinations is provided in the tuples list.
     * </remarks>
     *
     * <param name="vars"> a list of variables</param>
     * <returns> an instance of the TableConstraint class without any tuples. Tuples can be added
     * directly to the table constraint
     * </returns>
     */
    public TableConstraint AddForbiddenAssignments(IEnumerable<IntVar> vars)
    {
        TableConstraint ct = AddAllowedAssignments(vars);
        ct.Proto.Table.Negated = true;
        return ct;
    }

    /**
     * <summary>
     * Adds an automaton constraint.
     * </summary>
     *
     * <remarks>
     * <para>An automaton constraint takes a list of variables (of size n), an initial state, a set of
     * final states, and a set of transitions that will be added incrementally directly on the
     * returned AutomatonConstraint instance. A transition is a triplet ('tail', 'transition',
     * 'head'), where 'tail' and 'head' are states, and 'transition' is the label of an arc from
     * 'head' to 'tail', corresponding to the value of one variable in the list of variables. </para>
     *
     * <para>This automaton will be unrolled into a flow with n + 1 phases. Each phase contains the
     * possible states of the automaton. The first state contains the initial state. The last phase
     * contains the final states. </para>
     *
     * <para>Between two consecutive phases i and i + 1, the automaton creates a set of arcs. For each
     * transition (tail, label, head), it will add an arc from the state 'tail' of phase i and the
     * state 'head' of phase i + 1. This arc labeled by the value 'label' of the variables
     * 'variables[i]'. That is, this arc can only be selected if 'variables[i]' is assigned the value
     * 'label'. </para>
     *
     * <para>A feasible solution of this constraint is an assignment of variables such that, starting
     * from the initial state in phase 0, there is a path labeled by the values of the variables that
     * ends in one of the final states in the final phase. </para>
     * </remarks>
     *
     * <param name="vars"> a non empty list of variables whose values correspond to the labels
     *     of the arcs traversed by the automaton</param>
     * <param name="starting_state"> the initial state of the automaton</param>
     * <param name="final_states"> a non empty list of admissible final states </param>
     * <returns> an instance of the Constraint class </returns>
     */
    public AutomatonConstraint AddAutomaton(IEnumerable<IntVar> vars, long starting_state,
                                            IEnumerable<long> final_states)
    {
        AutomatonConstraint ct = new AutomatonConstraint(model_);
        AutomatonConstraintProto aut = new AutomatonConstraintProto();
        foreach (IntVar var in vars)
        {
            aut.Vars.Add(var.Index);
        }
        aut.StartingState = starting_state;
        foreach (long f in final_states)
        {
            aut.FinalStates.Add(f);
        }
        ct.Proto.Automaton = aut;
        return ct;
    }

    /**
     * <summary>
     * Adds <code> Inverse(variables, inverseVariables)</code>
     * </summary>
     *
     * <remarks>An inverse constraint enforces that if 'variables[i]' is assigned a value 'j', then
     * inverseVariables[j] is assigned a value 'i'. And vice versa.
     * </remarks>
     *
     * <param name="direct"> an array of integer variables</param>
     * <param name="reverse"> an array of integer variables</param>
     * <returns> an instance of the Constraint class </returns>
     */
    public Constraint AddInverse(IEnumerable<IntVar> direct, IEnumerable<IntVar> reverse)
    {
        Constraint ct = new Constraint(model_);
        InverseConstraintProto inverse = new InverseConstraintProto();
        foreach (IntVar var in direct)
        {
            inverse.FDirect.Add(var.Index);
        }
        foreach (IntVar var in reverse)
        {
            inverse.FInverse.Add(var.Index);
        }
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
     * <code>sum(levelChanges[i] * actives[i] if times[i] \lt;= t) in [min_level, max_level]</code>
     * </para>
     *
     * <para>Note that min level must be \lt;= 0, and the max level must be \gt;= 0. Please use fixed
     * level_changes to simulate an initial state. </para>
     * </remarks>
     *
     * <param name="minLevel"> at any time, the level of the reservoir must be greater of equal than the min
     *     level. minLevel must me \lt;= 0 </param>
     * <param name="maxLevel"> at any time, the level of the reservoir must be less or equal than the max
     *     level. maxLevel must be \gt;= 0 </param>
     * <returns> an instance of the ReservoirConstraint class </returns>
     */
    public ReservoirConstraint AddReservoirConstraint(long minLevel, long maxLevel)
    {
        ReservoirConstraint ct = new ReservoirConstraint(this, model_);
        ReservoirConstraintProto res = new ReservoirConstraintProto();

        res.MinLevel = minLevel;
        res.MaxLevel = maxLevel;
        ct.Proto.Reservoir = res;

        return ct;
    }

    public void AddMapDomain(IntVar var, IEnumerable<IntVar> bool_vars, long offset = 0)
    {
        int i = 0;
        foreach (IntVar bool_var in bool_vars)
        {
            int b_index = bool_var.Index;
            int var_index = var.Index;

            ConstraintProto ct1 = new ConstraintProto();
            LinearConstraintProto lin1 = new LinearConstraintProto();
            lin1.Vars.Add(var_index);
            lin1.Coeffs.Add(1L);
            lin1.Domain.Add(offset + i);
            lin1.Domain.Add(offset + i);
            ct1.Linear = lin1;
            ct1.EnforcementLiteral.Add(b_index);
            model_.Constraints.Add(ct1);

            ConstraintProto ct2 = new ConstraintProto();
            LinearConstraintProto lin2 = new LinearConstraintProto();
            lin2.Vars.Add(var_index);
            lin2.Coeffs.Add(1L);
            lin2.Domain.Add(Int64.MinValue);
            lin2.Domain.Add(offset + i - 1);
            lin2.Domain.Add(offset + i + 1);
            lin2.Domain.Add(Int64.MaxValue);
            ct2.Linear = lin2;
            ct2.EnforcementLiteral.Add(-b_index - 1);
            model_.Constraints.Add(ct2);

            i++;
        }
    }

    /**
     * <summary>
     * Adds <code>a => b</code>
     * </summary>
     */
    public Constraint AddImplication(ILiteral a, ILiteral b)
    {
        Constraint ct = new Constraint(model_);
        BoolArgumentProto or = new BoolArgumentProto();
        or.Literals.Add(a.Not().GetIndex());
        or.Literals.Add(b.GetIndex());
        ct.Proto.BoolOr = or;
        return ct;
    }

    /**
     * <summary>
     * Adds <code>Or(literals) == true</code>
     * </summary>
     */
    public Constraint AddBoolOr(IEnumerable<ILiteral> literals)
    {
        Constraint ct = new Constraint(model_);
        BoolArgumentProto bool_argument = new BoolArgumentProto();
        foreach (ILiteral lit in literals)
        {
            bool_argument.Literals.Add(lit.GetIndex());
        }
        ct.Proto.BoolOr = bool_argument;
        return ct;
    }

    /**
     * <summary>
     * Same as AddBoolOr: <code>Sum(literals) >= 1</code>
     * </summary>
     */
    public Constraint AddAtLeastOne(IEnumerable<ILiteral> literals)
    {
        return AddBoolOr(literals);
    }

    /**
     * <summary>
     * Adds <code> AtMostOne(literals): Sum(literals) \lt;= 1</code>
     * </summary>
     */
    public Constraint AddAtMostOne(IEnumerable<ILiteral> literals)
    {
        Constraint ct = new Constraint(model_);
        BoolArgumentProto bool_argument = new BoolArgumentProto();
        foreach (ILiteral lit in literals)
        {
            bool_argument.Literals.Add(lit.GetIndex());
        }
        ct.Proto.AtMostOne = bool_argument;
        return ct;
    }

    /**
     * <summary>
     * Adds <code> ExactlyOne(literals): Sum(literals) == 1</code>
     * </summary>
     */
    public Constraint AddExactlyOne(IEnumerable<ILiteral> literals)
    {
        Constraint ct = new Constraint(model_);
        BoolArgumentProto bool_argument = new BoolArgumentProto();
        foreach (ILiteral lit in literals)
        {
            bool_argument.Literals.Add(lit.GetIndex());
        }
        ct.Proto.ExactlyOne = bool_argument;
        return ct;
    }

    /**
     * <summary>
     * Adds <code>And(literals) == true</code>
     * </summary>
     */
    public Constraint AddBoolAnd(IEnumerable<ILiteral> literals)
    {
        Constraint ct = new Constraint(model_);
        BoolArgumentProto bool_argument = new BoolArgumentProto();
        foreach (ILiteral lit in literals)
        {
            bool_argument.Literals.Add(lit.GetIndex());
        }
        ct.Proto.BoolAnd = bool_argument;
        return ct;
    }

    /**
     * <summary>
     * Adds <code>XOr(literals) == true</code>
     * </summary>
     */
    public Constraint AddBoolXor(IEnumerable<ILiteral> literals)
    {
        Constraint ct = new Constraint(model_);
        BoolArgumentProto bool_argument = new BoolArgumentProto();
        foreach (ILiteral lit in literals)
        {
            bool_argument.Literals.Add(lit.GetIndex());
        }
        ct.Proto.BoolXor = bool_argument;
        return ct;
    }

    /**
     * <summary>
     * Adds <code>target == Min(exprs)</code>
     * </summary>
     */
    public Constraint AddMinEquality(LinearExpr target, IEnumerable<LinearExpr> exprs)
    {
        Constraint ct = new Constraint(model_);
        LinearArgumentProto lin = new LinearArgumentProto();
        foreach (LinearExpr expr in exprs)
        {
            lin.Exprs.Add(GetLinearExpressionProto(expr, /*negate=*/true));
        }
        lin.Target = GetLinearExpressionProto(target, /*negate=*/true);
        ct.Proto.LinMax = lin;
        return ct;
    }

    /**
     * <summary>
     * Adds <code>target == Max(exprs)</code>
     * </summary>
     */
    public Constraint AddMaxEquality(LinearExpr target, IEnumerable<LinearExpr> exprs)
    {
        Constraint ct = new Constraint(model_);
        LinearArgumentProto lin = new LinearArgumentProto();
        foreach (LinearExpr expr in exprs)
        {
            lin.Exprs.Add(GetLinearExpressionProto(expr));
        }
        lin.Target = GetLinearExpressionProto(target);
        ct.Proto.LinMax = lin;
        return ct;
    }

    /**
     * <summary>
     * Adds <code>target == num / denom</code> (integer division)
     * </summary>
     */
    public Constraint AddDivisionEquality<T, N, D>(T target, N num, D denom)
    {
        Constraint ct = new Constraint(model_);
        LinearArgumentProto div = new LinearArgumentProto();
        div.Exprs.Add(GetLinearExpressionProto(GetLinearExpr(num)));
        div.Exprs.Add(GetLinearExpressionProto(GetLinearExpr(denom)));
        div.Target = GetLinearExpressionProto(GetLinearExpr(target));
        ct.Proto.IntDiv = div;
        return ct;
    }

    /**
     * <summary>
     * Adds <code>target == abs(expr)</code>
     * </summary>
     */
    public Constraint AddAbsEquality(LinearExpr target, LinearExpr expr)
    {
        Constraint ct = new Constraint(model_);
        LinearArgumentProto abs = new LinearArgumentProto();
        abs.Exprs.Add(GetLinearExpressionProto(expr));
        abs.Exprs.Add(GetLinearExpressionProto(expr, /*negate=*/true));
        abs.Target = GetLinearExpressionProto(target);
        ct.Proto.LinMax = abs;
        return ct;
    }

    /**
     * <summary>
     * Adds <code>target == v % m</code>
     * </summary>
     */
    public Constraint AddModuloEquality<T, V, M>(T target, V v, M m)
    {
        Constraint ct = new Constraint(model_);
        LinearArgumentProto mod = new LinearArgumentProto();
        mod.Exprs.Add(GetLinearExpressionProto(GetLinearExpr(v)));
        mod.Exprs.Add(GetLinearExpressionProto(GetLinearExpr(m)));
        mod.Target = GetLinearExpressionProto(GetLinearExpr(target));
        ct.Proto.IntMod = mod;
        return ct;
    }

    /**
     * <summary>
     * Adds <code>target == PROD(exprs)</code>
     * </summary>
     */
    public Constraint AddMultiplicationEquality(LinearExpr target, IEnumerable<LinearExpr> exprs)
    {
        Constraint ct = new Constraint(model_);
        LinearArgumentProto prod = new LinearArgumentProto();
        prod.Target = GetLinearExpressionProto(target);
        foreach (LinearExpr expr in exprs)
        {
            prod.Exprs.Add(GetLinearExpressionProto(expr));
        }
        ct.Proto.IntProd = prod;
        return ct;
    }

    /**
     * <summary>
     * Adds <code>target == left * right</code>
     * </summary>
     */
    public Constraint AddMultiplicationEquality(LinearExpr target, LinearExpr left, LinearExpr right)
    {
        Constraint ct = new Constraint(model_);
        LinearArgumentProto prod = new LinearArgumentProto();
        prod.Target = GetLinearExpressionProto(target);
        prod.Exprs.Add(GetLinearExpressionProto(left));
        prod.Exprs.Add(GetLinearExpressionProto(right));
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
     * NoOverlap. Internally, it ensures that <code>start + size == end</code>.
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
        Add(startExpr + sizeExpr == endExpr);

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
     * NoOverlap. Internally, it ensures that <code>is_present => start + size == end</code>.
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
        Add(startExpr + sizeExpr == endExpr).OnlyEnforceIf(is_present);

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
     * Adds <code> NoOverlap(intervalVars)</code>.
     *
     * <remarks>A NoOverlap constraint ensures that all present intervals do not overlap in time.
     * </remarks>
     *
     * <param name="intervals"> the list of interval variables to constrain</param>
     * <returns> an instance of the Constraint class</returns>
     */
    public Constraint AddNoOverlap(IEnumerable<IntervalVar> intervals)
    {
        Constraint ct = new Constraint(model_);
        NoOverlapConstraintProto no_overlap = new NoOverlapConstraintProto();
        foreach (IntervalVar var in intervals)
        {
            no_overlap.Intervals.Add(var.GetIndex());
        }
        ct.Proto.NoOverlap = no_overlap;
        return ct;
    }

    /**
     * <summary>
     * Adds <code>NoOverlap2D(xIntervals, yIntervals)</code>
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
     * Adds <code>Cumulative(capacity)</code>
     * </summary>
     *
     * <remarks>
     * This constraint enforces that:
     * <code>
     * forall t:
     *      sum(demands[i] if (start(intervals[t]) \lt;= t \lt; end(intervals[t])) and (t is
     *          present)\lt;= capacity
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
        CumulativeConstraint ct = new CumulativeConstraint(this, model_);
        CumulativeConstraintProto cumul = new CumulativeConstraintProto();
        LinearExpr capacityExpr = GetLinearExpr(capacity);
        cumul.Capacity = GetLinearExpressionProto(capacityExpr);
        ct.Proto.Cumulative = cumul;
        return ct;
    }

    // Objective.

    /** <summary>Adds a minimization objective of a linear expression</summary>*/
    public void Minimize(LinearExpr obj)
    {
        SetObjective(obj, true);
    }

    /** <summary>Adds a maximization objective of a linear expression</summary>*/
    public void Maximize(LinearExpr obj)
    {
        SetObjective(obj, false);
    }

    /** <summary>Clears the objective</summary>*/
    void ClearObjective()
    {
        model_.Objective = null;
    }

    /** <summary>Returns whether the model contains an objective.</summary>*/
    bool HasObjective()
    {
        return model_.Objective is not null;
    }

    // Search Decision.

    /** <summary>Adds <code> DecisionStrategy(variables, var_str, dom_str)</code></summary>. */
    public void AddDecisionStrategy(IEnumerable<IntVar> vars,
                                    DecisionStrategyProto.Types.VariableSelectionStrategy var_str,
                                    DecisionStrategyProto.Types.DomainReductionStrategy dom_str)
    {
        DecisionStrategyProto ds = new DecisionStrategyProto();
        foreach (IntVar var in vars)
        {
            ds.Variables.Add(var.Index);
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

    /** <summary>Clears all assumptions </summary>*/
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
            objective.Vars.Add(intVar.Index);
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
            Dictionary<IntVar, long> dict = new Dictionary<IntVar, long>();
            long constant = LinearExpr.GetVarValueMap(obj, 1L, dict);
            if (minimize)
            {
                objective.ScalingFactor = 1L;
                objective.Offset = constant;
            }
            else
            {
                objective.ScalingFactor = -1L;
                objective.Offset = -constant;
            }
            foreach (KeyValuePair<IntVar, long> it in dict)
            {
                objective.Vars.Add(it.Key.Index);
                objective.Coeffs.Add(minimize ? it.Value : -it.Value);
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
     * Write the model as a protocol buffer to 'file'.
     * </summary>
     *
     * <param name="file"> file to write the model to. If the filename ends with 'txt', the
     *    model will be written as a text file, otherwise, the binary format will be used. </param>
     *
     * <returns> true if the model was correctly written </returns>
     */
    public Boolean ExportToFile(String file)
    {
        return CpSatHelper.WriteModelToFile(model_, file);
    }

    /**
     * <summary
     * >Returns a non empty string explaining the issue if the model is invalid.
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
        var.Domain.Add(value);
        var.Domain.Add(value);
        constant_map_.Add(value, index);
        model_.Variables.Add(var);
        return index;
    }

    public LinearExpr GetLinearExpr<X>(X x)
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

    public LinearExpressionProto GetLinearExpressionProto(LinearExpr expr, bool negate = false)
    {
        Dictionary<IntVar, long> dict = new Dictionary<IntVar, long>();
        long constant = LinearExpr.GetVarValueMap(expr, 1L, dict);
        long mult = negate ? -1 : 1;
        LinearExpressionProto linear = new LinearExpressionProto();
        foreach (KeyValuePair<IntVar, long> term in dict)
        {
            linear.Vars.Add(term.Key.Index);
            linear.Coeffs.Add(term.Value * mult);
        }
        linear.Offset = constant * mult;
        return linear;
    }

    private CpModelProto model_;
    private Dictionary<long, int> constant_map_;
    private BoolVar true_literal_;
}

} // namespace Google.OrTools.Sat
