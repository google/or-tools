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
///  Main modeling class.
/// </summary>
///
/// Proposes a factory to create all modeling objects understood by the Solver.
public class Model
{
    /// <summary>
    /// Main constructor.
    /// </summary>
    public Model()
    {
        helper_ = new ModelBuilderHelper();
        constantMap_ = new Dictionary<double, int>();
        tmp_var_value_map_ = new SortedDictionary<int, double>();
        tmp_terms_ = new Queue<Term>();
    }

    /// <summary>
    /// Returns a cloned model.
    /// </summary>
    /// <returns>A deep copy of the model.</returns>
    public Model Clone()
    {
        Model clonedModel = new Model();
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
        LinearConstraint lin = new LinearConstraint(helper_);
        var dict = tmp_var_value_map_;
        dict.Clear();
        double offset = LinearExpr.GetVarValueMap(expr, dict, tmp_terms_);
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

    /// Rebuilds a linear constraint from its index.
    public LinearConstraint ConstraintFromIndex(int index)
    {
        return new LinearConstraint(helper_, index);
    }

    /// <summary>
    /// Adds an enforced Linear constraint to the model.
    /// </summary>
    /// <param name="lin">A bounded linear expression</param>
    /// <param name="iVar">The indicator variable of the constraint.</param>
    /// <param name="iValue">The indicator value of the constraint.</param>
    /// <returns>A linear expression</returns>
    /// <exception cref="ArgumentException">Throw when the constraint is not supported by the linear solver</exception>
    public EnforcedLinearConstraint AddEnforced(BoundedLinearExpression lin, Variable iVar, bool iValue)
    {
        switch (lin.CtType)
        {
        case BoundedLinearExpression.Type.BoundExpression: {
            return AddEnforcedLinearConstraint(lin.Left, lin.Lb, lin.Ub, iVar, iValue);
        }
        case BoundedLinearExpression.Type.VarEqVar: {
            return AddEnforcedLinearConstraint(lin.Left - lin.Right, 0, 0, iVar, iValue);
        }
        case BoundedLinearExpression.Type.VarEqCst: {
            return AddEnforcedLinearConstraint(lin.Left, lin.Lb, lin.Lb, iVar, iValue);
        }
        default: {
            throw new ArgumentException("Cannot use '" + lin.ToString() + "' as a linear constraint.");
        }
        }
    }

    /// <summary>
    /// Adds the constraint iVar == iValue => expr in [lb, ub].
    /// </summary>
    /// <param name="expr">The constrained expression</param>
    /// <param name="lb">the lower bound of the constraint</param>
    /// <param name="ub">the upper bound of the constraint</param>
    /// <param name="iVar">the indicator variable of the constraint</param>
    /// <param name="iValue">the indicator value of the constraint</param>
    /// <returns>the enforced linear constraint</returns>
    public EnforcedLinearConstraint AddEnforcedLinearConstraint(LinearExpr expr, double lb, double ub, Variable iVar,
                                                                bool iValue)
    {
        EnforcedLinearConstraint lin = new EnforcedLinearConstraint(helper_);
        lin.IndicatorVariable = iVar;
        lin.IndicatorValue = iValue;
        var dict = tmp_var_value_map_;
        dict.Clear();
        double offset = LinearExpr.GetVarValueMap(expr, dict, tmp_terms_);
        foreach (KeyValuePair<int, double> term in dict)
        {
            helper_.AddEnforcedConstraintTerm(lin.Index, term.Key, term.Value);
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

    /// Rebuilds a linear constraint from its index.
    public EnforcedLinearConstraint EnforcedConstraintFromIndex(int index)
    {
        return new EnforcedLinearConstraint(helper_, index);
    }

    // Objective.

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
        var dict = tmp_var_value_map_;
        dict.Clear();
        double offset = LinearExpr.GetVarValueMap(obj, dict, tmp_terms_);
        foreach (KeyValuePair<int, double> term in dict)
        {
            if (term.Value != 0.0)
            {
                helper_.SetVarObjectiveCoefficient(term.Key, term.Value);
            }
        }
        helper_.SetObjectiveOffset(offset);
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
    /// Remove all hints from the model.
    /// </summary>
    public void ClearHints()
    {
        helper_.ClearHints();
    }

    /// <summary>
    /// Adds var == value as a hint to the model.  Note that variables must not appear more than once in the list of
    /// hints.
    /// </summary>
    public void AddHint(Variable var, double value)
    {
        helper_.AddHint(var.Index, value);
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
        return helper_.WriteModelToProtoFile(file);
    }

    /// <summary>
    /// load the model as a protocol buffer from 'file'.
    /// </summary>
    /// @param file file to read the model from.
    ///@return true if the model was correctly loaded.
    public bool ImportFromFile(String file)
    {
        return helper_.ReadModelFromProtoFile(file);
    }

    public String ExportToMpsString(bool obfuscate)
    {
        return helper_.ExportToMpsString(obfuscate);
    }

    public String ExportToLpString(bool obfuscate)
    {
        return helper_.ExportToLpString(obfuscate);
    }

    public bool WriteToMpsFile(String filename, bool obfuscate)
    {
        return helper_.WriteToMpsFile(filename, obfuscate);
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

    // Used to process linear expressions.
    private SortedDictionary<int, double> tmp_var_value_map_;
    private Queue<Term> tmp_terms_;
}

} // namespace Google.OrTools.ModelBuilder
