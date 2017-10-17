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

namespace Google.OrTools.ConstraintSolver
{
using System;
using System.Collections.Generic;

public interface IConstraintWithStatus
{
  Solver solver();
  IntVar Var();
}

public abstract class BaseEquality : IConstraintWithStatus
{
  abstract public Solver solver();
  abstract public IntVar Var();

  public static IntExpr operator+(BaseEquality a, BaseEquality b) {
    return a.solver().MakeSum(a.Var(), b.Var());
  }
  public static IntExpr operator+(BaseEquality a, long v) {
    return a.solver().MakeSum(a.Var(), v);
  }
  public static IntExpr operator+(long v, BaseEquality a) {
    return a.solver().MakeSum(a.Var(), v);
  }
  public static IntExpr operator-(BaseEquality a, BaseEquality b) {
    return a.solver().MakeDifference(a.Var(), b.Var());
  }
  public static IntExpr operator-(BaseEquality a, long v) {
    return a.solver().MakeSum(a.Var(), -v);
  }
  public static IntExpr operator-(long v, BaseEquality a) {
    return a.solver().MakeDifference(v, a.Var());
  }
  public static IntExpr operator*(BaseEquality a, BaseEquality b) {
    return a.solver().MakeProd(a.Var(), b.Var());
  }
  public static IntExpr operator*(BaseEquality a, long v) {
    return a.solver().MakeProd(a.Var(), v);
  }
  public static IntExpr operator*(long v, BaseEquality a) {
    return a.solver().MakeProd(a.Var(), v);
  }
  public static IntExpr operator/(BaseEquality a, long v) {
    return a.solver().MakeDiv(a.Var(), v);
  }
  public static IntExpr operator-(BaseEquality a) {
    return a.solver().MakeOpposite(a.Var());
  }
  public IntExpr Abs() {
    return this.solver().MakeAbs(this.Var());
  }
  public IntExpr Square() {
    return this.solver().MakeSquare(this.Var());
  }
  public static WrappedConstraint operator ==(BaseEquality a, long v) {
    return new WrappedConstraint(a.solver().MakeEquality(a.Var(), v));
  }
  public static WrappedConstraint operator ==(long v, BaseEquality a) {
    return new WrappedConstraint(a.solver().MakeEquality(a.Var(), v));
  }
  public static WrappedConstraint operator !=(BaseEquality a, long v) {
    return new WrappedConstraint(a.solver().MakeNonEquality(a.Var(), v));
  }
  public static WrappedConstraint operator !=(long v, BaseEquality a) {
    return new WrappedConstraint(a.solver().MakeNonEquality(a.Var(), v));
  }
  public static WrappedConstraint operator >=(BaseEquality a, long v) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator >=(long v, BaseEquality a) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator >(BaseEquality a, long v) {
    return new WrappedConstraint(a.solver().MakeGreater(a.Var(), v));
  }
  public static WrappedConstraint operator >(long v, BaseEquality a) {
    return new WrappedConstraint(a.solver().MakeLess(a.Var(), v));
  }
  public static WrappedConstraint operator <=(BaseEquality a, long v) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator <=(long v, BaseEquality a) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator <(BaseEquality a, long v) {
    return new WrappedConstraint(a.solver().MakeLess(a.Var(), v));
  }
  public static WrappedConstraint operator <(long v, BaseEquality a) {
    return new WrappedConstraint(a.solver().MakeGreater(a.Var(), v));
  }
  public static WrappedConstraint operator >=(BaseEquality a, BaseEquality b) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a.Var(),
                                 b.Var()));
  }
  public static WrappedConstraint operator >(BaseEquality a, BaseEquality b) {
    return new WrappedConstraint(a.solver().MakeGreater(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator <=(BaseEquality a, BaseEquality b) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator <(BaseEquality a, BaseEquality b) {
    return new WrappedConstraint(a.solver().MakeLess(a.Var(), b.Var()));
  }
  public static ConstraintEquality operator ==(BaseEquality a, BaseEquality b) {
    return new ConstraintEquality(a, b, true);
  }
  public static ConstraintEquality operator !=(BaseEquality a, BaseEquality b) {
    return new ConstraintEquality(a, b, false);
  }
}

public class WrappedConstraint : BaseEquality
{
  public bool Val { get; set; }

  public Constraint Cst { get; set; }

  public WrappedConstraint(Constraint cst) : this(true, cst) {}

  public WrappedConstraint(bool val) : this(val, null) {}

  public WrappedConstraint(bool val, Constraint cst)
  {
    this.Val = val;
    this.Cst = cst;
  }

  public static implicit operator bool(WrappedConstraint valCstPair)
  {
    return valCstPair.Val;
  }

  public static implicit operator Constraint(WrappedConstraint valCstPair)
  {
    return valCstPair.Cst;
  }

  public static implicit operator IntVar(WrappedConstraint eq)
  {
    return eq.Var();
  }

  public static implicit operator IntExpr(WrappedConstraint eq)
  {
    return eq.Var();
  }

  public override Solver solver()
  {
    return this.Cst.solver();
  }

  public override IntVar Var()
  {
    return Cst.Var();
  }
}

public class IntExprEquality : BaseEquality
{
  public IntExprEquality(IntExpr a, IntExpr b, bool equality)
  {
    this.left_ = a;
    this.right_ = b;
    this.equality_ = equality;
  }

  bool IsTrue()
  {
    return (object)left_ == (object)right_ ? equality_ : !equality_;
  }

  Constraint ToConstraint()
  {
    return equality_ ?
        left_.solver().MakeEquality(left_.Var(), right_.Var()) :
        left_.solver().MakeNonEquality(left_.Var(), right_.Var());
  }

  public static bool operator true(IntExprEquality eq)
  {
    return eq.IsTrue();
  }

  public static bool operator false(IntExprEquality eq)
  {
    return !eq.IsTrue();
  }

  public static implicit operator Constraint(IntExprEquality eq)
  {
    return eq.ToConstraint();
  }

  public override IntVar Var()
  {
    return equality_ ?
        left_.solver().MakeIsEqualVar(left_, right_) :
        left_.solver().MakeIsDifferentVar(left_, right_);
  }

  public static implicit operator IntVar(IntExprEquality eq)
  {
    return eq.Var();
  }

  public static implicit operator IntExpr(IntExprEquality eq)
  {
    return eq.Var();
  }

  public override Solver solver()
  {
    return left_.solver();
  }

  private IntExpr left_;
  private IntExpr right_;
  private bool equality_;
}

public class ConstraintEquality : BaseEquality
{
  public ConstraintEquality(IConstraintWithStatus a,
                            IConstraintWithStatus b,
                            bool equality)
  {
    this.left_ = a;
    this.right_ = b;
    this.equality_ = equality;
  }

  bool IsTrue()
  {
    return (object)left_ == (object)right_ ? equality_ : !equality_;
  }

  Constraint ToConstraint()
  {
    return equality_ ?
        left_.solver().MakeEquality(left_.Var(), right_.Var()) :
        left_.solver().MakeNonEquality(left_.Var(), right_.Var());
  }

  public static bool operator true(ConstraintEquality eq)
  {
    return eq.IsTrue();
  }

  public static bool operator false(ConstraintEquality eq)
  {
    return !eq.IsTrue();
  }

  public static implicit operator Constraint(ConstraintEquality eq)
  {
    return eq.ToConstraint();
  }

  public override IntVar Var()
  {
    return equality_ ?
        left_.solver().MakeIsEqualVar(left_.Var(), right_.Var()) :
        left_.solver().MakeIsDifferentVar(left_.Var(), right_.Var());
  }

  public static implicit operator IntVar(ConstraintEquality eq)
  {
    return eq.Var();
  }

  public static implicit operator IntExpr(ConstraintEquality eq)
  {
    return eq.Var();
  }

  public override Solver solver()
  {
    return left_.solver();
  }

  private IConstraintWithStatus left_;
  private IConstraintWithStatus right_;
  private bool equality_;
}
}  // namespace Google.OrTools.ConstraintSolver
