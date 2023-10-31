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

/// <summary>
///  Main modeling class.
/// </summary>
///
/// Proposes a factory to create all modeling objects understood by the ModelSolver.
public class ModelBuilder
{
    /// <summary>
    /// Main constructor.
    /// </summary>
    public ModelBuilder()
    {
        helper_ = new ModelBuilderHelper();
        constantMap_ = new Dictionary<double, int>();
        var_value_map_ = new SortedDictionary<int, double>();
    }

    /// <summary>
    /// Returns a cloned model.
    /// </summary>
    /// <returns>A deep copy of the model.</returns>
    public ModelBuilder Clone()
    {
        ModelBuilder clonedModel = new ModelBuilder();
        clonedModel.Helper.OverwriteModel(Helper);
        foreach (KeyValuePair<double, int> entry in constantMap_)
        {
            clonedModel.constantMap_[entry.Key] = entry.Value;
        }
        return clonedModel;
    }

    // Integer variables.

    /// <summary>
    /// reates a variable with domain [lb, ub].
    /// </summary>
    /// <param name="lb">The lower bound of the variable</param>
    /// <param name="ub">The upper bound of the variable</param>
    /// <param name="isIntegral">Indicates if the variable is restricted to take only integral values</param>
    /// <param name="name">The name of the variable</param>
    /// <returns>The new variable</returns>
    public Variable NewVar(double lb, double ub, bool isIntegral, String name)
    {
        return new Variable(helper_, lb, ub, isIntegral, name);
    }

    /// <summary>
    /// Creates a continuous variable with domain [lb, ub].
    /// </summary>
    /// <param name="lb">The lower bound of the variable</param>
    /// <param name="ub">The upper bound of the variable</param>
    /// <param name="name">The name of the variable</param>
    /// <returns>The new continuous variable</returns>
    public Variable NewNumVar(double lb, double ub, String name)
    {
        return new Variable(helper_, lb, ub, false, name);
    }

    /// <summary>
    /// Creates an integer variable with domain [lb, ub].
    /// </summary>
    /// <param name="lb">The lower bound of the variable</param>
    /// <param name="ub">The upper bound of the variable</param>
    /// <param name="name">The name of the variable</param>
    /// <returns>The new integer variable</returns>
    public Variable NewIntVar(double lb, double ub, String name)
    {
        return new Variable(helper_, lb, ub, true, name);
    }

    /// <summary>
    /// Creates a bool variable with the given name.
    /// </summary>
    /// <param name="name">The name of the variable</param>
    /// <returns>The new Boolean variable</returns>
    public Variable NewBoolVar(String name)
    {
        return new Variable(helper_, 0, 1, true, name);
    }

    /// <summary>
    /// Creates a constant variable.
    /// </summary>
    /// <param name="value">the value of the constant variable</param>
    /// <returns>A new variable with a fixed value</returns>
    public Variable NewConstant(double value)
    {
        if (constantMap_.TryGetValue(value, out int index))
        {
            return new Variable(helper_, index);
        }
        Variable cste = new Variable(helper_, value, value, false, ""); // bounds and name.
        constantMap_.Add(value, cste.Index);
        return cste;
    }

    /// Rebuilds a variable from its index.
    public Variable VarFromIndex(int index)
    {
        return new Variable(helper_, index);
    }

    /// <summary>
    /// Adds a Linear constraint to the model.
    /// </summary>
    /// <param name="lin">A bounded linear expression</param>
    /// <returns>A linear expression</returns>
    /// <exception cref="ArgumentException">Throw when the constraint is not supported by the linear solver</exception>
    public LinearConstraint Add(BoundedLinearExpression lin)
    {
        switch (lin.CtType)
        {
        case BoundedLinearExpression.Type.BoundExpression: {
            return AddLinearConstraint(lin.Left, lin.Lb, lin.Ub);
        }
        case BoundedLinearExpression.Type.VarEqVar: {
            return AddLinearConstraint(lin.Left - lin.Right, 0, 0);
        }
        case BoundedLinearExpression.Type.VarEqCst: {
            return AddLinearConstraint(lin.Left, lin.Lb, lin.Lb);
        }
        default: {
            throw new ArgumentException("Cannot use '" + lin.ToString() + "' as a linear constraint.");
        }
        }
        return null;
    }

    /// <summary>
    /// Adds the constraint expr in [lb, ub].
    /// </summary>
    /// <param name="expr">The constrained expression</param>
    /// <param name="lb">the constrained lower bound of the expression</param>
    /// <param name="ub">the constrained upper bound of the expression</param>
    /// <returns>the linear constraint</returns>
    public LinearConstraint AddLinearConstraint(LinearExpr expr, double lb, double ub)
    {
        var dict = var_value_map_;
        dict.Clear();
        double offset = LinearExpr.GetVarValueMap(expr, dict, terms_);
        LinearConstraint lin = new LinearConstraint(helper_);
        foreach (KeyValuePair<int, double> term in dict)
        {
            helper_.AddConstraintTerm(lin.Index, term.Key, term.Value);
        }
        if (lb == Double.NegativeInfinity || lb == Double.PositiveInfinity)
        {
            lin.LowerBound = lb;
        }
        else
        {
            lin.LowerBound = lb - offset;
        }
        if (ub == Double.NegativeInfinity || ub == Double.PositiveInfinity)
        {
            lin.UpperBound = ub;
        }
        else
        {
            lin.UpperBound = ub - offset;
        }
        return lin;
    }

    /// <summary>
    /// Returns the number of variables in the model.
    /// </summary>
    public int VariablesCount()
    {
        return helper_.VariablesCount();
    }

    /// <summary>
    /// Returns the number of constraints in the model.
    /// </summary>
    public int ConstraintsCount()
    {
        return helper_.ConstraintsCount();
    }

    /// Rebuilds a linear constraint from its index.
    public LinearConstraint ConstraintFromIndex(int index)
    {
        return new LinearConstraint(helper_, index);
    }

    /// <summary>
    /// Minimize expression.
    /// </summary>
    /// <param name="obj">the linear expression to minimize</param>
    public void Minimize(LinearExpr obj)
    {
        Optimize(obj, false);
    }

    /// <summary>
    /// Maximize expression.
    /// </summary>
    /// <param name="obj">the linear expression to maximize</param>
    public void Maximize(LinearExpr obj)
    {
        Optimize(obj, true);
    }

    /// <summary>
    /// Sets the objective expression.
    /// </summary>
    /// <param name="obj">the linear expression to optimize</param>
    /// <param name="maximize">the direction of the optimization</param>
    public void Optimize(LinearExpr obj, bool maximize)
    {
        helper_.ClearObjective();
        double offset = LinearExpr.GetVarValueMap(expr, dict, terms_);
        LinearConstraint lin = new LinearConstraint(helper_);
        foreach (KeyValuePair<int, double> term in dict)
        {
            if (term.Value != 0.0)
            {
                helper_.setVarObjectiveCoefficient(term.Key, term.Value);
            }
        }
        helper_.SetObjectiveOffset(e.getOffset());
        helper_.SetMaximize(maximize);
    }

    /// <summary>
    /// The offset of the objective.
    /// </summary>
    public double ObjectiveOffset
    {
        get {
            return helper_.ObjectiveOffset();
        }
        set {
            helper_.SetObjectiveOffset(value);
        }
    }

    /// <summary>
    /// The name of the model.
    /// </summary>
    public String Name
    {
        get {
            return helper_.Name();
        }
        set {
            helper_.SetName(value);
        }
    }

    /// <summary>
    /// Write the model as a protocol buffer to 'file'.
    /// </summary>
    /// @param file file to write the model to. If the filename ends with 'txt', the model will be
    ///    written as a text file, otherwise, the binary format will be used.
    ///@return true if the model was correctly written.
    public bool ExportToFile(String file)
    {
        return helper_.WriteModelToFile(file);
    }

    public String ExportToMpsString(bool obfuscate)
    {
        return helper_.ExportToMpsString(obfuscate);
    }

    public String ExportToLpString(bool obfuscate)
    {
        return helper_.ExportToLpString(obfuscate);
    }

    public bool ImportFromMpsString(String mpsString)
    {
        return helper_.ImportFromMpsString(mpsString);
    }

    public bool ImportFromMpsFile(String mpsFile)
    {
        return helper_.ImportFromMpsString(mpsFile);
    }

    public bool ImportFromLpString(String lpString)
    {
        return helper_.ImportFromLpString(lpString);
    }

    public bool ImportFromLpFile(String lpFile)
    {
        return helper_.ImportFromMpsString(lpFile);
    }

    /// <summary>
    /// The model builder helper.
    /// </summary>
    public ModelBuilderHelper Helper
    {
        get {
            return helper_;
        }
    }

    private ModelBuilderHelper helper_;
    private Dictionary<double, int> constantMap_;

    // Used to process linear exppressions.
    private SortedDictionary<int, double> var_value_map_;
    private Queue<Term> terms_;
}

} // namespace Google.OrTools.ModelBuilder