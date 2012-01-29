// Copyright 2010-2012 Google
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

public class LinearConstraint
{

  public virtual String ToString()
  {
    return "LinearConstraint";
  }

  public virtual MPConstraint Extract(MPSolver solver)
  {
    return null;
  }
}

public class RangeConstraint : LinearConstraint
{

  public RangeConstraint(LinearExpr expr, double lb, double ub)
  {
    this.expr_ = expr;
    this.lb_ = lb;
    this.ub_ = ub;
  }

  public override String ToString()
  {
    return "" + lb_ + " <= " + expr_.ToString() + " <= " + ub_;
  }

  public override MPConstraint Extract(MPSolver solver)
  {
    Dictionary<MPVariable, double> coefficients =
        new Dictionary<MPVariable, double>();
    double constant = expr_.Visit(coefficients);
    MPConstraint ct = solver.MakeConstraint(lb_ - constant, ub_ - constant);
    foreach (KeyValuePair<MPVariable, double> pair in coefficients)
    {
      ct.SetCoefficient(pair.Key, pair.Value);
    }
    return ct;
  }

  public static implicit operator bool(RangeConstraint ct)
  {
    return false;
  }

  private LinearExpr expr_;
  private double lb_;
  private double ub_;
}

public class Equality : LinearConstraint
{
  public Equality(LinearExpr left, LinearExpr right, bool equality)
  {
    this.left_ = left;
    this.right_ = right;
    this.equality_ = equality;
  }

  public override String ToString()
  {
    return "" + left_.ToString() + " == " + right_.ToString();
  }

  public override MPConstraint Extract(MPSolver solver)
  {
    Dictionary<MPVariable, double> coefficients =
        new Dictionary<MPVariable, double>();
    double constant = left_.Visit(coefficients);
    constant += right_.DoVisit(coefficients, -1);
    MPConstraint ct = solver.MakeConstraint(-constant, -constant);
    foreach (KeyValuePair<MPVariable, double> pair in coefficients)
    {
      ct.SetCoefficient(pair.Key, pair.Value);
    }
    return ct;
  }

  public static implicit operator bool(Equality ct)
  {
    return (object)ct.left_ == (object)ct.right_ ? ct.equality_ : !ct.equality_;
  }

  private LinearExpr left_;
  private LinearExpr right_;
  private bool equality_;
}
}  // namespace Google.OrTools.LinearSolver
