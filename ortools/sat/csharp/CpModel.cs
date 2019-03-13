// Copyright 2010-2018 Google LLC
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
    get { return model_; }
  }

  int Negated(int index)
  {
    return -index - 1;
  }

  // Integer variables and constraints.

  public IntVar NewIntVar(long lb, long ub, string name)
  {
    long[] bounds = { lb, ub };
    return new IntVar(model_, bounds, name);
  }

  public IntVar NewEnumeratedIntVar(IEnumerable<long> bounds, string name)
  {
    return new IntVar(model_, bounds, name);
  }

  // Constants (named or not).

  // TODO: Cache constant.
  public IntVar NewConstant(long value)
  {
    long[] bounds = { value, value };
    return new IntVar(model_, bounds, String.Format("{0}", value));
  }

  public IntVar NewConstant(long value, string name)
  {
    long[] bounds = { value, value };
    return new IntVar(model_, bounds, name);
  }

  public IntVar NewBoolVar(string name)
  {
    long[] bounds = { 0L, 1L };
    return new IntVar(model_, bounds, name);
  }

  public Constraint AddLinearConstraint(IEnumerable<Tuple<IntVar, long>> terms,
                                        long lb, long ub)
  {
    Constraint ct = new Constraint(model_);
    LinearConstraintProto lin = new LinearConstraintProto();
    foreach (Tuple<IntVar, long> term in terms)
    {
      lin.Vars.Add(term.Item1.Index);
      lin.Coeffs.Add(term.Item2);
    }
    lin.Domain.Add(lb);
    lin.Domain.Add(ub);
    ct.Proto.Linear = lin;
    return ct;
  }

  public Constraint AddLinearConstraint(IEnumerable<IntVar> vars,
                                        IEnumerable<long> coeffs,
                                        long lb, long ub)
  {
    Constraint ct = new Constraint(model_);
    LinearConstraintProto lin = new LinearConstraintProto();
    foreach (IntVar var in vars)
    {
      lin.Vars.Add(var.Index);
    }
    foreach (long coeff in coeffs)
    {
      lin.Coeffs.Add(coeff);
    }
    lin.Domain.Add(lb);
    lin.Domain.Add(ub);
    ct.Proto.Linear = lin;
    return ct;
  }

  public Constraint AddLinearConstraint(IEnumerable<IntVar> vars,
                                        IEnumerable<int> coeffs,
                                        long lb, long ub)
  {
    Constraint ct = new Constraint(model_);
    LinearConstraintProto lin = new LinearConstraintProto();
    foreach (IntVar var in vars)
    {
      lin.Vars.Add(var.Index);
    }
    foreach (int coeff in coeffs)
    {
      lin.Coeffs.Add(coeff);
    }
    lin.Domain.Add(lb);
    lin.Domain.Add(ub);
    ct.Proto.Linear = lin;
    return ct;
  }

  public Constraint AddLinearConstraintWithBounds(
      IEnumerable<Tuple<IntVar, long>> terms, IEnumerable<long> bounds)
  {
    Constraint ct = new Constraint(model_);
    LinearConstraintProto lin = new LinearConstraintProto();
    foreach (Tuple<IntVar, long> term in terms)
    {
      lin.Vars.Add(term.Item1.Index);
      lin.Coeffs.Add(term.Item2);
    }
    foreach (long b in bounds)
    {
      lin.Domain.Add(b);
    }
    ct.Proto.Linear = lin;
    return ct;
  }

  public Constraint AddLinearConstraintWithBounds(IEnumerable<IntVar> vars,
                                                  IEnumerable<long> coeffs,
                                                  IEnumerable<long> bounds)
  {
    Constraint ct = new Constraint(model_);
    LinearConstraintProto lin = new LinearConstraintProto();
    foreach (IntVar var in vars)
    {
      lin.Vars.Add(var.Index);
    }
    foreach (long coeff in coeffs)
    {
      lin.Coeffs.Add(coeff);
    }
    foreach (long b in bounds)
    {
      lin.Domain.Add(b);
    }
    ct.Proto.Linear = lin;
    return ct;
  }

  public Constraint AddLinearConstraintWithBounds(IEnumerable<IntVar> vars,
                                                  IEnumerable<int> coeffs,
                                                  IEnumerable<long> bounds)
  {
    Constraint ct = new Constraint(model_);
    LinearConstraintProto lin = new LinearConstraintProto();
    foreach (IntVar var in vars)
    {
      lin.Vars.Add(var.Index);
    }
    foreach (int coeff in coeffs)
    {
      lin.Coeffs.Add(coeff);
    }
    foreach (long b in bounds)
    {
      lin.Domain.Add(b);
    }
    ct.Proto.Linear = lin;
    return ct;
  }

  public Constraint AddSumConstraint(IEnumerable<IntVar> vars, long lb,
                                     long ub)
  {
    Constraint ct = new Constraint(model_);
    LinearConstraintProto lin = new LinearConstraintProto();
    foreach (IntVar var in vars)
    {
      lin.Vars.Add(var.Index);
      lin.Coeffs.Add(1L);
    }
    lin.Domain.Add(lb);
    lin.Domain.Add(ub);
    ct.Proto.Linear = lin;
    return ct;
  }

  public Constraint Add(BoundIntegerExpression lin)
  {
    switch (lin.CtType)
    {
      case BoundIntegerExpression.Type.BoundExpression:
        {
          Dictionary<IntVar, long> dict = new Dictionary<IntVar, long>();
          long constant = IntegerExpression.GetVarValueMap(lin.Left, 1L, dict);
          Constraint ct = new Constraint(model_);
          LinearConstraintProto linear = new LinearConstraintProto();
          foreach (KeyValuePair<IntVar, long> term in dict)
          {
            linear.Vars.Add(term.Key.Index);
            linear.Coeffs.Add(term.Value);
          }
          linear.Domain.Add(lin.Lb == Int64.MinValue ? Int64.MinValue
                            : lin.Lb - constant);
          linear.Domain.Add(lin.Ub == Int64.MaxValue ? Int64.MaxValue
                            : lin.Ub - constant);
          ct.Proto.Linear = linear;
          return ct;
        }
      case BoundIntegerExpression.Type.VarEqVar:
        {
          Dictionary<IntVar, long> dict = new Dictionary<IntVar, long>();
          long constant = IntegerExpression.GetVarValueMap(lin.Left, 1L, dict);
          constant +=  IntegerExpression.GetVarValueMap(lin.Right, -1L, dict);
          Constraint ct = new Constraint(model_);
          LinearConstraintProto linear = new LinearConstraintProto();
          foreach (KeyValuePair<IntVar, long> term in dict)
          {
            linear.Vars.Add(term.Key.Index);
            linear.Coeffs.Add(term.Value);
          }
          linear.Domain.Add(-constant);
          linear.Domain.Add(-constant);
          ct.Proto.Linear = linear;
          return ct;
        }
      case BoundIntegerExpression.Type.VarDiffVar:
        {
          Dictionary<IntVar, long> dict = new Dictionary<IntVar, long>();
          long constant = IntegerExpression.GetVarValueMap(lin.Left, 1L, dict);
          constant +=  IntegerExpression.GetVarValueMap(lin.Right, -1L, dict);
          Constraint ct = new Constraint(model_);
          LinearConstraintProto linear = new LinearConstraintProto();
          foreach (KeyValuePair<IntVar, long> term in dict)
          {
            linear.Vars.Add(term.Key.Index);
            linear.Coeffs.Add(term.Value);
          }
          linear.Domain.Add(Int64.MinValue);
          linear.Domain.Add(-constant - 1);
          linear.Domain.Add(-constant + 1);
          linear.Domain.Add(Int64.MaxValue);
          ct.Proto.Linear = linear;
          return ct;
        }
      case BoundIntegerExpression.Type.VarEqCst:
        {
          Dictionary<IntVar, long> dict = new Dictionary<IntVar, long>();
          long constant = IntegerExpression.GetVarValueMap(lin.Left, 1L, dict);
          Constraint ct = new Constraint(model_);
          LinearConstraintProto linear = new LinearConstraintProto();
          foreach (KeyValuePair<IntVar, long> term in dict)
          {
            linear.Vars.Add(term.Key.Index);
            linear.Coeffs.Add(term.Value);
          }
          linear.Domain.Add(lin.Lb - constant);
          linear.Domain.Add(lin.Lb - constant);
          ct.Proto.Linear = linear;
          return ct;
        }
      case BoundIntegerExpression.Type.VarDiffCst:
        {
          Dictionary<IntVar, long> dict = new Dictionary<IntVar, long>();
          long constant = IntegerExpression.GetVarValueMap(lin.Left, 1L, dict);
          Constraint ct = new Constraint(model_);
          LinearConstraintProto linear = new LinearConstraintProto();
          foreach (KeyValuePair<IntVar, long> term in dict)
          {
            linear.Vars.Add(term.Key.Index);
            linear.Coeffs.Add(term.Value);
          }
          linear.Domain.Add(Int64.MinValue);
          linear.Domain.Add(lin.Lb - constant - 1);
          linear.Domain.Add(lin.Lb - constant + 1);
          linear.Domain.Add(Int64.MaxValue);
          ct.Proto.Linear = linear;
          return ct;
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

  public Constraint AddElement(IntVar index, IEnumerable<IntVar> vars,
                               IntVar target)
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

  public Constraint AddElement(IntVar index, IEnumerable<long> values,
                               IntVar target)
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

  public Constraint AddElement(IntVar index, IEnumerable<int> values,
                               IntVar target)
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

  public Constraint AddAllowedAssignments(IEnumerable<IntVar> vars,
                                          long[,] tuples)
  {
    Constraint ct = new Constraint(model_);
    TableConstraintProto table = new TableConstraintProto();
    foreach (IntVar var in vars)
    {
      table.Vars.Add(var.Index);
    }
    for (int i = 0; i < tuples.GetLength(0); ++i)
    {
      for (int j = 0; j < tuples.GetLength(1);++j)
      {
        table.Values.Add(tuples[i, j]);
      }
    }
    ct.Proto.Table = table;
    return ct;
  }

  public Constraint AddForbiddenAssignments(IEnumerable<IntVar> vars,
                                            long[,] tuples)
  {
    Constraint ct = AddAllowedAssignments(vars, tuples);
    ct.Proto.Table.Negated = true;
    return ct;
  }

  public Constraint AddAutomaton(IEnumerable<IntVar> vars,
                                long starting_state,
                                long[,] transitions,
                                IEnumerable<long> final_states) {
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

  public Constraint AddAutomaton(
      IEnumerable<IntVar> vars,
      long starting_state,
      IEnumerable<Tuple<long, long, long>> transitions,
      IEnumerable<long> final_states) {
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

  public Constraint AddInverse(IEnumerable<IntVar> direct,
                               IEnumerable<IntVar> reverse)
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

  public Constraint AddReservoirConstraint<I>(IEnumerable<IntVar> times,
                                              IEnumerable<I> demands,
                                              long min_level, long max_level)
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

    ct.Proto.Reservoir = res;
    return ct;
  }

  public Constraint AddReservoirConstraintWithActive<I>(
    IEnumerable<IntVar> times,
    IEnumerable<I> demands,
    IEnumerable<IntVar> actives,
    long min_level, long max_level)
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

    ct.Proto.Reservoir = res;
    return ct;
  }

  public void AddMapDomain(
      IntVar var, IEnumerable<IntVar> bool_vars, long offset = 0)
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

  public Constraint AddMinEquality(IntVar target, IEnumerable<IntVar> vars)
  {
    Constraint ct = new Constraint(model_);
    IntegerArgumentProto args = new IntegerArgumentProto();
    foreach (IntVar var in vars)
    {
      args.Vars.Add(var.Index);
    }
    args.Target = target.Index;
    ct.Proto.IntMin = args;
    return ct;
  }

  public Constraint AddMaxEquality(IntVar target, IEnumerable<IntVar> vars)
  {
    Constraint ct = new Constraint(model_);
    IntegerArgumentProto args = new IntegerArgumentProto();
    foreach (IntVar var in vars)
    {
      args.Vars.Add(var.Index);
    }
    args.Target = target.Index;
    ct.Proto.IntMax = args;
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

  public Constraint AddAbsEquality(IntVar target, IntVar var)
  {
    Constraint ct = new Constraint(model_);
    IntegerArgumentProto args = new IntegerArgumentProto();
    args.Vars.Add(var.Index);
    args.Vars.Add(-var.Index - 1);
    args.Target = target.Index;
    ct.Proto.IntMax = args;
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

  public Constraint AddProdEquality(IntVar target, IEnumerable<IntVar> vars)
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

  // Scheduling support

  public IntervalVar NewIntervalVar<S, D, E>(
      S start, D duration, E end, string name) {
    return new IntervalVar(model_,
                           GetOrCreateIndex(start),
                           GetOrCreateIndex(duration),
                           GetOrCreateIndex(end),
                           name);
  }


  public IntervalVar NewOptionalIntervalVar<S, D, E>(
      S start, D duration, E end, ILiteral is_present, string name) {
    int i = is_present.GetIndex();
    return new IntervalVar(model_,
                           GetOrCreateIndex(start),
                           GetOrCreateIndex(duration),
                           GetOrCreateIndex(end),
                           i,
                           name);
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

  public Constraint AddNoOverlap2D(IEnumerable<IntervalVar> x_intervals,
                                   IEnumerable<IntervalVar> y_intervals)
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

  public Constraint AddCumulative<D, C>(IEnumerable<IntervalVar> intervals,
                                        IEnumerable<D> demands,
                                        C capacity) {
    Constraint ct = new Constraint(model_);
    CumulativeConstraintProto cumul = new CumulativeConstraintProto();
    foreach (IntervalVar var in intervals)
    {
      cumul.Intervals.Add(var.GetIndex());
    }
    foreach (D demand in demands)
    {
      cumul.Demands.Add(GetOrCreateIndex(demand));
    }
    cumul.Capacity = GetOrCreateIndex(capacity);
    ct.Proto.Cumulative = cumul;
    return ct;
  }


  // Objective.
  public void Minimize(IntegerExpression obj)
  {
    SetObjective(obj, true);
  }

  public void Maximize(IntegerExpression obj)
  {
    SetObjective(obj, false);
  }

  bool HasObjective()
  {
    return model_.Objective == null;
  }

  // Search Decision.

  public void AddDecisionStrategy(IEnumerable<IntVar> vars,
                                  DecisionStrategyProto.Types.VariableSelectionStrategy var_str,
                                  DecisionStrategyProto.Types.DomainReductionStrategy dom_str) {
    DecisionStrategyProto ds = new DecisionStrategyProto();
    foreach (IntVar var in vars)
    {
      ds.Variables.Add(var.Index);
    }
    ds.VariableSelectionStrategy = var_str;
    ds.DomainReductionStrategy = dom_str;
    model_.SearchStrategy.Add(ds);
  }

  // Internal methods.

  void SetObjective(IntegerExpression obj, bool minimize)
  {
    CpObjectiveProto objective = new CpObjectiveProto();
    if (obj is IntVar)
    {
      objective.Coeffs.Add(1L);
      objective.Offset = 0L;
      if (minimize)
      {
        objective.Vars.Add(obj.Index);
        objective.ScalingFactor = 1L;
      }
      else
      {
        objective.Vars.Add(Negated(obj.Index));
        objective.ScalingFactor = -1L;
      }
    }
    else
    {
      Dictionary<IntVar, long> dict = new Dictionary<IntVar, long>();
      long constant = IntegerExpression.GetVarValueMap(obj, 1L, dict);
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
        objective.Coeffs.Add(it.Value);
        if (minimize)
        {
          objective.Vars.Add(it.Key.Index);
        }
        else
        {
          objective.Vars.Add(Negated(it.Key.Index));
        }
      }
    }
    model_.Objective = objective;
  }

  public String ModelStats() {
    return SatHelper.ModelStats(model_);
  }

 public String Validate() {
    return SatHelper.ValidateModel(model_);
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

  private CpModelProto model_;
  private Dictionary<long, int> constant_map_;
}

}  // namespace Google.OrTools.Sat
