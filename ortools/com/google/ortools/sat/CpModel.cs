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

  // TODO: AddElement

  // TODO: AddCircuit

  // TODO: AddAllowedAssignments

  // TODO: AddForbiddenAssignments

  // TODO: AddAutomata

  // TODO: AddInverse

  // TODO: AddReservoirConstraint

  // TODO: AddMapDomain

  // TODO: AddImplication

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
    // TODO: Implement me for general IntegerExpression.
  }

  private CpModelProto model_;
  private Dictionary<long, IntVar> constant_map_;
}

}  // namespace Google.OrTools.Sat