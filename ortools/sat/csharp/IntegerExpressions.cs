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
using Google.OrTools.Util;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using Google.Protobuf.Collections;

public interface ILiteral
{
    ILiteral Not();
    int GetIndex();
    LinearExpr NotAsExpr();
}

internal static class HelperExtensions
{
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void AddOrIncrement(this Dictionary<IntVar, long> dict, IntVar key, long increment)
    {
#if NET6_0_OR_GREATER
        System.Runtime.InteropServices.CollectionsMarshal.GetValueRefOrAddDefault(dict, key, out _) += increment;
#else
        if (dict.TryGetValue(key, out var value))
        {
            dict[key] = value + increment;
        }
        else
        {
            dict.Add(key, increment);
        }
#endif
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void TrySetCapacity<TField, TValues>(this RepeatedField<TField> field, IEnumerable<TValues> values)
    {
        if (values is ICollection<TValues> collection)
        {
            field.Capacity = collection.Count;
        }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void TryEnsureCapacity<TValue, TValues>(this List<TValue> list, IEnumerable<TValues> values)
    {
        // Check for ICollection as the generic version is not covariant and TValues could be LinearExpr, IntVar, ...
        if (values is ICollection collection)
        {
            list.Capacity = Math.Max(list.Count + collection.Count, list.Capacity);
        }
    }
}

// Holds a term (expression * coefficient)
public struct Term
{
    public LinearExpr expr;
    public long coefficient;

    public Term(LinearExpr e, long c)
    {
        this.expr = e;
        this.coefficient = c;
    }
}

// Holds a linear expression.
public class LinearExpr
{
    public static LinearExpr Sum(IEnumerable<LinearExpr> exprs)
    {
        return NewBuilder(0).AddSum(exprs);
    }

    public static LinearExpr Sum(IEnumerable<ILiteral> literals)
    {
        return NewBuilder(0).AddSum(literals);
    }

    public static LinearExpr Sum(IEnumerable<BoolVar> vars)
    {
        return NewBuilder(0).AddSum(vars);
    }

    public static LinearExpr WeightedSum(IEnumerable<LinearExpr> exprs, IEnumerable<int> coeffs)
    {
        return NewBuilder(0).AddWeightedSum(exprs, coeffs);
    }

    public static LinearExpr WeightedSum(IEnumerable<LinearExpr> exprs, IEnumerable<long> coeffs)
    {
        return NewBuilder(0).AddWeightedSum(exprs, coeffs);
    }

    public static LinearExpr WeightedSum(IEnumerable<ILiteral> literals, IEnumerable<int> coeffs)
    {
        return NewBuilder(0).AddWeightedSum(literals, coeffs);
    }

    public static LinearExpr WeightedSum(IEnumerable<ILiteral> literals, IEnumerable<long> coeffs)
    {
        return NewBuilder(0).AddWeightedSum(literals, coeffs);
    }

    public static LinearExpr WeightedSum(IEnumerable<BoolVar> vars, IEnumerable<int> coeffs)
    {
        return NewBuilder(0).AddWeightedSum(vars, coeffs);
    }

    public static LinearExpr WeightedSum(IEnumerable<BoolVar> vars, IEnumerable<long> coeffs)
    {
        return NewBuilder(0).AddWeightedSum(vars, coeffs);
    }

    public static LinearExpr Term(LinearExpr expr, long coeff)
    {
        return Prod(expr, coeff);
    }

    public static LinearExpr Term(ILiteral literal, long coeff)
    {
        if (literal is BoolVar boolVar)
        {
            return Prod(boolVar, coeff);
        }
        else
        {
            return Affine(literal.NotAsExpr(), -coeff, coeff);
        }
    }

    public static LinearExpr Term(BoolVar var, long coeff)
    {
        return Prod(var, coeff);
    }

    public static LinearExpr Affine(LinearExpr expr, long coeff, long offset)
    {
        if (offset == 0)
        {
            return Prod(expr, coeff);
        }
        return NewBuilder().AddTerm(expr, coeff).Add(offset);
    }

    public static LinearExpr Affine(ILiteral literal, long coeff, long offset)
    {
        return NewBuilder().AddTerm(literal, coeff).Add(offset);
    }

    public static LinearExpr Affine(BoolVar var, long coeff, long offset)
    {
        return NewBuilder().AddTerm(var, coeff).Add(offset);
    }

    public static LinearExpr Constant(long value)
    {
        return NewBuilder(0).Add(value);
    }

    public static LinearExprBuilder NewBuilder(int sizeHint = 2)
    {
        return new LinearExprBuilder(sizeHint);
    }

    public static LinearExpr operator +(LinearExpr a, LinearExpr b)
    {
        return NewBuilder().Add(a).Add(b);
    }

    public static LinearExpr operator +(LinearExpr a, long v)
    {
        return NewBuilder().Add(a).Add(v);
    }

    public static LinearExpr operator +(long v, LinearExpr a)
    {
        return NewBuilder().Add(a).Add(v);
    }

    public static LinearExpr operator -(LinearExpr a, LinearExpr b)
    {
        return NewBuilder().Add(a).AddTerm(b, -1);
    }

    public static LinearExpr operator -(LinearExpr a, long v)
    {
        return NewBuilder().Add(a).Add(-v);
    }

    public static LinearExpr operator -(long v, LinearExpr a)
    {
        return NewBuilder().AddTerm(a, -1).Add(v);
    }

    public static LinearExpr operator *(LinearExpr a, long v)
    {
        return Prod(a, v);
    }

    public static LinearExpr operator *(long v, LinearExpr a)
    {
        return Prod(a, v);
    }

    public static LinearExpr operator -(LinearExpr a)
    {
        return Prod(a, -1);
    }

    public static BoundedLinearExpression operator ==(LinearExpr a, LinearExpr b)
    {
        return new BoundedLinearExpression(a, b, true);
    }

    public static BoundedLinearExpression operator !=(LinearExpr a, LinearExpr b)
    {
        return new BoundedLinearExpression(a, b, false);
    }

    public static BoundedLinearExpression operator ==(LinearExpr a, long v)
    {
        return new BoundedLinearExpression(a, v, true);
    }

    public static BoundedLinearExpression operator !=(LinearExpr a, long v)
    {
        return new BoundedLinearExpression(a, v, false);
    }

    public static BoundedLinearExpression operator >=(LinearExpr a, long v)
    {
        return new BoundedLinearExpression(v, a, Int64.MaxValue);
    }

    public static BoundedLinearExpression operator >=(long v, LinearExpr a)
    {
        return a <= v;
    }

    public static BoundedLinearExpression operator>(LinearExpr a, long v)
    {
        return new BoundedLinearExpression(v + 1, a, Int64.MaxValue);
    }

    public static BoundedLinearExpression operator>(long v, LinearExpr a)
    {
        return a < v;
    }

    public static BoundedLinearExpression operator <=(LinearExpr a, long v)
    {
        return new BoundedLinearExpression(Int64.MinValue, a, v);
    }

    public static BoundedLinearExpression operator <=(long v, LinearExpr a)
    {
        return a >= v;
    }

    public static BoundedLinearExpression operator<(LinearExpr a, long v)
    {
        return new BoundedLinearExpression(Int64.MinValue, a, v - 1);
    }

    public static BoundedLinearExpression operator<(long v, LinearExpr a)
    {
        return a > v;
    }

    public static BoundedLinearExpression operator >=(LinearExpr a, LinearExpr b)
    {
        return new BoundedLinearExpression(0, a - b, Int64.MaxValue);
    }

    public static BoundedLinearExpression operator>(LinearExpr a, LinearExpr b)
    {
        return new BoundedLinearExpression(1, a - b, Int64.MaxValue);
    }

    public static BoundedLinearExpression operator <=(LinearExpr a, LinearExpr b)
    {
        return new BoundedLinearExpression(Int64.MinValue, a - b, 0);
    }

    public static BoundedLinearExpression operator<(LinearExpr a, LinearExpr b)
    {
        return new BoundedLinearExpression(Int64.MinValue, a - b, -1);
    }

    public static LinearExpr Prod(LinearExpr e, long v)
    {
        if (v == 0)
        {
            return NewBuilder(0);
        }
        else if (v == 1)
        {
            return e;
        }
        else
        {
            return NewBuilder(1).AddTerm(e, v);
        }
    }

    public static long GetVarValueMap(LinearExpr e, long initial_coeff, Dictionary<IntVar, long> dict)
    {
        List<Term> terms = new List<Term>();
        if (e is not null)
        {
            terms.Add(new Term(e, initial_coeff));
        }
        long constant = 0;

        while (terms.Count > 0)
        {
            Term term = terms[0];
            terms.RemoveAt(0);
            if (term.coefficient == 0 || term.expr is null)
            {
                continue;
            }

            if (term.expr is LinearExprBuilder b)
            {
                constant += term.coefficient * b.Offset;
                foreach (Term sub in b.Terms)
                {
                    if (sub.expr is IntVar i) // Quick unroll.
                    {
                        dict.AddOrIncrement(i, term.coefficient * sub.coefficient);
                    }
                    else
                    {
                        terms.Add(new Term(sub.expr, sub.coefficient * term.coefficient));
                    }
                }
            }
            else if (term.expr is IntVar i)
            {
                dict.AddOrIncrement(i, term.coefficient);
            }
            else if (term.expr is NotBoolVar notBoolVar)
            {
                dict.AddOrIncrement((IntVar)notBoolVar.Not(), -term.coefficient);
                constant += term.coefficient;
            }
            else
            {
                throw new ArgumentException("Cannot interpret '" + term.expr.ToString() + "' in an integer expression");
            }
        }
        return constant;
    }

    public static LinearExpr RebuildLinearExprFromLinearExpressionProto(LinearExpressionProto proto, CpModelProto model)
    {
        int numElements = proto.Vars.Count;
        long offset = proto.Offset;
        if (numElements == 0)
        {
            return LinearExpr.Constant(offset);
        }
        else if (numElements == 1)
        {
            IntVar var = new IntVar(model, proto.Vars[0]);
            long coeff = proto.Coeffs[0];
            return LinearExpr.Affine(var, coeff, offset);
        }
        else
        {
            LinearExprBuilder builder = LinearExpr.NewBuilder(numElements);
            for (int i = 0; i < numElements; ++i)
            {
                builder.AddTerm(new IntVar(model, proto.Vars[i]), proto.Coeffs[i]);
            }
            builder.Add(offset);
            return builder;
        }
    }
}

public sealed class LinearExprBuilder : LinearExpr
{
    public LinearExprBuilder(int sizeHint = 2)
    {
        terms_ = new List<Term>(sizeHint);
        offset_ = 0;
    }

    public LinearExprBuilder Add(LinearExpr expr)
    {
        return AddTerm(expr, 1);
    }

    public LinearExprBuilder Add(ILiteral literal)
    {
        return AddTerm(literal, 1);
    }

    public LinearExprBuilder Add(BoolVar var)
    {
        return AddTerm(var, 1);
    }

    public LinearExprBuilder Add(long constant)
    {
        offset_ += constant;
        return this;
    }

    public LinearExprBuilder AddTerm(LinearExpr expr, long coefficient)
    {
        terms_.Add(new Term(expr, coefficient));
        return this;
    }

    public LinearExprBuilder AddTerm(ILiteral literal, long coefficient)
    {
        if (literal is BoolVar boolVar)
        {
            terms_.Add(new Term(boolVar, coefficient));
        }
        else
        {
            offset_ += coefficient;
            terms_.Add(new Term(literal.NotAsExpr(), -coefficient));
        }
        return this;
    }

    public LinearExprBuilder AddTerm(BoolVar var, long coefficient)
    {
        terms_.Add(new Term(var, coefficient));
        return this;
    }

    public LinearExprBuilder AddSum(IEnumerable<LinearExpr> exprs)
    {
        terms_.TryEnsureCapacity(exprs);
        foreach (LinearExpr expr in exprs)
        {
            AddTerm(expr, 1);
        }
        return this;
    }

    public LinearExprBuilder AddSum(IEnumerable<ILiteral> literals)
    {
        terms_.TryEnsureCapacity(literals);
        foreach (ILiteral literal in literals)
        {
            AddTerm(literal, 1);
        }
        return this;
    }

    public LinearExprBuilder AddSum(IEnumerable<BoolVar> vars)
    {
        terms_.TryEnsureCapacity(vars);
        foreach (BoolVar var in vars)
        {
            AddTerm(var, 1);
        }
        return this;
    }

    public LinearExprBuilder AddWeightedSum(IEnumerable<LinearExpr> exprs, IEnumerable<long> coefficients)
    {
        terms_.TryEnsureCapacity(exprs);
        foreach (var p in exprs.Zip(coefficients, (e, c) => new { Expr = e, Coeff = c }))
        {
            AddTerm(p.Expr, p.Coeff);
        }
        return this;
    }

    public LinearExprBuilder AddWeightedSum(IEnumerable<LinearExpr> exprs, IEnumerable<int> coefficients)
    {
        terms_.TryEnsureCapacity(exprs);
        foreach (var p in exprs.Zip(coefficients, (e, c) => new { Expr = e, Coeff = c }))
        {
            AddTerm(p.Expr, p.Coeff);
        }
        return this;
    }

    public LinearExprBuilder AddWeightedSum(IEnumerable<ILiteral> literals, IEnumerable<int> coefficients)
    {
        terms_.TryEnsureCapacity(literals);
        foreach (var p in literals.Zip(coefficients, (l, c) => new { Literal = l, Coeff = c }))
        {
            AddTerm(p.Literal, p.Coeff);
        }
        return this;
    }

    public LinearExprBuilder AddWeightedSum(IEnumerable<ILiteral> literals, IEnumerable<long> coefficients)
    {
        terms_.TryEnsureCapacity(literals);
        foreach (var p in literals.Zip(coefficients, (l, c) => new { Literal = l, Coeff = c }))
        {
            AddTerm(p.Literal, p.Coeff);
        }
        return this;
    }

    public LinearExprBuilder AddWeightedSum(IEnumerable<BoolVar> vars, IEnumerable<long> coefficients)
    {
        terms_.TryEnsureCapacity(vars);
        foreach (var p in vars.Zip(coefficients, (v, c) => new { Var = v, Coeff = c }))
        {
            AddTerm(p.Var, p.Coeff);
        }
        return this;
    }

    public LinearExprBuilder AddWeightedSum(IEnumerable<BoolVar> vars, IEnumerable<int> coefficients)
    {
        terms_.TryEnsureCapacity(vars);
        foreach (var p in vars.Zip(coefficients, (v, c) => new { Var = v, Coeff = c }))
        {
            AddTerm(p.Var, p.Coeff);
        }
        return this;
    }

    public override string ToString()
    {
        string result = "";
        foreach (Term term in terms_)
        {
            bool first = String.IsNullOrEmpty(result);
            if (term.expr is null || term.coefficient == 0)
            {
                continue;
            }
            if (term.coefficient == 1)
            {
                if (!first)
                {
                    result += " + ";
                }

                result += term.expr.ToString();
            }
            else if (term.coefficient > 0)
            {
                if (!first)
                {
                    result += " + ";
                }

                result += String.Format("{0} * {1}", term.coefficient, term.expr.ToString());
            }
            else if (term.coefficient == -1)
            {
                if (!first)
                {
                    result += String.Format(" - {0}", term.expr.ToString());
                }
                else
                {
                    result += String.Format("-{0}", term.expr.ToString());
                }
            }
            else
            {
                if (!first)
                {
                    result += String.Format(" - {0} * {1}", -term.coefficient, term.expr.ToString());
                }
                else
                {
                    result += String.Format("{0} * {1}", term.coefficient, term.expr.ToString());
                }
            }
        }
        if (offset_ > 0)
        {
            if (!String.IsNullOrEmpty(result))
            {
                result += String.Format(" + {0}", offset_);
            }
            else
            {
                result += String.Format("{0}", offset_);
            }
        }
        else if (offset_ < 0)
        {
            if (!String.IsNullOrEmpty(result))
            {
                result += String.Format(" - {0}", -offset_);
            }
            else
            {
                result += String.Format("{0}", offset_);
            }
        }
        return result;
    }

    public long Offset
    {
        get {
            return offset_;
        }
    }

    public List<Term> Terms
    {
        get {
            return terms_;
        }
    }

    private long offset_;
    private List<Term> terms_;
}

public class IntVar : LinearExpr
{
    public IntVar(CpModelProto model, Domain domain, string name)
    {
        index_ = model.Variables.Count;
        var_ = new IntegerVariableProto();
        var_.Name = name;
        var_.Domain.Add(domain.FlattenedIntervals());
        model.Variables.Add(var_);
    }

    public IntVar(CpModelProto model, long lb, long ub, string name)
    {
        index_ = model.Variables.Count;
        var_ = new IntegerVariableProto();
        var_.Name = name;
        var_.Domain.Capacity = 2;
        var_.Domain.Add(lb);
        var_.Domain.Add(ub);
        model.Variables.Add(var_);
    }

    public IntVar(CpModelProto model, int index)
    {
        index_ = index;
        var_ = model.Variables[index];
    }

    public int GetIndex()
    {
        return index_;
    }

    public int Index
    {
        get {
            return GetIndex();
        }
    }

    public IntegerVariableProto Proto
    {
        get {
            return var_;
        }
        set {
            var_ = value;
        }
    }

    public Domain Domain
    {
        get {
            return CpSatHelper.VariableDomain(var_);
        }
    }

    public override string ToString()
    {
        return var_.Name ?? var_.ToString();
    }

    public string Name()
    {
        return var_.Name;
    }

    protected readonly int index_;
    protected IntegerVariableProto var_;
}

public sealed class BoolVar : IntVar, ILiteral
{

    public BoolVar(CpModelProto model, String name) : base(model, 0, 1, name)
    {
    }

    public BoolVar(CpModelProto model, int index) : base(model, index)
    {
    }

    public ILiteral Not()
    {
        return negation_ ??= new NotBoolVar(this);
    }

    public LinearExpr NotAsExpr()
    {
        return (LinearExpr)Not();
    }

    private NotBoolVar negation_;
}

public sealed class NotBoolVar : LinearExpr, ILiteral
{
    public NotBoolVar(BoolVar boolvar)
    {
        boolvar_ = boolvar;
    }

    public int GetIndex()
    {
        return -boolvar_.GetIndex() - 1;
    }

    public int Index
    {
        get {
            return GetIndex();
        }
    }

    public ILiteral Not()
    {
        return boolvar_;
    }

    public LinearExpr NotAsExpr()
    {
        return (LinearExpr)Not();
    }

    public override string ToString()
    {
        return String.Format("Not({0})", boolvar_.ToString());
    }

    private readonly BoolVar boolvar_;
}

public sealed class BoundedLinearExpression
{
    public enum Type
    {
        BoundExpression,
        VarEqVar,
        VarDiffVar,
        VarEqCst,
        VarDiffCst,
    }

    public BoundedLinearExpression(long lb, LinearExpr expr, long ub)
    {
        left_ = expr;
        right_ = null;
        lb_ = lb;
        ub_ = ub;
        type_ = Type.BoundExpression;
    }

    public BoundedLinearExpression(LinearExpr left, LinearExpr right, bool equality)
    {
        left_ = left;
        right_ = right;
        lb_ = 0;
        ub_ = 0;
        type_ = equality ? Type.VarEqVar : Type.VarDiffVar;
    }

    public BoundedLinearExpression(LinearExpr left, long v, bool equality)
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

    public static BoundedLinearExpression operator <=(BoundedLinearExpression a, long v)
    {
        if (a.CtType != Type.BoundExpression || a.Ub != Int64.MaxValue)
        {
            throw new ArgumentException("Operator <= not supported for this BoundedLinearExpression");
        }
        return new BoundedLinearExpression(a.Lb, a.Left, v);
    }

    public static BoundedLinearExpression operator<(BoundedLinearExpression a, long v)
    {
        if (a.CtType != Type.BoundExpression || a.Ub != Int64.MaxValue)
        {
            throw new ArgumentException("Operator < not supported for this BoundedLinearExpression");
        }
        return new BoundedLinearExpression(a.Lb, a.Left, v - 1);
    }

    public static BoundedLinearExpression operator >=(BoundedLinearExpression a, long v)
    {
        if (a.CtType != Type.BoundExpression || a.Lb != Int64.MinValue)
        {
            throw new ArgumentException("Operator >= not supported for this BoundedLinearExpression");
        }
        return new BoundedLinearExpression(v, a.Left, a.Ub);
    }

    public static BoundedLinearExpression operator>(BoundedLinearExpression a, long v)
    {
        if (a.CtType != Type.BoundExpression || a.Lb != Int64.MinValue)
        {
            throw new ArgumentException("Operator < not supported for this BoundedLinearExpression");
        }
        return new BoundedLinearExpression(v + 1, a.Left, a.Ub);
    }

    public LinearExpr Left
    {
        get {
            return left_;
        }
    }

    public LinearExpr Right
    {
        get {
            return right_;
        }
    }

    public long Lb
    {
        get {
            return lb_;
        }
    }

    public long Ub
    {
        get {
            return ub_;
        }
    }

    public Type CtType
    {
        get {
            return type_;
        }
    }

    private LinearExpr left_;
    private LinearExpr right_;
    private long lb_;
    private long ub_;
    private Type type_;
}

} // namespace Google.OrTools.Sat
