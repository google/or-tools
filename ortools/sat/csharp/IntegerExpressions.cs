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
  using Google.OrTools.Util;

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
      LinearExpression[] exprs = new LinearExpression[vars.Length];
      for (int i = 0; i < vars.Length; ++i)
      {
        exprs[i] = vars[i] * coeffs[i];
      }
      return new SumArray(exprs);
    }
    public static SumArray ScalProd(this IntVar[] vars, long[] coeffs)
    {
      LinearExpression[] exprs = new LinearExpression[vars.Length];
      for (int i = 0; i < vars.Length; ++i)
      {
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

  // Holds an linear expression.
  public class LinearExpression
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

    public static LinearExpression operator +(LinearExpression a,
                                              LinearExpression b)
    {
      return new SumArray(a, b);
    }

    public static LinearExpression operator +(LinearExpression a, long v)
    {
      return new SumArray(a, v);
    }

    public static LinearExpression operator +(long v, LinearExpression a)
    {
      return new SumArray(a, v);
    }

    public static LinearExpression operator -(LinearExpression a,
                                              LinearExpression b)
    {
      return new SumArray(a, Prod(b, -1));
    }

    public static LinearExpression operator -(LinearExpression a, long v)
    {
      return new SumArray(a, -v);
    }

    public static LinearExpression operator -(long v, LinearExpression a)
    {
      return new SumArray(Prod(a, -1), v);
    }

    public static LinearExpression operator *(LinearExpression a, long v)
    {
      return Prod(a, v);
    }

    public static LinearExpression operator *(long v, LinearExpression a)
    {
      return Prod(a, v);
    }

    public static LinearExpression operator -(LinearExpression a)
    {
      return Prod(a, -1);
    }

    public static BoundedLinearExpression operator ==(LinearExpression a,
                                                      LinearExpression b)
    {
      return new BoundedLinearExpression(a, b, true);
    }

    public static BoundedLinearExpression operator !=(LinearExpression a,
                                                      LinearExpression b)
    {
      return new BoundedLinearExpression(a, b, false);
    }

    public static BoundedLinearExpression operator ==(LinearExpression a,
                                                     long v)
    {
      return new BoundedLinearExpression(a, v, true);
    }

    public static BoundedLinearExpression operator !=(LinearExpression a,
                                                     long v)
    {
      return new BoundedLinearExpression(a, v, false);
    }

    public static BoundedLinearExpression operator >=(LinearExpression a,
                                                     long v)
    {
      return new BoundedLinearExpression(v, a, Int64.MaxValue);
    }

    public static BoundedLinearExpression operator >=(long v,
                                                     LinearExpression a)
    {
      return a <= v;
    }

    public static BoundedLinearExpression operator >(LinearExpression a,
                                                    long v)
    {
      return new BoundedLinearExpression(v + 1, a, Int64.MaxValue);
    }

    public static BoundedLinearExpression operator >(long v, LinearExpression a)
    {
      return a < v;
    }

    public static BoundedLinearExpression operator <=(LinearExpression a,
                                                     long v)
    {
      return new BoundedLinearExpression(Int64.MinValue, a, v);
    }

    public static BoundedLinearExpression operator <=(long v,
                                                     LinearExpression a)
    {
      return a >= v;
    }

    public static BoundedLinearExpression operator <(LinearExpression a,
                                                    long v)
    {
      return new BoundedLinearExpression(Int64.MinValue, a, v - 1);
    }

    public static BoundedLinearExpression operator <(long v, LinearExpression a)
    {
      return a > v;
    }

    public static BoundedLinearExpression operator >=(LinearExpression a,
                                                     LinearExpression b)
    {
      return new BoundedLinearExpression(0, a - b, Int64.MaxValue);
    }

    public static BoundedLinearExpression operator >(LinearExpression a,
                                                    LinearExpression b)
    {
      return new BoundedLinearExpression(1, a - b, Int64.MaxValue);
    }

    public static BoundedLinearExpression operator <=(LinearExpression a,
                                                     LinearExpression b)
    {
      return new BoundedLinearExpression(Int64.MinValue, a - b, 0);
    }

    public static BoundedLinearExpression operator <(LinearExpression a,
                                                    LinearExpression b)
    {
      return new BoundedLinearExpression(Int64.MinValue, a - b, -1);
    }

    public static LinearExpression Prod(LinearExpression e, long v)
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

    public static long GetVarValueMap(LinearExpression e,
                                      long initial_coeff,
                                      Dictionary<IntVar, long> dict)
    {
      List<LinearExpression> exprs = new List<LinearExpression>();
      List<long> coeffs = new List<long>();
      exprs.Add(e);
      coeffs.Add(initial_coeff);
      long constant = 0;

      while (exprs.Count > 0)
      {
        LinearExpression expr = exprs[0];
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
          foreach (LinearExpression sub in a.Expressions)
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

  public class ProductCst : LinearExpression
  {
    public ProductCst(LinearExpression e, long v)
    {
      expr_ = e;
      coeff_ = v;
    }

    public LinearExpression Expr
    {
      get { return expr_; }
    }

    public long Coeff
    {
      get { return coeff_; }
    }

    private LinearExpression expr_;
    private long coeff_;

  }

  public class SumArray : LinearExpression
  {
    public SumArray(LinearExpression a, LinearExpression b)
    {
      expressions_ = new List<LinearExpression>();
      expressions_.Add(a);
      expressions_.Add(b);
      constant_ = 0L;
    }

    public SumArray(LinearExpression a, long b)
    {
      expressions_ = new List<LinearExpression>();
      expressions_.Add(a);
      constant_ = b;
    }

    public SumArray(IEnumerable<LinearExpression> exprs)
    {
      expressions_ = new List<LinearExpression>();
      foreach (LinearExpression e in exprs)
      {
        if (e != null)
        {
          expressions_.Add(e);
        }
      }
      constant_ = 0L;
    }

    public SumArray(IEnumerable<LinearExpression> exprs, long cte)
    {
      expressions_ = new List<LinearExpression>();
      foreach (LinearExpression e in exprs)
      {
        if (e != null)
        {
          expressions_.Add(e);
        }
      }
      constant_ = cte;
    }

    public List<LinearExpression> Expressions
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
        LinearExpression expr = expressions_[i];
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
          LinearExpression sub = p.Expr;
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

    private List<LinearExpression> expressions_;
    private long constant_;

  }

  public class IntVar : LinearExpression, ILiteral
  {
    public IntVar(CpModelProto model, Domain domain, string name)
    {
      model_ = model;
      index_ = model.Variables.Count;
      var_ = new IntegerVariableProto();
      var_.Name = name;
      var_.Domain.Add(domain.FlattenedIntervals());
      model.Variables.Add(var_);
      negation_ = null;
    }

    public int Index
    {
      get { return index_; }
    }

    public override int GetIndex()
    {
      return index_;
    }

    public IntegerVariableProto Proto
    {
      get { return var_; }
      set { var_ = value; }
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

  public class NotBooleanVariable : LinearExpression, ILiteral
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

    public override string ShortString()
    {
      return String.Format("Not({0})", boolvar_.ShortString());
    }

    private IntVar boolvar_;
  }

  public class BoundedLinearExpression
  {
    public enum Type
    {
      BoundExpression,
      VarEqVar,
      VarDiffVar,
      VarEqCst,
      VarDiffCst,
    }

    public BoundedLinearExpression(long lb, LinearExpression expr, long ub)
    {
      left_ = expr;
      right_ = null;
      lb_ = lb;
      ub_ = ub;
      type_ = Type.BoundExpression;
    }

    public BoundedLinearExpression(LinearExpression left, LinearExpression right,
                                  bool equality)
    {
      left_ = left;
      right_ = right;
      lb_ = 0;
      ub_ = 0;
      type_ = equality ? Type.VarEqVar : Type.VarDiffVar;
    }

    public BoundedLinearExpression(LinearExpression left, long v, bool equality)
    {
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

    public static bool operator true(BoundedLinearExpression bie)
    {
      return bie.IsTrue();
    }

    public static bool operator false(BoundedLinearExpression bie)
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
          throw new ArgumentException("Wrong mode in BoundedLinearExpression.");
      }
    }

    public static BoundedLinearExpression operator <=(BoundedLinearExpression a,
                                                     long v)
    {
      if (a.CtType != Type.BoundExpression || a.Ub != Int64.MaxValue)
      {
        throw new ArgumentException(
            "Operator <= not supported for this BoundedLinearExpression");
      }
      return new BoundedLinearExpression(a.Lb, a.Left, v);
    }

    public static BoundedLinearExpression operator <(BoundedLinearExpression a,
                                                    long v)
    {
      if (a.CtType != Type.BoundExpression || a.Ub != Int64.MaxValue)
      {
        throw new ArgumentException(
            "Operator < not supported for this BoundedLinearExpression");
      }
      return new BoundedLinearExpression(a.Lb, a.Left, v - 1);
    }

    public static BoundedLinearExpression operator >=(BoundedLinearExpression a,
                                                     long v)
    {
      if (a.CtType != Type.BoundExpression || a.Lb != Int64.MinValue)
      {
        throw new ArgumentException(
            "Operator >= not supported for this BoundedLinearExpression");
      }
      return new BoundedLinearExpression(v, a.Left, a.Ub);
    }

    public static BoundedLinearExpression operator >(BoundedLinearExpression a,
                                                    long v)
    {
      if (a.CtType != Type.BoundExpression || a.Lb != Int64.MinValue)
      {
        throw new ArgumentException(
            "Operator < not supported for this BoundedLinearExpression");
      }
      return new BoundedLinearExpression(v + 1, a.Left, a.Ub);
    }

    public LinearExpression Left
    {
      get { return left_; }
    }

    public LinearExpression Right
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

    private LinearExpression left_;
    private LinearExpression right_;
    private long lb_;
    private long ub_;
    private Type type_;
  }

}  // namespace Google.OrTools.Sat
