// Copyright 2010-2025 Google LLC
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

namespace Google.OrTools.ModelBuilder
{
using Google.OrTools.Util;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using Google.Protobuf.Collections;

internal static class HelperExtensions
{
    //     [MethodImpl(MethodImplOptions.AggressiveInlining)]
    //     public static void AddOrIncrement(this SortedDictionary<int, double> dict, int key, double increment)
    //     {
    // #if NET6_0_OR_GREATER
    //         System.Runtime.InteropServices.CollectionsMarshal.GetValueRefOrAddDefault(dict, key, out _) += increment;
    // #else
    //         if (dict.TryGetValue(key, out var value))
    //         {
    //             dict[key] = value + increment;
    //         }
    //         else
    //         {
    //             dict.Add(key, increment);
    //         }
    // #endif
    //     }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void TrySetCapacity<TField, TValues>(this RepeatedField<TField> field, IEnumerable<TValues> values)
    {
        if (values is ICollection<TValues> collection)
        {
            field.Capacity = collection.Count;
        }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void TryEnsureCapacity<TValue, TValues>(this List<TValue> list, IEnumerable<TValues> values)
    {
        // Check for ICollection as the generic version is not covariant and TValues could be LinearExpr, Variable, ...
        if (values is ICollection collection)
        {
            list.Capacity = Math.Max(list.Count + collection.Count, list.Capacity);
        }
    }

#if NETFRAMEWORK
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static bool TryDequeue<T>(this Queue<T> queue, out T value)
    {
        if (queue.Count > 0)
        {
            value = queue.Dequeue();
            return true;
        }

        value = default;
        return false;
    }
#endif
}

// Holds a term (expression * coefficient)
public struct Term
{
    public LinearExpr expr;
    public double coefficient;

    public Term(LinearExpr e, double c)
    {
        this.expr = e;
        this.coefficient = c;
    }
}

/**
 * <summary>
 * Holds a linear expression: <c>sum (ai * xi) + b</c>.
 * </summary>
 */
public class LinearExpr
{
    /** <summary> Creates <c>Sum(exprs)</c>.</summary> */
    public static LinearExpr Sum(IEnumerable<LinearExpr> exprs)
    {
        return NewBuilder(0).AddSum(exprs);
    }

    /** <summary> Creates <c>Sum(exprs[i] * coeffs[i])</c>.</summary> */
    public static LinearExpr WeightedSum(IEnumerable<LinearExpr> exprs, IEnumerable<int> coeffs)
    {
        return NewBuilder(0).AddWeightedSum(exprs, coeffs);
    }

    /** <summary> Creates <c>Sum(exprs[i] * coeffs[i])</c>.</summary> */
    public static LinearExpr WeightedSum(IEnumerable<LinearExpr> exprs, IEnumerable<double> coeffs)
    {
        return NewBuilder(0).AddWeightedSum(exprs, coeffs);
    }

    /** <summary> Creates <c>expr * coeff</c>.</summary> */
    public static LinearExpr Term(LinearExpr expr, double coeff)
    {
        return Prod(expr, coeff);
    }

    /** <summary> Creates <c>expr * coeff + offset</c>.</summary> */
    public static LinearExpr Affine(LinearExpr expr, double coeff, double offset)
    {
        if (offset == 0)
        {
            return Prod(expr, coeff);
        }
        return NewBuilder().AddTerm(expr, coeff).Add(offset);
    }

    /** <summary> Creates a constant expression.</summary> */
    public static LinearExpr Constant(double value)
    {
        return NewBuilder(0).Add(value);
    }

    /** <summary> Creates a builder class for linear expression.</summary> */
    public static LinearExprBuilder NewBuilder(int sizeHint = 2)
    {
        return new LinearExprBuilder(sizeHint);
    }

    public static LinearExpr operator +(LinearExpr a, LinearExpr b)
    {
        return NewBuilder().Add(a).Add(b);
    }

    public static LinearExpr operator +(LinearExpr a, double v)
    {
        return NewBuilder().Add(a).Add(v);
    }

    public static LinearExpr operator +(double v, LinearExpr a)
    {
        return NewBuilder().Add(a).Add(v);
    }

    public static LinearExpr operator -(LinearExpr a, LinearExpr b)
    {
        return NewBuilder().Add(a).AddTerm(b, -1);
    }

    public static LinearExpr operator -(LinearExpr a, double v)
    {
        return NewBuilder().Add(a).Add(-v);
    }

    public static LinearExpr operator -(double v, LinearExpr a)
    {
        return NewBuilder().AddTerm(a, -1).Add(v);
    }

    public static LinearExpr operator *(LinearExpr a, double v)
    {
        return Prod(a, v);
    }

    public static LinearExpr operator *(double v, LinearExpr a)
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

    public static BoundedLinearExpression operator ==(LinearExpr a, double v)
    {
        return new BoundedLinearExpression(a, v, true);
    }

    public static BoundedLinearExpression operator !=(LinearExpr a, double v)
    {
        return new BoundedLinearExpression(a, v, false);
    }

    public static BoundedLinearExpression operator >=(LinearExpr a, double v)
    {
        return new BoundedLinearExpression(v, a, Double.PositiveInfinity);
    }

    public static BoundedLinearExpression operator >=(double v, LinearExpr a)
    {
        return a <= v;
    }

    public static BoundedLinearExpression operator <=(LinearExpr a, double v)
    {
        return new BoundedLinearExpression(Double.NegativeInfinity, a, v);
    }

    public static BoundedLinearExpression operator <=(double v, LinearExpr a)
    {
        return a >= v;
    }

    public static BoundedLinearExpression operator >=(LinearExpr a, LinearExpr b)
    {
        return new BoundedLinearExpression(0, a - b, Double.PositiveInfinity);
    }

    public static BoundedLinearExpression operator <=(LinearExpr a, LinearExpr b)
    {
        return new BoundedLinearExpression(Double.NegativeInfinity, a - b, 0);
    }

    internal static LinearExpr Prod(LinearExpr e, double v)
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

    internal static double GetVarValueMap(LinearExpr e, SortedDictionary<int, double> dict, Queue<Term> terms)
    {
        double constant = 0;
        double coefficient = 1;
        LinearExpr expr = e;
        terms.Clear();

        do
        {
            switch (expr)
            {
            case LinearExprBuilder builder:
                constant += coefficient * builder.Offset;
                if (coefficient == 1)
                {
                    foreach (Term sub in builder.Terms)
                    {
                        terms.Enqueue(sub);
                    }
                }
                else
                {
                    foreach (Term sub in builder.Terms)
                    {
                        terms.Enqueue(new Term(sub.expr, sub.coefficient * coefficient));
                    }
                }
                break;
            case Variable var:
                if (dict.TryGetValue(var.Index, out var c))
                {
                    dict[var.Index] = c + coefficient;
                }
                else
                {
                    dict.Add(var.Index, coefficient);
                }
                break;
            default:
                throw new ArgumentException("Cannot evaluate '" + expr + "' in an expression");
            }

            if (!terms.TryDequeue(out var term))
            {
                break;
            }
            expr = term.expr;
            coefficient = term.coefficient;
        } while (true);

        return constant;
    }
}

/** <summary> A builder class for linear expressions.</summary> */
public sealed class LinearExprBuilder : LinearExpr
{
    public LinearExprBuilder(int sizeHint = 2)
    {
        terms_ = new List<Term>(sizeHint);
        offset_ = 0;
    }

    /** <summary> Adds <c>expr</c> to the builder.</summary> */
    public LinearExprBuilder Add(LinearExpr expr)
    {
        return AddTerm(expr, 1);
    }

    /** <summary> Adds <c>constant</c> to the builder.</summary> */
    public LinearExprBuilder Add(double constant)
    {
        offset_ += constant;
        return this;
    }

    /** <summary> Adds <c>expr * coefficient</c> to the builder.</summary> */
    public LinearExprBuilder AddTerm(LinearExpr expr, double coefficient)
    {
        terms_.Add(new Term(expr, coefficient));
        return this;
    }

    /** <summary> Adds <c>sum(exprs)</c> to the builder.</summary> */
    public LinearExprBuilder AddSum(IEnumerable<LinearExpr> exprs)
    {
        terms_.TryEnsureCapacity(exprs);
        foreach (LinearExpr expr in exprs)
        {
            AddTerm(expr, 1);
        }
        return this;
    }

    /** <summary> Adds <c>sum(exprs[i] * coeffs[i])</c> to the builder.</summary> */
    public LinearExprBuilder AddWeightedSum(IEnumerable<LinearExpr> exprs, IEnumerable<double> coefficients)
    {
        terms_.TryEnsureCapacity(exprs);
        foreach (var p in exprs.Zip(coefficients, (e, c) => new { Expr = e, Coeff = c }))
        {
            AddTerm(p.Expr, p.Coeff);
        }
        return this;
    }

    /** <summary> Adds <c>sum(exprs[i] * coeffs[i])</c> to the builder.</summary> */
    public LinearExprBuilder AddWeightedSum(IEnumerable<LinearExpr> exprs, IEnumerable<long> coefficients)
    {
        terms_.TryEnsureCapacity(exprs);
        foreach (var p in exprs.Zip(coefficients, (e, c) => new { Expr = e, Coeff = c }))
        {
            AddTerm(p.Expr, p.Coeff);
        }
        return this;
    }

    /** <summary> Adds <c>sum(exprs[i] * coeffs[i])</c> to the builder.</summary> */
    public LinearExprBuilder AddWeightedSum(IEnumerable<LinearExpr> exprs, IEnumerable<int> coefficients)
    {
        terms_.TryEnsureCapacity(exprs);
        foreach (var p in exprs.Zip(coefficients, (e, c) => new { Expr = e, Coeff = c }))
        {
            AddTerm(p.Expr, p.Coeff);
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

    public double Offset
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

    private double offset_;
    private List<Term> terms_;
}

/**
 * <summary>
 * Holds a variable.
 * </summary>
 * <remarks>
 * This class must be constructed from the CpModel class.
 * </remarks>
 */
public class Variable : LinearExpr
{
    public Variable(ModelBuilderHelper helper, double lb, double ub, bool isIntegral, string name)
    {
        helper_ = helper;
        index_ = helper_.AddVar();
        helper_.SetVarLowerBound(index_, lb);
        helper_.SetVarUpperBound(index_, ub);
        helper_.SetVarIntegrality(index_, isIntegral);
        if (!string.IsNullOrEmpty(name))
        {
            helper_.SetVarName(index_, name);
        }
    }

    public Variable(ModelBuilderHelper helper, int index)
    {
        helper_ = helper;
        index_ = index;
    }

    /** Returns the index of the variable in the underlying ModelBuilderHelper. */
    public int Index
    {
        get {
            return index_;
        }
    }

    /** The underlying VariableProto. */
    public ModelBuilderHelper Helper
    {
        get {
            return helper_;
        }
    }

    /** Returns the domain of the variable. */
    public double LowerBound
    {
        get {
            return helper_.VarLowerBound(index_);
        }
        set {
            helper_.SetVarLowerBound(index_, value);
        }
    }

    public double UpperBound
    {
        get {
            return helper_.VarUpperBound(index_);
        }
        set {
            helper_.SetVarUpperBound(index_, value);
        }
    }

    public override string ToString()
    {
        string name = helper_.VarName(index_);
        return string.IsNullOrEmpty(name) ? String.Format("var_{0}", index_) : name;
    }

    /** Returns the name of the variable given upon creation. */
    public string Name
    {
        get {
            return helper_.VarName(index_);
        }
    }

    protected readonly int index_;
    protected ModelBuilderHelper helper_;
}

/**
 * <summary>
 * Holds a linear constraint: <c> expression ∈ domain</c>
 * </summary>
 * <remarks>
 * This class must be constructed from the CpModel class or from the comparison operators.
 * </remarks>
 */
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

    public BoundedLinearExpression(double lb, LinearExpr expr, double ub)
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

    public BoundedLinearExpression(LinearExpr left, double v, bool equality)
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

    public static bool operator true(BoundedLinearExpression ble)
    {
        return ble.IsTrue();
    }

    public static bool operator false(BoundedLinearExpression ble)
    {
        return !ble.IsTrue();
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

    public static BoundedLinearExpression operator <=(BoundedLinearExpression a, double v)
    {
        if (a.CtType != Type.BoundExpression || a.Ub != Double.PositiveInfinity)
        {
            throw new ArgumentException("Operator <= not supported for this BoundedLinearExpression");
        }
        return new BoundedLinearExpression(a.Lb, a.Left, v);
    }

    public static BoundedLinearExpression operator >=(BoundedLinearExpression a, double v)
    {
        if (a.CtType != Type.BoundExpression || a.Lb != Double.NegativeInfinity)
        {
            throw new ArgumentException("Operator >= not supported for this BoundedLinearExpression");
        }
        return new BoundedLinearExpression(v, a.Left, a.Ub);
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

    public double Lb
    {
        get {
            return lb_;
        }
    }

    public double Ub
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
    private double lb_;
    private double ub_;
    private Type type_;
}

} // namespace Google.OrTools.ModelBuilder
