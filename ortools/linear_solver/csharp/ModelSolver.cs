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
/// Model solver class
/// </summary>
public class Solver
{
    class SolverException : Exception
    {
        public SolverException(String methodName, String msg) : base(methodName + ": " + msg)
        {
        }
    }

    /// <summary>
    /// Creates the solver with the supplied solver backend.
    /// </summary>
    /// <param name="solverName">the name of the solver backend</param>
    public Solver(String solverName)
    {
        this.helper_ = new ModelSolverHelper(solverName);
        this.logCallback_ = null;
    }

    /// <summary>
    /// Solves given model, and returns the status of the response.
    /// </summary>
    /// <param name="model">the model to solve</param>
    /// <returns>the status of the solve</returns>
    public SolveStatus Solve(Model model)
    {
        if (logCallback_ == null)
        {
            helper_.ClearLogCallback();
        }
        else
        {
            helper_.SetLogCallbackFromDirectorClass(logCallback_);
        }
        helper_.Solve(model.Helper);
        if (!helper_.HasResponse())
        {
            return SolveStatus.UNKNOWN_STATUS;
        }
        return helper_.Status();
    }

    /// <summary>
    /// Enables or disables the underlying solver output.
    /// </summary>
    /// <param name="enable">the Boolean that controls the output</param>
    public void EnableOutput(bool enable)
    {
        helper_.EnableOutput(enable);
    }

    /** Sets the time limit for the solve in seconds. */
    public void SetTimeLimitInSeconds(double limit)
    {
        helper_.SetTimeLimitInSeconds(limit);
    }

    /** Sets solver specific parameters as string. */
    public void SetSolverSpecificParameters(String parameters)
    {
        helper_.SetSolverSpecificParameters(parameters);
    }

    /// <summary>
    /// Returns whether solver specified during the ctor was found and correctly installed.
    /// </summary>
    /// <returns>whether the solver is supported or not</returns>
    public bool SolverIsSupported()
    {
        return helper_.SolverIsSupported();
    }

    /// <summary>
    /// Tries to interrupt the solve. Returns true if the feature is supported.
    /// </summary>
    /// <returns>whether the solver supports interruption</returns>
    public bool InterruptSolve()
    {
        return helper_.InterruptSolve();
    }

    /// <summary>
    /// Returns true if solve() was called, and a response was returned.
    /// </summary>
    /// <returns>whether solve did happen</returns>
    public bool HasResponse()
    {
        return helper_.HasResponse();
    }

    /// <summary>
    /// Returns true if solve() was called, and a solution was returned.
    /// </summary>
    /// <returns>whether a solution was found</returns>
    public bool HasSolution()
    {
        return helper_.HasSolution();
    }

    /// <summary>
    /// The best objective value found during search. This raises a SolverException if no solution has been found,
    /// or if Solve() has not been called.
    /// </summary>
    public double ObjectiveValue
    {
        get {
            if (!helper_.HasSolution())
            {
                throw new SolverException("Solver.ObjectiveValue", "Solve() was not called or no solution was found");
            }
            return helper_.ObjectiveValue();
        }
    }

    /// <summary>
    /// The best objective bound found during search. This raises a SolverException if no solution has been found,
    /// or if Solve() has not been called.
    /// </summary>
    public double BestObjectiveBound
    {
        get {
            if (!helper_.HasSolution())
            {
                throw new SolverException("Solver.BestObjectiveBound",
                                          "Solve() was not called or no solution was found");
            }
            return helper_.BestObjectiveBound();
        }
    }

    /// <summary>
    /// The value of a variable in the current solution. This raises a SolverException if no solution has been
    /// found, or if Solve() has not been called.
    /// </summary>
    public double Value(Variable var)
    {
        if (!helper_.HasSolution())
        {
            throw new SolverException("Solver.Value())", "Solve() was not called or no solution was found");
        }
        return helper_.VariableValue(var.Index);
    }
    /// <summary>
    /// The reduced cost of a variable in the current solution. This raises a SolverException if no solution has
    /// been found, or if Solve() has not been called.
    /// </summary>
    public double ReducedCost(Variable var)
    {
        if (!helper_.HasSolution())
        {
            throw new SolverException("Solver.ReducedCost())", "Solve() was not called or no solution was found");
        }
        return helper_.ReducedCost(var.Index);
    }

    /// <summary>
    /// The dual value of a linear constraint in the current solution. This raises a SolverException if no solution
    /// has been found, or if Solve() has not been called.
    /// </summary>
    public double DualValue(LinearConstraint ct)
    {
        if (!helper_.HasSolution())
        {
            throw new SolverException("Solver.DualValue())", "Solve() was not called or no solution was found");
        }
        return helper_.DualValue(ct.Index);
    }

    /// <summary>
    /// The activity of a constraint in the current solution. This raises a SolverException if no solution has been
    /// found, or if Solve() has not been called.
    /// </summary>
    public double Activity(LinearConstraint ct)
    {
        if (!helper_.HasSolution())
        {
            throw new SolverException("Solver.Activity())", "Solve() was not called or no solution was found");
        }
        return helper_.Activity(ct.Index);
    }

    /// <summary>
    /// Sets the log callback for the solver.
    /// </summary>
    public MbLogCallback LogCallback
    {
        get {
            return logCallback_;
        }
        set {
            logCallback_ = value;
        }
    }

    /// <summary>
    /// Returns the elapsed time since the creation of the solver.
    /// </summary>
    public double WallTime
    {
        get {
            return helper_.WallTime();
        }
    }

    /// <summary>
    /// Returns the user time since the creation of the solver.
    /// </summary>
    public double UserTime
    {
        get {
            return helper_.UserTime();
        }
    }

    private ModelSolverHelper helper_;
    private MbLogCallback logCallback_;
}

} // namespace Google.OrTools.ModelBuilder
