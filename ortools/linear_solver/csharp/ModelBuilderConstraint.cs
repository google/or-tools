// Copyright 2010-2022 Google LLC
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

/** Wrapper around a linear constraint stored in the ModelBuilderHelper instance. */
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

    /** Returns the index of the constraint in the model. */
    public int Index
    {
        get {
            return index_;
        }
    }

    /** Returns the constraint builder. */
    public ModelBuilderHelper Helper
    {
        get {
            return helper_;
        }
    }

    /** The lower bound of the constraint. */
    public double LowerBound
    {
        get {
            return helper_.ConstraintLowerBound(index_);
        }
        set {
            helper_.SetConstraintLowerBound(index_, value);
        }
    }

    /** The upper bound of the constraint. */
    public double UpperBound
    {
        get {
            return helper_.ConstraintUpperBound(index_);
        }
        set {
            helper_.SetConstraintUpperBound(index_, value);
        }
    }

    /** The name of the variable given upon creation. */
    public String Name
    {
        get {
            return helper_.ConstraintName(index_);
        }
        set {
            helper_.SetConstraintName(index_, value);
        }
    }

    // Adds var * coeff to the constraint.
    public void AddTerm(Variable var, double coeff)
    {
        helper_.AddConstraintTerm(index_, var.Index, coeff);
    }

    /** Inline setter */
    public LinearConstraint WithName(String name)
    {
        Name = name;
        return this;
    }

    private readonly int index_;
    private ModelBuilderHelper helper_;
}

} // namespace Google.OrTools.ModelBuilder