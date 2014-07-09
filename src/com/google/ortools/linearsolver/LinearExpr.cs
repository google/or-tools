// Copyright 2010-2014 Google
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

namespace Google.OrTools.LinearSolver
{
  using System;
  using System.Collections.Generic;

public class LinearExpr
{
  public virtual double DoVisit(Dictionary<Variable, double> coefficients,
                                double multiplier)
  {
    return 0;
  }

  public double Visit(Dictionary<Variable, double> coefficients)
  {
    return DoVisit(coefficients, 1.0);
  }

  public static LinearExpr operator+(LinearExpr a, double v)
  {
    return new SumCst(a, v);
  }

  public static LinearExpr operator+(double v, LinearExpr a)
  {
    return new SumCst(a, v);
  }

  public static LinearExpr operator+(LinearExpr a, LinearExpr b)
  {
    return new Sum(a, b);
  }

  public static LinearExpr operator-(LinearExpr a, double v)
  {
    return new SumCst(a, -v);
  }

  public static LinearExpr operator-(double v, LinearExpr a)
  {
    return new SumCst(new ProductCst(a, -1.0), v);
  }

  public static LinearExpr operator-(LinearExpr a, LinearExpr b)
  {
    return new Sum(a, new ProductCst(b, -1.0));
  }

  public static LinearExpr operator-(LinearExpr a)
  {
    return new ProductCst(a, -1.0);
  }

  public static LinearExpr operator*(LinearExpr a, double v)
  {
    return new ProductCst(a, v);
  }

  public static LinearExpr operator/(LinearExpr a, double v)
  {
    return new ProductCst(a, 1 / v);
  }

  public static LinearExpr operator*(double v, LinearExpr a)
  {
    return new ProductCst(a, v);
  }

  public static RangeConstraint operator==(LinearExpr a, double v)
  {
    return new RangeConstraint(a, v, v);
  }

  public static RangeConstraint operator==(double v, LinearExpr a)
  {
    return new RangeConstraint(a, v, v);
  }

  public static RangeConstraint operator!=(LinearExpr a, double v)
  {
    return new RangeConstraint(a, 1, -1);
  }

  public static RangeConstraint operator!=(double v, LinearExpr a)
  {
    return new RangeConstraint(a, 1, -1);
  }

  public static Equality operator==(LinearExpr a, LinearExpr b)
  {
    return new Equality(a, b, true);
  }

  public static Equality operator!=(LinearExpr a, LinearExpr b)
  {
    return new Equality(a, b, false);
  }

  public static RangeConstraint operator<=(LinearExpr a, double v)
  {
    return new RangeConstraint(a, double.NegativeInfinity, v);
  }

  public static RangeConstraint operator>=(LinearExpr a, double v)
  {
    return new RangeConstraint(a, v, double.PositiveInfinity);
  }

    public static RangeConstraint operator<=(LinearExpr a, LinearExpr b)
  {
    return a - b <= 0;
  }

  public static RangeConstraint operator>=(LinearExpr a, LinearExpr b)
  {
    return a - b >= 0;
  }

  public static implicit operator LinearExpr(Variable a)
  {
    return new VarWrapper(a);
  }
}

public static class LinearExprArrayHelper
{
  public static LinearExpr Sum(this LinearExpr[] exprs)
  {
    return new SumArray(exprs);
  }

  public static LinearExpr Sum(this Variable[] vars)
  {
    return new SumVarArray(vars);
  }
}

  // def __ge__(self, arg):
  //   if isinstance(arg, (int, long, float)):
  //     return LinearConstraint(self, arg, 1e308)
  //   else:
  //     return LinearConstraint(Sum(self, ProductCst(arg, -1)), 0.0, 1e308)

  // def __le__(self, arg):
  //   if isinstance(arg, (int, long, float)):
  //     return LinearConstraint(self, -1e308, arg)
  //   else:
  //     return LinearConstraint(Sum(self, ProductCst(arg, -1)), -1e308, 0.0)


class ProductCst : LinearExpr
{
  public ProductCst(LinearExpr expr, double coeff)
  {
    this.coeff_ = coeff;
    this.expr_ = expr;
  }

  public override String ToString()
  {
    return "(" + expr_.ToString() + " * " + coeff_ + ")";
  }

  public override double DoVisit(Dictionary<Variable, double> coefficients,
                                 double multiplier)
  {
    double current_multiplier = multiplier * coeff_;
    if (current_multiplier != 0.0)
    {
      return expr_.DoVisit(coefficients, current_multiplier);
    }
    else
    {
      return 0.0;
    }
  }

  private LinearExpr expr_;
  private double coeff_;
}

class SumCst : LinearExpr
{
  public SumCst(LinearExpr expr, double coeff)
  {
    this.coeff_ = coeff;
    this.expr_ = expr;
  }

  public override String ToString()
  {
    return "(" + expr_.ToString() + " + " + coeff_ + ")";
  }

  public override double DoVisit(Dictionary<Variable, double> coefficients,
                                 double multiplier)
  {
    if (multiplier != 0.0)
    {
      return coeff_ * multiplier + expr_.DoVisit(coefficients, multiplier);
    }
    else
    {
      return 0.0;
    }
  }

  private LinearExpr expr_;
  private double coeff_;
}

class VarWrapper : LinearExpr
{
  public VarWrapper(Variable var)
  {
    this.var_ = var;
  }

  public override String ToString()
  {
    return var_.Name();
  }

  public override double DoVisit(Dictionary<Variable, double> coefficients,
                                 double multiplier)
  {
    if (multiplier != 0.0)
    {
      if (coefficients.ContainsKey(var_))
      {
        coefficients[var_] += multiplier;
      }
      else
      {
        coefficients[var_] = multiplier;
      }
    }
    return 0.0;
  }

  private Variable var_;
}


class Sum : LinearExpr
{
  public Sum(LinearExpr left, LinearExpr right)
  {
    this.left_ = left;
    this.right_ = right;
  }

  public override String ToString()
  {
    return "(" + left_.ToString() + " + " + right_.ToString() + ")";
  }

  public override double DoVisit(Dictionary<Variable, double> coefficients,
                                 double multiplier)
  {
    if (multiplier != 0.0)
    {
      return left_.DoVisit(coefficients, multiplier) +
          right_.DoVisit(coefficients, multiplier);
    }
    else
    {
      return 0.0;
    }
  }

  private LinearExpr left_;
  private LinearExpr right_;
}

public class SumArray : LinearExpr
{
  public SumArray(LinearExpr[] array)
  {
    this.array_ = array;
  }

  public override double DoVisit(Dictionary<Variable, double> coefficients,
                                 double multiplier) {
    if (multiplier != 0.0)
    {
      double constant = 0.0;
      foreach (LinearExpr expr in array_)
      {
        constant += expr.DoVisit(coefficients, multiplier);
      }
      return constant;
    }
    else
    {
      return 0.0;
    }
  }

  private LinearExpr[] array_;
}

public class SumVarArray : LinearExpr
{
  public SumVarArray(Variable[] array)
  {
    this.array_ = array;
  }

  public override double DoVisit(Dictionary<Variable, double> coefficients,
                                 double multiplier) {
    if (multiplier != 0.0)
    {
      foreach (Variable var in array_)
      {
        if (coefficients.ContainsKey(var))
        {
          coefficients[var] += multiplier;
        }
        else
        {
          coefficients[var] = multiplier;
        }
      }
    }
    return 0.0;
  }

  private Variable[] array_;
}
}  // namespace Google.OrTools.LinearSolver
