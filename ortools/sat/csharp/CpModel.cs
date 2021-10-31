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
            return new IntVar(model_, new Domain(lb, ub), name);
        }

        public IntVar NewIntVarFromDomain(Domain domain, string name)
        {
            return new IntVar(model_, domain, name);
        }
        // Constants (named or not).

        // TODO: Cache constant.
        public IntVar NewConstant(long value)
        {
            return new IntVar(model_, new Domain(value), String.Format("{0}", value));
        }

        public IntVar NewConstant(long value, string name)
        {
            return new IntVar(model_, new Domain(value), name);
        }

        public IntVar NewBoolVar(string name)
        {
            return new IntVar(model_, new Domain(0, 1), name);
        }

        public Constraint AddLinearConstraint(LinearExpr linear_expr, long lb, long ub)
        {
            return AddLinearExpressionInDomain(linear_expr, new Domain(lb, ub));
        }

        public Constraint AddLinearExpressionInDomain(LinearExpr linear_expr, Domain domain)
        {
            Dictionary<IntVar, long> dict = new Dictionary<IntVar, long>();
            long constant = LinearExpr.GetVarValueMap(linear_expr, 1L, dict);
            Constraint ct = new Constraint(model_);
            LinearConstraintProto linear = new LinearConstraintProto();
            foreach (KeyValuePair<IntVar, long> term in dict)
            {
                linear.Vars.Add(term.Key.Index);
                linear.Coeffs.Add(term.Value);
            }
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
            ct.Proto.Linear = linear;
            return ct;
        }

        public Constraint Add(BoundedLinearExpression lin)
        {
            switch (lin.CtType)
            {
            case BoundedLinearExpression.Type.BoundExpression: {
                return AddLinearExpressionInDomain(lin.Left, new Domain(lin.Lb, lin.Ub));
            }
            case BoundedLinearExpression.Type.VarEqVar: {
                return AddLinearExpressionInDomain(lin.Left - lin.Right, new Domain(0));
            }
            case BoundedLinearExpression.Type.VarDiffVar: {
                return AddLinearExpressionInDomain(
                    lin.Left - lin.Right,
                    Domain.FromFlatIntervals(new long[] { Int64.MinValue, -1, 1, Int64.MaxValue }));
            }
            case BoundedLinearExpression.Type.VarEqCst: {
                return AddLinearExpressionInDomain(lin.Left, new Domain(lin.Lb, lin.Lb));
            }
            case BoundedLinearExpression.Type.VarDiffCst: {
                return AddLinearExpressionInDomain(
                    lin.Left,
                    Domain.FromFlatIntervals(new long[] { Int64.MinValue, lin.Lb - 1, lin.Lb + 1, Int64.MaxValue }));
            }
            }
            return null;
        }

        public Constraint AddAllDifferent(IEnumerable<IntVar> vars)
        {
            Constraint ct = new Constraint(model_);
            AllDifferentConstraintProto alldiff = new AllDifferentConstraintProto();
            foreach (IntVar var in vars)
            {
                alldiff.Vars.Add(var.Index);
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

        public Constraint AddCircuit(IEnumerable<Tuple<int, int, ILiteral>> arcs)
        {
            Constraint ct = new Constraint(model_);
            CircuitConstraintProto circuit = new CircuitConstraintProto();
            foreach (var arc in arcs)
            {
                circuit.Tails.Add(arc.Item1);
                circuit.Heads.Add(arc.Item2);
                circuit.Literals.Add(arc.Item3.GetIndex());
            }
            ct.Proto.Circuit = circuit;
            return ct;
        }

        public Constraint AddAllowedAssignments(IEnumerable<IntVar> vars, long[,] tuples)
        {
            Constraint ct = new Constraint(model_);
            TableConstraintProto table = new TableConstraintProto();
            foreach (IntVar var in vars)
            {
                table.Vars.Add(var.Index);
            }
            for (int i = 0; i < tuples.GetLength(0); ++i)
            {
                for (int j = 0; j < tuples.GetLength(1); ++j)
                {
                    table.Values.Add(tuples[i, j]);
                }
            }
            ct.Proto.Table = table;
            return ct;
        }

        public Constraint AddForbiddenAssignments(IEnumerable<IntVar> vars, long[,] tuples)
        {
            Constraint ct = AddAllowedAssignments(vars, tuples);
            ct.Proto.Table.Negated = true;
            return ct;
        }

        public Constraint AddAutomaton(IEnumerable<IntVar> vars, long starting_state, long[,] transitions,
                                       IEnumerable<long> final_states)
        {
            Constraint ct = new Constraint(model_);
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
            for (int i = 0; i < transitions.GetLength(0); ++i)
            {
                aut.TransitionTail.Add(transitions[i, 0]);
                aut.TransitionLabel.Add(transitions[i, 1]);
                aut.TransitionHead.Add(transitions[i, 2]);
            }

            ct.Proto.Automaton = aut;
            return ct;
        }

        public Constraint AddAutomaton(IEnumerable<IntVar> vars, long starting_state,
                                       IEnumerable<Tuple<long, long, long>> transitions, IEnumerable<long> final_states)
        {
            Constraint ct = new Constraint(model_);
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
            foreach (Tuple<long, long, long> transition in transitions)
            {
                aut.TransitionHead.Add(transition.Item1);
                aut.TransitionLabel.Add(transition.Item2);
                aut.TransitionTail.Add(transition.Item3);
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

        public Constraint AddReservoirConstraint<I>(IEnumerable<IntVar> times, IEnumerable<I> demands, long min_level,
                                                    long max_level)
        {
            Constraint ct = new Constraint(model_);
            ReservoirConstraintProto res = new ReservoirConstraintProto();
            foreach (IntVar var in times)
            {
                res.Times.Add(var.Index);
            }
            foreach (I d in demands)
            {
                res.Demands.Add(Convert.ToInt64(d));
            }

            res.MinLevel = min_level;
            res.MaxLevel = max_level;
            ct.Proto.Reservoir = res;

            return ct;
        }

        public Constraint AddReservoirConstraintWithActive<I>(IEnumerable<IntVar> times, IEnumerable<I> demands,
                                                              IEnumerable<IntVar> actives, long min_level,
                                                              long max_level)
        {
            Constraint ct = new Constraint(model_);
            ReservoirConstraintProto res = new ReservoirConstraintProto();
            foreach (IntVar var in times)
            {
                res.Times.Add(var.Index);
            }
            foreach (I d in demands)
            {
                res.Demands.Add(Convert.ToInt64(d));
            }
            foreach (IntVar var in actives)
            {
                res.Actives.Add(var.Index);
            }
            res.MinLevel = min_level;
            res.MaxLevel = max_level;
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

        public Constraint AddMinEquality(LinearExpr target, IEnumerable<IntVar> vars)
        {
            Constraint ct = new Constraint(model_);
            LinearArgumentProto args = new LinearArgumentProto();
            foreach (IntVar var in vars)
            {
                args.Exprs.Add(GetLinearExpressionProto(var, /*negate=*/true));
            }
            args.Target = GetLinearExpressionProto(target, /*negate=*/true);
            ct.Proto.LinMax = args;
            return ct;
        }

        public Constraint AddLinMinEquality(LinearExpr target, IEnumerable<LinearExpr> exprs)
        {
            Constraint ct = new Constraint(model_);
            LinearArgumentProto args = new LinearArgumentProto();
            foreach (LinearExpr expr in exprs)
            {
                args.Exprs.Add(GetLinearExpressionProto(expr, /*negate=*/true));
            }
            args.Target = GetLinearExpressionProto(target, /*negate=*/true);
            ct.Proto.LinMax = args;
            return ct;
        }

        public Constraint AddMaxEquality(IntVar target, IEnumerable<IntVar> vars)
        {
            Constraint ct = new Constraint(model_);
            LinearArgumentProto args = new LinearArgumentProto();
            foreach (IntVar var in vars)
            {
                args.Exprs.Add(GetLinearExpressionProto(var));
            }
            args.Target = GetLinearExpressionProto(target);
            ct.Proto.LinMax = args;
            return ct;
        }

        public Constraint AddLinMaxEquality(LinearExpr target, IEnumerable<LinearExpr> exprs)
        {
            Constraint ct = new Constraint(model_);
            LinearArgumentProto args = new LinearArgumentProto();
            foreach (LinearExpr expr in exprs)
            {
                args.Exprs.Add(GetLinearExpressionProto(expr));
            }
            args.Target = GetLinearExpressionProto(target);
            ct.Proto.LinMax = args;
            return ct;
        }

        public Constraint AddDivisionEquality<T, N, D>(T target, N num, D denom)
        {
            Constraint ct = new Constraint(model_);
            IntegerArgumentProto args = new IntegerArgumentProto();
            args.Vars.Add(GetOrCreateIndex(num));
            args.Vars.Add(GetOrCreateIndex(denom));
            args.Target = GetOrCreateIndex(target);
            ct.Proto.IntDiv = args;
            return ct;
        }

        public Constraint AddAbsEquality(LinearExpr target, LinearExpr expr)
        {
            Constraint ct = new Constraint(model_);
            LinearArgumentProto args = new LinearArgumentProto();
            args.Exprs.Add(GetLinearExpressionProto(expr));
            args.Exprs.Add(GetLinearExpressionProto(expr, /*negate=*/true));
            args.Target = GetLinearExpressionProto(target);
            ct.Proto.LinMax = args;
            return ct;
        }

        public Constraint AddModuloEquality<T, V, M>(T target, V v, M m)
        {
            Constraint ct = new Constraint(model_);
            IntegerArgumentProto args = new IntegerArgumentProto();
            args.Vars.Add(GetOrCreateIndex(v));
            args.Vars.Add(GetOrCreateIndex(m));
            args.Target = GetOrCreateIndex(target);
            ct.Proto.IntMod = args;
            return ct;
        }

        public Constraint AddMultiplicationEquality(IntVar target, IEnumerable<IntVar> vars)
        {
            Constraint ct = new Constraint(model_);
            IntegerArgumentProto args = new IntegerArgumentProto();
            args.Target = target.Index;
            foreach (IntVar var in vars)
            {
                args.Vars.Add(var.Index);
            }
            ct.Proto.IntProd = args;
            return ct;
        }

        public Constraint AddProdEquality(IntVar target, IEnumerable<IntVar> vars)
        {
            return AddMultiplicationEquality(target, vars);
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
            NoOverlapConstraintProto args = new NoOverlapConstraintProto();
            foreach (IntervalVar var in intervals)
            {
                args.Intervals.Add(var.GetIndex());
            }
            ct.Proto.NoOverlap = args;
            return ct;
        }

        public Constraint AddNoOverlap2D(IEnumerable<IntervalVar> x_intervals, IEnumerable<IntervalVar> y_intervals)
        {
            Constraint ct = new Constraint(model_);
            NoOverlap2DConstraintProto args = new NoOverlap2DConstraintProto();
            foreach (IntervalVar var in x_intervals)
            {
                args.XIntervals.Add(var.GetIndex());
            }
            foreach (IntervalVar var in y_intervals)
            {
                args.YIntervals.Add(var.GetIndex());
            }
            ct.Proto.NoOverlap2D = args;
            return ct;
        }

        public Constraint AddCumulative<D, C>(IEnumerable<IntervalVar> intervals, IEnumerable<D> demands, C capacity)
        {
            Constraint ct = new Constraint(model_);
            CumulativeConstraintProto cumul = new CumulativeConstraintProto();
            foreach (IntervalVar var in intervals)
            {
                cumul.Intervals.Add(var.GetIndex());
            }
            foreach (D demand in demands)
            {
                LinearExpr demandExpr = GetLinearExpr(demand);
                cumul.Demands.Add(GetLinearExpressionProto(demandExpr));
            }
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

        public void Minimize()
        {
            SetObjective(null, true);
        }

        public void Maximize()
        {
            SetObjective(null, false);
        }

        public void AddVarToObjective(IntVar var)
        {
            if ((Object)var == null)
                return;
            model_.Objective.Vars.Add(var.Index);
            model_.Objective.Coeffs.Add(model_.Objective.ScalingFactor > 0 ? 1 : -1);
        }

        public void AddTermToObjective(IntVar var, long coeff)
        {
            if (coeff == 0 || (Object)var == null)
                return;
            model_.Objective.Vars.Add(var.Index);
            model_.Objective.Coeffs.Add(model_.Objective.ScalingFactor > 0 ? coeff : -coeff);
        }

        bool HasObjective()
        {
            return model_.Objective == null;
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
                objective.Vars.Add(obj.Index);
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

        private LinearExpr GetLinearExpr<X>(X x)
        {
            if (typeof(X) == typeof(IntVar))
            {
                return (IntVar)(Object)x;
            }
            if (typeof(X) == typeof(long) || typeof(X) == typeof(int) || typeof(X) == typeof(short))
            {
                return new ConstantExpr(Convert.ToInt64(x));
            }
            if (typeof(X) == typeof(LinearExpr))
            {
                return (LinearExpr)(Object)x;
            }
            throw new ArgumentException("Cannot convert argument to LinearExpr");
        }

        private LinearExpressionProto GetLinearExpressionProto(LinearExpr expr, bool negate = false)
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
    }

} // namespace Google.OrTools.Sat
