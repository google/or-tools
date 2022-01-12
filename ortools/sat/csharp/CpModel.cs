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

    public IntVar NewIntVar(long lb, long ub, string name)
    {
        return new IntVar(model_, lb, ub, name);
    }

    public IntVar NewIntVarFromDomain(Domain domain, string name)
    {
        return new IntVar(model_, domain, name);
    }

    public IntVar NewConstant(long value)
    {
        return new IntVar(model_, ConvertConstant(value));
    }

    public IntVar NewConstant(long value, string name)
    {
        return new IntVar(model_, value, value, name);
    }

    public BoolVar NewBoolVar(string name)
    {
        return new BoolVar(model_, name);
    }

    public ILiteral TrueLiteral()
    {
        if (true_literal_ == null)
        {
            true_literal_ = new BoolVar(model_, ConvertConstant(1));
        }
        return true_literal_;
    }

    public ILiteral FalseLiteral()
    {
        return TrueLiteral().Not();
    }

    private long FillLinearConstraint(LinearExpr expr, ref LinearConstraintProto linear)
    {
        Dictionary<IntVar, long> dict = new Dictionary<IntVar, long>();
        long constant = LinearExpr.GetVarValueMap(expr, 1L, dict);
        foreach (KeyValuePair<IntVar, long> term in dict)
        {
            linear.Vars.Add(term.Key.Index);
            linear.Coeffs.Add(term.Value);
        }
        return constant;
    }
    public Constraint AddLinearConstraint(LinearExpr expr, long lb, long ub)
    {
        LinearConstraintProto linear = new LinearConstraintProto();
        long constant = FillLinearConstraint(expr, ref linear);
        linear.Domain.Add(lb is Int64.MinValue ? lb : lb - constant);
        linear.Domain.Add(ub is Int64.MaxValue ? ub : ub - constant);

        Constraint ct = new Constraint(model_);
        ct.Proto.Linear = linear;
        return ct;
    }

    public Constraint AddLinearExpressionInDomain(LinearExpr expr, Domain domain)
    {
        LinearConstraintProto linear = new LinearConstraintProto();
        long constant = FillLinearConstraint(expr, ref linear);
        foreach (long value in domain.FlattenedIntervals())
        {
            if (value == Int64.MinValue || value == Int64.MaxValue)
            {
                linear.Domain.Add(value);
            }
            else
            {
                linear.Domain.Add(value - constant);
            }
        }

        Constraint ct = new Constraint(model_);
        ct.Proto.Linear = linear;
        return ct;
    }

    private Constraint AddLinearExpressionNotEqualCst(LinearExpr expr, long value)
    {
        LinearConstraintProto linear = new LinearConstraintProto();
        long constant = FillLinearConstraint(expr, ref linear);
        linear.Domain.Add(Int64.MinValue);
        linear.Domain.Add(value - constant - 1);
        linear.Domain.Add(value - constant + 1);
        linear.Domain.Add(Int64.MaxValue);

        Constraint ct = new Constraint(model_);
        ct.Proto.Linear = linear;
        return ct;
    }

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

    public CircuitConstraint AddCircuit()
    {
        CircuitConstraint ct = new CircuitConstraint(model_);
        ct.Proto.Circuit = new CircuitConstraintProto();
        return ct;
    }

    public MultipleCircuitConstraint AddMultipleCircuit()
    {
        MultipleCircuitConstraint ct = new MultipleCircuitConstraint(model_);
        ct.Proto.Routes = new RoutesConstraintProto();
        return ct;
    }

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

    public TableConstraint AddForbiddenAssignments(IEnumerable<IntVar> vars)
    {
        TableConstraint ct = AddAllowedAssignments(vars);
        ct.Proto.Table.Negated = true;
        return ct;
    }

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

    public Constraint AddImplication(ILiteral a, ILiteral b)
    {
        Constraint ct = new Constraint(model_);
        BoolArgumentProto or = new BoolArgumentProto();
        or.Literals.Add(a.Not().GetIndex());
        or.Literals.Add(b.GetIndex());
        ct.Proto.BoolOr = or;
        return ct;
    }

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

    public Constraint AddAtLeastOne(IEnumerable<ILiteral> literals)
    {
        return AddBoolOr(literals);
    }

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

    public IntervalVar NewIntervalVar<S, D, E>(S start, D duration, E end, string name)
    {
        LinearExpr startExpr = GetLinearExpr(start);
        LinearExpr durationExpr = GetLinearExpr(duration);
        LinearExpr endExpr = GetLinearExpr(end);
        Add(startExpr + durationExpr == endExpr);

        LinearExpressionProto startProto = GetLinearExpressionProto(startExpr);
        LinearExpressionProto durationProto = GetLinearExpressionProto(durationExpr);
        LinearExpressionProto endProto = GetLinearExpressionProto(endExpr);
        return new IntervalVar(model_, startProto, durationProto, endProto, name);
    }

    public IntervalVar NewFixedSizeIntervalVar<S>(S start, long duration, string name)
    {
        LinearExpr startExpr = GetLinearExpr(start);
        LinearExpr durationExpr = GetLinearExpr(duration);
        LinearExpr endExpr = LinearExpr.Sum(new LinearExpr[] { startExpr, durationExpr });

        LinearExpressionProto startProto = GetLinearExpressionProto(startExpr);
        LinearExpressionProto durationProto = GetLinearExpressionProto(durationExpr);
        LinearExpressionProto endProto = GetLinearExpressionProto(endExpr);
        return new IntervalVar(model_, startProto, durationProto, endProto, name);
    }

    public IntervalVar NewOptionalIntervalVar<S, D, E>(S start, D duration, E end, ILiteral is_present, string name)
    {
        LinearExpr startExpr = GetLinearExpr(start);
        LinearExpr durationExpr = GetLinearExpr(duration);
        LinearExpr endExpr = GetLinearExpr(end);
        Add(startExpr + durationExpr == endExpr).OnlyEnforceIf(is_present);

        LinearExpressionProto startProto = GetLinearExpressionProto(startExpr);
        LinearExpressionProto durationProto = GetLinearExpressionProto(durationExpr);
        LinearExpressionProto endProto = GetLinearExpressionProto(endExpr);
        return new IntervalVar(model_, startProto, durationProto, endProto, is_present.GetIndex(), name);
    }

    public IntervalVar NewOptionalFixedSizeIntervalVar<S>(S start, long duration, ILiteral is_present, string name)
    {
        LinearExpr startExpr = GetLinearExpr(start);
        LinearExpr durationExpr = GetLinearExpr(duration);
        LinearExpr endExpr = LinearExpr.Sum(new LinearExpr[] { startExpr, durationExpr });

        LinearExpressionProto startProto = GetLinearExpressionProto(startExpr);
        LinearExpressionProto durationProto = GetLinearExpressionProto(durationExpr);
        LinearExpressionProto endProto = GetLinearExpressionProto(endExpr);
        return new IntervalVar(model_, startProto, durationProto, endProto, is_present.GetIndex(), name);
    }

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

    public NoOverlap2dConstraint AddNoOverlap2D()
    {
        NoOverlap2dConstraint ct = new NoOverlap2dConstraint(model_);
        ct.Proto.NoOverlap2D = new NoOverlap2DConstraintProto();
        return ct;
    }

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
    public void Minimize(LinearExpr obj)
    {
        SetObjective(obj, true);
    }

    public void Maximize(LinearExpr obj)
    {
        SetObjective(obj, false);
    }

    void ClearObjective()
    {
        model_.Objective = null;
    }

    bool HasObjective()
    {
        return model_.Objective != null;
    }

    // Search Decision.

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

    public void AddHint(IntVar var, long value)
    {
        if (model_.SolutionHint == null)
        {
            model_.SolutionHint = new PartialVariableAssignment();
        }
        model_.SolutionHint.Vars.Add(var.GetIndex());
        model_.SolutionHint.Values.Add(value);
    }

    public void ClearHints()
    {
        model_.SolutionHint = null;
    }

    public void AddAssumption(ILiteral lit)
    {
        model_.Assumptions.Add(lit.GetIndex());
    }

    public void AddAssumptions(IEnumerable<ILiteral> literals)
    {
        foreach (ILiteral lit in literals)
        {
            AddAssumption(lit);
        }
    }

    public void ClearAssumptions()
    {
        model_.Assumptions.Clear();
    }

    // Internal methods.

    void SetObjective(LinearExpr obj, bool minimize)
    {
        CpObjectiveProto objective = new CpObjectiveProto();
        if (obj == null)
        {
            objective.Offset = 0L;
            objective.ScalingFactor = minimize ? 1L : -1;
        }
        else if (obj is IntVar)
        {
            objective.Offset = 0L;
            objective.Vars.Add(((IntVar)obj).Index);
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
    public String ModelStats()
    {
        return CpSatHelper.ModelStats(model_);
    }

    public Boolean ExportToFile(String filename)
    {
        return CpSatHelper.WriteModelToFile(model_, filename);
    }

    public String Validate()
    {
        return CpSatHelper.ValidateModel(model_);
    }

    private int ConvertConstant(long value)
    {
        if (constant_map_.ContainsKey(value))
        {
            return constant_map_[value];
        }
        else
        {
            int index = model_.Variables.Count;
            IntegerVariableProto var = new IntegerVariableProto();
            var.Domain.Add(value);
            var.Domain.Add(value);
            constant_map_.Add(value, index);
            model_.Variables.Add(var);
            return index;
        }
    }

    private int GetOrCreateIndex<X>(X x)
    {
        if (typeof(X) == typeof(IntVar))
        {
            IntVar vx = (IntVar)(Object)x;
            return vx.Index;
        }
        if (typeof(X) == typeof(long) || typeof(X) == typeof(int))
        {
            return ConvertConstant(Convert.ToInt64(x));
        }
        throw new ArgumentException("Cannot extract index from argument");
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
