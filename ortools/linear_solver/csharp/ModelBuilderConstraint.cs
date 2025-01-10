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

/// <summary>
/// Wrapper around a linear constraint stored in the ModelBuilderHelper instance.
/// </summary>
public class LinearConstraint
{
    public LinearConstraint(ModelBuilderHelper helper)
    {
        helper_ = helper;
        index_ = helper_.AddLinearConstraint();
    }

    public LinearConstraint(ModelBuilderHelper helper, int index)
    {
        helper_ = helper;
        index_ = index;
    }

    /// <summary>
    /// Returns the index of the constraint in the model.
    /// </summary>
    public int Index
    {
        get {
            return index_;
        }
    }

    /// <summary>
    /// Returns the constraint builder.
    /// </summary>
    public ModelBuilderHelper Helper
    {
        get {
            return helper_;
        }
    }

    /// <summary>
    /// The lower bound of the constraint.
    /// </summary>
    public double LowerBound
    {
        get {
            return helper_.ConstraintLowerBound(index_);
        }
        set {
            helper_.SetConstraintLowerBound(index_, value);
        }
    }

    /// <summary>
    /// The upper bound of the constraint.
    /// </summary>
    public double UpperBound
    {
        get {
            return helper_.ConstraintUpperBound(index_);
        }
        set {
            helper_.SetConstraintUpperBound(index_, value);
        }
    }

    /// <summary>
    /// The name of the variable given upon creation.
    /// </summary>
    public String Name
    {
        get {
            return helper_.ConstraintName(index_);
        }
        set {
            helper_.SetConstraintName(index_, value);
        }
    }

    /// <summary>
    /// Adds var * coeff to the constraint.
    /// </summary>
    /// <param name="var">the variable of the term to add</param>
    /// <param name="coeff">the coefficient of the term to add</param>
    public void AddTerm(Variable var, double coeff)
    {
        helper_.SafeAddConstraintTerm(index_, var.Index, coeff);
    }

    /// <summary>
    /// Sets the coefficient of var to coeff, adding or removing a term if needed.
    /// </summary>
    /// <param name="var">the variable of the term to set</param>
    /// <param name="coeff">the coefficient of the term to set</param>
    public void SetVariableCoefficient(Variable var, double coeff)
    {
        helper_.SetConstraintCoefficient(index_, var.Index, coeff);
    }

    /// <summary>
    /// Clear all terms of the constraint.
    /// </summary>
    public void ClearTerms()
    {
        helper_.ClearConstraintTerms(index_);
    }

    /// <summary>
    /// Inline name setter.
    /// </summary>
    /// <param name="name">the new name of the variable</param>
    /// <returns>this</returns>
    public LinearConstraint WithName(String name)
    {
        Name = name;
        return this;
    }

    private readonly int index_;
    private ModelBuilderHelper helper_;
}

/// <summary>
/// Wrapper around an enforced linear constraint stored in the ModelBuilderHelper instance.
/// </summary>
public class EnforcedLinearConstraint
{
    public EnforcedLinearConstraint(ModelBuilderHelper helper)
    {
        helper_ = helper;
        index_ = helper_.AddEnforcedLinearConstraint();
    }

    public EnforcedLinearConstraint(ModelBuilderHelper helper, int index)
    {
        if (!helper.IsEnforcedConstraint(index))
        {
            throw new ArgumentException("the given index does not refer to an enforced linear constraint");
        }
        helper_ = helper;
        index_ = index;
    }

    /// <summary>
    /// Returns the index of the constraint in the model.
    /// </summary>
    public int Index
    {
        get {
            return index_;
        }
    }

    /// <summary>
    /// Returns the constraint builder.
    /// </summary>
    public ModelBuilderHelper Helper
    {
        get {
            return helper_;
        }
    }

    /// <summary>
    /// The lower bound of the constraint.
    /// </summary>
    public double LowerBound
    {
        get {
            return helper_.EnforcedConstraintLowerBound(index_);
        }
        set {
            helper_.SetEnforcedConstraintLowerBound(index_, value);
        }
    }

    /// <summary>
    /// The upper bound of the constraint.
    /// </summary>
    public double UpperBound
    {
        get {
            return helper_.EnforcedConstraintUpperBound(index_);
        }
        set {
            helper_.SetEnforcedConstraintUpperBound(index_, value);
        }
    }

    /// <summary>
    /// The indicator variable of the constraint.
    /// </summary>
    public Variable IndicatorVariable
    {
        get {
            return new Variable(helper_, helper_.EnforcedIndicatorVariableIndex(index_));
        }
        set {
            helper_.SetEnforcedIndicatorVariableIndex(index_, value.Index);
        }
    }

    /// <summary>
    /// The indicator value of the constraint.
    /// </summary>
    public bool IndicatorValue
    {
        get {
            return helper_.EnforcedIndicatorValue(index_);
        }
        set {
            helper_.SetEnforcedIndicatorValue(index_, value);
        }
    }

    /// <summary>
    /// The name of the variable given upon creation.
    /// </summary>
    public String Name
    {
        get {
            return helper_.EnforcedConstraintName(index_);
        }
        set {
            helper_.SetEnforcedConstraintName(index_, value);
        }
    }

    /// <summary>
    /// Adds var * coeff to the constraint.
    /// </summary>
    /// <param name="var">the variable of the term to add</param>
    /// <param name="coeff">the coefficient of the term to add</param>
    public void AddTerm(Variable var, double coeff)
    {
        helper_.SafeAddEnforcedConstraintTerm(index_, var.Index, coeff);
    }

    /// <summary>
    /// Sets the coefficient of var to coeff, adding or removing a term if needed.
    /// </summary>
    /// <param name="var">the variable of the term to set</param>
    /// <param name="coeff">the coefficient of the term to set</param>
    public void SetVariableCoefficient(Variable var, double coeff)
    {
        helper_.SetEnforcedConstraintCoefficient(index_, var.Index, coeff);
    }

    /// <summary>
    /// Clear all terms of the constraint.
    /// </summary>
    public void ClearTerms()
    {
        helper_.ClearEnforcedConstraintTerms(index_);
    }

    /// <summary>
    /// Inline setter
    /// </summary>
    /// <param name="name">The name to assign</param>
    /// <returns>this</returns>
    public EnforcedLinearConstraint WithName(String name)
    {
        Name = name;
        return this;
    }

    private readonly int index_;
    private ModelBuilderHelper helper_;
}

} // namespace Google.OrTools.ModelBuilder