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

namespace Google.OrTools.Sat
{
using System;
using System.Collections.Generic;

// Holds an integer expression.
public class IntegerExpression
{

  public int Index
  {
    get { return GetIndex(); }
  }

  public virtual int GetIndex()
  {
    throw new NotImplementedException();
  }

  public static IntegerExpression operator+(IntegerExpression a,
                                            IntegerExpression b) {
    return new SumArray(a, b);
  }

  public static IntegerExpression operator+(IntegerExpression a, long v) {
    return new SumArray(a, v);
  }

  public static IntegerExpression operator+(long v, IntegerExpression a) {
    return new SumArray(a, v);
  }

  public static IntegerExpression operator-(IntegerExpression a,
                                            IntegerExpression b) {
    return new SumArray(a, Prod(b, -1));
  }

  public static IntegerExpression operator-(IntegerExpression a, long v) {
    return new SumArray(a, -v);
  }

  public static IntegerExpression operator-(long v, IntegerExpression a) {
    return new SumArray(Prod(a, -1), v);
  }

  public static IntegerExpression operator*(IntegerExpression a, long v) {
    return Prod(a, v);
  }

  public static IntegerExpression operator*(long v, IntegerExpression a) {
    return Prod(a, v);
  }

  public static IntegerExpression operator-(IntegerExpression a) {
    return Prod(a, -1);
  }

  public static IntegerExpression Prod(IntegerExpression e, long v)
  {
    if (e is ProductCst)
    {
      ProductCst p = (ProductCst)e;
      return new ProductCst(p.Expr, p.Coeff * v);
    }
    else
    {
      return new ProductCst(e, v);
    }
  }

  public static long GetVarValueMap(IntegerExpression e,
                                    Dictionary<IntVar, long> dict)
  {
    List<IntegerExpression> exprs = new List<IntegerExpression>();
    List<long> coeffs = new List<long>();
    exprs.Add(e);
    coeffs.Add(1L);
    long constant = 0;

    while (exprs.Count > 0)
    {
      IntegerExpression expr = exprs[0];
      exprs.RemoveAt(0);
      long coeff = coeffs[0];
      coeffs.RemoveAt(0);
      if (coeff == 0) continue;

      if (expr is ProductCst)
      {
        ProductCst p = (ProductCst)expr;
        if (p.Coeff != 0)
        {
          exprs.Add(p.Expr);
          coeffs.Add(p.Coeff);
        }
      }
      else if (expr is SumArray)
      {
        SumArray a = (SumArray)expr;
        constant += coeff * a.Constant;
        foreach (IntegerExpression sub in a.Expressions)
        {
          exprs.Add(sub);
          coeffs.Add(coeff);
        }
      }
      else if (expr is IntVar)
      {
        IntVar i = (IntVar)expr;
        if (dict.ContainsKey(i))
        {
          dict[i] += coeff;
        }
        else
        {
          dict.Add(i, coeff);
        }

      }
      else if (expr is NotBooleanVariable)
      {
        throw new ArgumentException(
            "Cannot interpret a literal in an integer expression.");
      }
      else
      {
        throw new ArgumentException("Cannot interpret '" + expr.ToString() +
                                    "' in an integer expression");
      }
    }
    return constant;
  }
}

public class ProductCst : IntegerExpression
{
  public ProductCst(IntegerExpression e, long v)
  {
    expr_ = e;
    coeff_ = v;
  }

  public IntegerExpression Expr
  {
    get { return expr_; }
  }

  public long Coeff
  {
    get { return coeff_; }
  }

  private IntegerExpression expr_;
  private long coeff_;

}

public class SumArray : IntegerExpression
{
  public SumArray(IntegerExpression a, IntegerExpression b)
  {
    expressions_ = new List<IntegerExpression>();
    expressions_.Add(a);
    expressions_.Add(b);
  }

  public SumArray(IntegerExpression a, long b)
  {
    expressions_.Add(a);
    constant_ = b;
  }

  public List<IntegerExpression> Expressions
  {
    get { return expressions_; }
  }

  public long Constant
  {
    get { return constant_; }
  }

  private List<IntegerExpression> expressions_;
  private long constant_;

}

public class IntVar : IntegerExpression
{
  public IntVar(CpModelProto model, IEnumerable<long> bounds, string name,
         int is_present_index) {
    model_ = model;
    index_ = model.Variables.Count;
    var_ = new IntegerVariableProto();
    var_.Name = name;
    var_.Domain.Add(bounds);
    var_.EnforcementLiteral.Add(is_present_index);
    model.Variables.Add(var_);
  }

  public IntVar(CpModelProto model, IEnumerable<long> bounds, string name) {
    model_ = model;
    index_ = model.Variables.Count;
    var_ = new IntegerVariableProto();
    var_.Name = name;
    var_.Domain.Add(bounds);
    model.Variables.Add(var_);
  }

  public override int GetIndex()
  {
    return index_;
  }

  public override string ToString()
  {
    return var_.ToString();
  }


  private CpModelProto model_;
  private int index_;
  private List<long> bounds_;
  private IntegerVariableProto var_;
}

class NotBooleanVariable : IntegerExpression
{
  NotBooleanVariable(IntVar boolvar)
  {
    boolvar_ = boolvar;
  }

  public override int GetIndex()
  {
    return -boolvar_.Index - 1;
  }

  IntVar Not()
  {
    return boolvar_;
  }

  private IntVar boolvar_;
}

public class BoundIntegerExpression
{

}

public class Constraint
{
  public Constraint(CpModelProto model)
  {
    index_ = model.Constraints.Count;
    constraint_ = new ConstraintProto();
    model.Constraints.Add(constraint_);
  }

  public void OnlyEnforceIf(IntegerExpression lit)
  {
    constraint_.EnforcementLiteral.Add(lit.Index);
  }

  public int Index
  {
    get  { return index_; }
  }

  public ConstraintProto Proto
  {
    get { return constraint_; }
  }

  private int index_;
  private ConstraintProto constraint_;
}

class IntervalVar
{

}

public class CpModel
{
  public CpModel()
  {
    model_ = new CpModelProto();
    constant_map_ = new Dictionary<long, IntVar>();
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

  public IntVar NewIntVar(IEnumerable<long> bounds, string name)
  {
    return new IntVar(model_, bounds, name);
  }

  // TODO: Add optional version of above 2 NewIntVar().

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

  // TODO: AddLinearConstraintWithBounds

  public Constraint Add(BoundIntegerExpression lin)
  {
    // TODO: Implement me.
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

  // TODO: AddElement

  // TODO: AddCircuit

  // TODO: AddAllowedAssignments

  // TODO: AddForbiddenAssignments

  // TODO: AddAutomata

  // TODO: AddInverse

  // TODO: AddReservoirConstraint

  // TODO: AddMapDomain

  // TODO: AddImplication

  // TODO: AddBoolOr

  // TODO: AddBoolAnd

  // TODO: AddBoolXOr

  // TODO: AddMinEquality

  // TODO: AddMaxEquality

  // TODO: AddDivisionEquality

  // TODO: AddModuloEquality

  // TODO: AddProdEquality

  // Scheduling support

  // TODO: NewInterval

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
      long constant = IntegerExpression.GetVarValueMap(obj, dict);
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
    // TODO: Implement me for general IntegerExpression.
  }

  private CpModelProto model_;
  private Dictionary<long, IntVar> constant_map_;
}

public class CpSolver
{
  public CpSolver()
  {
  }

  public CpSolverStatus Solve(CpModel model)
  {
    if (string_parameters_ != null)
    {
      response_ = SatHelper.SolveWithStringParameters(model.Model,
                                                      string_parameters_);
    }
    else
    {
      response_ = SatHelper.Solve(model.Model);
    }
    return response_.Status;
  }

  public string StringParameters
  {
    get { return string_parameters_; }
    set { string_parameters_ = value; }
  }

  public CpSolverResponse Response
  {
    get { return response_; }
  }

  private CpModelProto model_;
  private CpSolverResponse response_;
  string string_parameters_;
}

}  // namespace Google.OrTools.Sat