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

/** Model solver class */
public class ModelSolver
{
    class ModelSolverException : Exception
    {
        public ModelSolverException(String methodName, String msg) : base(methodName + ": " + msg)
        {
        }
    }

    /** Creates the solver with the supplied solver backend. */
    public ModelSolver(String solverName)
    {
        this.helper_ = new ModelSolverHelper(solverName);
        this.logCallback_ = null;
    }

    /** Solves given model, and returns the status of the response. */
    public SolveStatus solve(ModelBuilder model)
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

    /** Enables or disables the underlying solver output. */
    public void EnableOutput(bool enable)
    {
        helper_.EnableOutput(enable);
    }

    /** Sets the time limit for the solve in seconds. */
    // public void SetTimeLimit(Duration limit)
    // {
    //     helper_.SetTimeLimitInSeconds((double)limit.toMillis() / 1000.0);
    // }

    /** Sets solver specific parameters as string. */
    public void SetSolverSpecificParameters(String parameters)
    {
        helper_.SetSolverSpecificParameters(parameters);
    }

    /** Returns whether solver specified during the ctor was found and correctly installed. */
    public bool SolverIsSupported()
    {
        return helper_.SolverIsSupported();
    }

    /** Tries to interrupt the solve. Returns true if the feature is supported. */
    public bool InterruptSolve()
    {
        return helper_.InterruptSolve();
    }

    /** Returns true if solve() was called, and a response was returned. */
    public bool HasResponse()
    {
        return helper_.HasResponse();
    }

    /** Returns true if solve() was called, and a solution was returned. */
    public bool HasSolution()
    {
        return helper_.HasSolution();
    }

    /** Checks that the solver has found a solution, and returns the objective value. */
    public double ObjectiveValue
    {
        get {
            if (!helper_.HasSolution())
            {
                throw new ModelSolverException("ModelSolver.getObjectiveValue()",
                                               "solve() was not called or no solution was found");
            }
            return helper_.ObjectiveValue();
        }
    }

    /** Checks that the solver has found a solution, and returns the objective value. */
    public double BestObjectiveBound
    {
        get {
            if (!helper_.HasSolution())
            {
                throw new ModelSolverException("ModelSolver.getBestObjectiveBound()",
                                               "solve() was not called or no solution was found");
            }
            return helper_.BestObjectiveBound();
        }
    }

    /** Checks that the solver has found a solution, and returns the value of the given variable. */
    public double Value(Variable var)
    {
        if (!helper_.HasSolution())
        {
            throw new ModelSolverException("ModelSolver.getValue())",
                                           "solve() was not called or no solution was found");
        }
        return helper_.VariableValue(var.Index);
    }
    /**
     * Checks that the solver has found a solution, and returns the reduced cost of the given
     * variable.
     */
    public double ReducedCost(Variable var)
    {
        if (!helper_.HasSolution())
        {
            throw new ModelSolverException("ModelSolver.getReducedCost())",
                                           "solve() was not called or no solution was found");
        }
        return helper_.ReducedCost(var.Index);
    }

    /**
     * Checks that the solver has found a solution, and returns the dual value of the given
     * constraint.
     */
    public double DualValue(LinearConstraint ct)
    {
        if (!helper_.HasSolution())
        {
            throw new ModelSolverException("ModelSolver.getDualValue())",
                                           "solve() was not called or no solution was found");
        }
        return helper_.DualValue(ct.Index);
    }

    /**
     * Checks that the solver has found a solution, and returns the activity of the given constraint.
     */
    public double Activity(LinearConstraint ct)
    {
        if (!helper_.HasSolution())
        {
            throw new ModelSolverException("ModelSolver.getActivity())",
                                           "solve() was not called or no solution was found");
        }
        return helper_.Activity(ct.Index);
    }

    /** Sets the log callback for the solver. */
    public LogCallback LogCallback
    {
        get {
            return logCallback_;
        }
        set {
            logCallback_ = value;
        }
    }

    /** Returns the elapsed time since the creation of the solver. */
    public double WallTime
    {
        get {
            return helper_.WallTime();
        }
    }

    /** Returns the user time since the creation of the solver. */
    public double UserTime
    {
        get {
            return helper_.UserTime();
        }
    }

    private ModelSolverHelper helper_;
    private LogCallback logCallback_;
}

} // namespace Google.OrTools.ModelBuilder
