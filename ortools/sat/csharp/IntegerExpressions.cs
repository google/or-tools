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

// Helpers.

// IntVar[] helper class.
public static class IntVarArrayHelper
{
  public static SumArray Sum(this IntVar[] vars)
  {
    return new SumArray(vars);
  }
  public static SumArray ScalProd(this IntVar[] vars, int[] coeffs)
  {
    IntegerExpression[] exprs = new IntegerExpression[vars.Length];
    for (int i = 0; i < vars.Length; ++i) {
      exprs[i] = vars[i] * coeffs[i];
    }
    return new SumArray(exprs);
  }
  public static SumArray ScalProd(this IntVar[] vars, long[] coeffs)
  {
    IntegerExpression[] exprs = new IntegerExpression[vars.Length];
    for (int i = 0; i < vars.Length; ++i) {
      exprs[i] = vars[i] * coeffs[i];
    }
    return new SumArray(exprs);
  }
}

public interface ILiteral
{
  ILiteral Not();
  int GetIndex();
}

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

  public virtual string ShortString()
  {
    return ToString();
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

  public static BoundIntegerExpression operator ==(IntegerExpression a,
                                                   IntegerExpression b) {
    return new BoundIntegerExpression(a, b, true);
  }

  public static BoundIntegerExpression operator !=(IntegerExpression a,
                                                   IntegerExpression b) {
    return new BoundIntegerExpression(a, b, false);
  }

  public static BoundIntegerExpression operator ==(IntegerExpression a,
                                                   long v) {
    return new BoundIntegerExpression(a, v, true);
  }

  public static BoundIntegerExpression operator !=(IntegerExpression a,
                                                   long v) {
    return new BoundIntegerExpression(a, v, false);
  }

  public static BoundIntegerExpression operator >=(IntegerExpression a,
                                                   long v) {
    return new BoundIntegerExpression(v, a, Int64.MaxValue);
  }

  public static BoundIntegerExpression operator >=(long v,
                                                   IntegerExpression a) {
    return a <= v;
  }

  public static BoundIntegerExpression operator >(IntegerExpression a,
                                                  long v) {
    return new BoundIntegerExpression(v + 1, a, Int64.MaxValue);
  }

  public static BoundIntegerExpression operator >(long v, IntegerExpression a) {
    return a < v;
  }

  public static BoundIntegerExpression operator <=(IntegerExpression a,
                                                   long v) {
    return new BoundIntegerExpression(Int64.MinValue, a, v);
  }

    public static BoundIntegerExpression operator <=(long v,
                                                     IntegerExpression a) {
      return a >= v;
  }

  public static BoundIntegerExpression operator <(IntegerExpression a,
                                                  long v) {
    return new BoundIntegerExpression(Int64.MinValue, a, v - 1);
  }

  public static BoundIntegerExpression operator <(long v, IntegerExpression a) {
    return a > v;
  }

  public static BoundIntegerExpression operator >=(IntegerExpression a,
                                                   IntegerExpression b) {
    return new BoundIntegerExpression(0, a - b, Int64.MaxValue);
  }

  public static BoundIntegerExpression operator >(IntegerExpression a,
                                                  IntegerExpression b) {
    return new BoundIntegerExpression(1, a - b, Int64.MaxValue);
  }

  public static BoundIntegerExpression operator <=(IntegerExpression a,
                                                   IntegerExpression b) {
    return new BoundIntegerExpression(Int64.MinValue, a - b, 0);
  }

  public static BoundIntegerExpression operator <(IntegerExpression a,
                                                  IntegerExpression b) {
    return new BoundIntegerExpression(Int64.MinValue, a - b, -1);
  }

  public static IntegerExpression Prod(IntegerExpression e, long v)
  {
    if (v == 1)
    {
      return e;
    }
    else if (e is ProductCst)
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
                                    long initial_coeff,
                                    Dictionary<IntVar, long> dict)
  {
    List<IntegerExpression> exprs = new List<IntegerExpression>();
    List<long> coeffs = new List<long>();
    exprs.Add(e);
    coeffs.Add(initial_coeff);
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
          coeffs.Add(p.Coeff * coeff);
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
    constant_ = 0L;
  }

  public SumArray(IntegerExpression a, long b)
  {
    expressions_ = new List<IntegerExpression>();
    expressions_.Add(a);
    constant_ = b;
  }

  public SumArray(IEnumerable<IntegerExpression> exprs)
  {
    expressions_ = new List<IntegerExpression>();
    foreach (IntegerExpression e in exprs)
    {
      if (e != null)
      {
        expressions_.Add(e);
      }
    }
    constant_ = 0L;
  }

  public SumArray(IEnumerable<IntegerExpression> exprs, long cte)
  {
    expressions_ = new List<IntegerExpression>();
    foreach (IntegerExpression e in exprs)
    {
      if (e != null)
      {
        expressions_.Add(e);
      }
    }
    constant_ = cte;
  }

  public List<IntegerExpression> Expressions
  {
    get { return expressions_; }
  }

  public long Constant
  {
    get { return constant_; }
  }

  public override string ShortString()
  {
    return String.Format("({0})", ToString());
  }

  public override string ToString()
  {
    string result = "";
    for (int i = 0; i < expressions_.Count; ++i)
    {
      bool negated = false;
      IntegerExpression expr = expressions_[i];
      if (i != 0)
      {
        if (expr is ProductCst && ((ProductCst)expr).Coeff < 0)
        {
          result += String.Format(" - ");
          negated = true;
        }
        else
        {
          result += String.Format(" + ");
        }
      }

      if (expr is IntVar)
      {
        result += expr.ShortString();
      }
      else if (expr is ProductCst)
      {
        ProductCst p = (ProductCst)expr;
        long coeff = negated ? -p.Coeff : p.Coeff;
        IntegerExpression sub = p.Expr;
        if (coeff == 1)
        {
          result += sub.ShortString();
        }
        else if (coeff == -1)
        {
          result += String.Format("-{0}", coeff, sub.ShortString());
        }
        else
        {
          result += String.Format("{0}*{1}", coeff, sub.ShortString());
        }
      }
      else
      {
        result += String.Format("({0})", expr.ShortString());
      }
    }
    return result;
  }

  private List<IntegerExpression> expressions_;
  private long constant_;

}

public class IntVar : IntegerExpression, ILiteral
{
  public IntVar(CpModelProto model, IEnumerable<long> bounds,
                int is_present_index, string name) {
    model_ = model;
    index_ = model.Variables.Count;
    var_ = new IntegerVariableProto();
    var_.Name = name;
    var_.Domain.Add(bounds);
    var_.EnforcementLiteral.Add(is_present_index);
    model.Variables.Add(var_);
    negation_ = null;
  }

  public IntVar(CpModelProto model, IEnumerable<long> bounds, string name) {
    model_ = model;
    index_ = model.Variables.Count;
    var_ = new IntegerVariableProto();
    var_.Name = name;
    var_.Domain.Add(bounds);
    model.Variables.Add(var_);
    negation_ = null;
  }

  public override int GetIndex()
  {
    return index_;
  }

  public override string ToString()
  {
    return var_.ToString();
  }

  public override string ShortString()
  {
    if (var_.Name != null)
    {
      return var_.Name;
    }
    else
    {
      return var_.ToString();
    }
  }

  public string Name()
  {
    return var_.Name;
  }

  public ILiteral Not()
  {
    foreach (long b in var_.Domain)
    {
      if (b < 0 || b > 1)
      {
        throw new ArgumentException(
            "Cannot call Not() on a non boolean variable");
      }
    }
    if (negation_ == null)
    {
      negation_ = new NotBooleanVariable(this);
    }
    return negation_;
  }


  private CpModelProto model_;
  private int index_;
  private List<long> bounds_;
  private IntegerVariableProto var_;
  private NotBooleanVariable negation_;
}

public class NotBooleanVariable : IntegerExpression, ILiteral
{
  public NotBooleanVariable(IntVar boolvar)
  {
    boolvar_ = boolvar;
  }

  public override int GetIndex()
  {
    return -boolvar_.Index - 1;
  }

  public ILiteral Not()
  {
    return boolvar_;
  }

  public string ShortString()
  {
    return String.Format("Not({0})", boolvar_.ShortString());
  }

  private IntVar boolvar_;
}

public class BoundIntegerExpression
{
  public enum Type
  {
    BoundExpression,
    VarEqVar,
    VarDiffVar,
    VarEqCst,
    VarDiffCst,
  }

  public BoundIntegerExpression(long lb, IntegerExpression expr, long ub)
  {
    left_ = expr;
    right_ = null;
    lb_ = lb;
    ub_ = ub;
    type_ = Type.BoundExpression;
  }

  public BoundIntegerExpression(IntegerExpression left, IntegerExpression right,
                                bool equality) {
    left_ = left;
    right_ = right;
    lb_ = 0;
    ub_ = 0;
    type_ = equality ? Type.VarEqVar : Type.VarDiffVar;
  }

  public BoundIntegerExpression(IntegerExpression left, long v, bool equality) {
    left_ = left;
    right_ = null;
    lb_ = v;
    ub_ = 0;
    type_ = equality ? Type.VarEqCst : Type.VarDiffCst;
  }

  bool IsTrue()
  {
    if (type_ == Type.VarEqVar)
    {
      return (object)left_ == (object)right_;
    }
    else if (type_ == Type.VarDiffVar)
    {
      return (object)left_ != (object)right_;
    }
    return false;
  }

  public static bool operator true(BoundIntegerExpression bie)
  {
    return bie.IsTrue();
  }

  public static bool operator false(BoundIntegerExpression bie)
  {
    return !bie.IsTrue();
  }

  public override string ToString()
  {
    switch (type_)
    {
      case Type.BoundExpression:
        return String.Format("{0} <= {1} <= {2}", lb_, left_, ub_);
      case Type.VarEqVar:
        return String.Format("{0} == {1}", left_, right_);
      case Type.VarDiffVar:
        return String.Format("{0} != {1}", left_, right_);
      case Type.VarEqCst:
        return String.Format("{0} == {1}", left_, lb_);
      case Type.VarDiffCst:
        return String.Format("{0} != {1}", left_, lb_);
      default:
        throw new ArgumentException("Wrong mode in BoundIntegerExpression.");
    }
  }

  public static BoundIntegerExpression operator <=(BoundIntegerExpression a,
                                                   long v) {
    if (a.CtType != Type.BoundExpression || a.Ub != Int64.MaxValue)
    {
      throw new ArgumentException(
          "Operator <= not supported for this BoundIntegerExpression");
    }
    return new BoundIntegerExpression(a.Lb, a.Left, v);
  }

  public static BoundIntegerExpression operator <(BoundIntegerExpression a,
                                                  long v) {
    if (a.CtType != Type.BoundExpression || a.Ub != Int64.MaxValue)
    {
      throw new ArgumentException(
          "Operator < not supported for this BoundIntegerExpression");
    }
    return new BoundIntegerExpression(a.Lb, a.Left, v - 1);
  }

  public static BoundIntegerExpression operator >=(BoundIntegerExpression a,
                                                   long v) {
    if (a.CtType != Type.BoundExpression || a.Lb != Int64.MinValue)
    {
      throw new ArgumentException(
          "Operator >= not supported for this BoundIntegerExpression");
    }
    return new BoundIntegerExpression(v, a.Left, a.Ub);
  }

  public static BoundIntegerExpression operator >(BoundIntegerExpression a,
                                                  long v) {
    if (a.CtType != Type.BoundExpression || a.Lb != Int64.MinValue)
    {
      throw new ArgumentException(
          "Operator < not supported for this BoundIntegerExpression");
    }
    return new BoundIntegerExpression(v + 1, a.Left, a.Ub);
  }

  public IntegerExpression Left
  {
    get { return left_; }
  }

  public IntegerExpression Right
  {
    get { return right_; }
  }

  public long Lb
  {
    get { return lb_; }
  }

  public long Ub
  {
    get { return ub_; }
  }

  public Type CtType
  {
    get { return type_; }
  }

  private IntegerExpression left_;
  private IntegerExpression right_;
  private long lb_;
  private long ub_;
  private Type type_;
}

}  // namespace Google.OrTools.Sat
