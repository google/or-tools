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

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Google.OrTools.Sat
{
/**
 * <summary>
 * Wrapper around the SAT solver.
 * </summary>
 *
 * <remarks>This class proposes a <c>Solve()</c> method, as well as accessors to get the values of
 * variables in the best solution, as well as general statistics of the search.
 * </remarks>
 */
public class CpSolver
{
    /** <summary>Solves the given model, and returns the solve status.</summary> */
    public CpSolverStatus Solve(CpModel model, SolutionCallback cb = null)
    {
        // Setup search.
        CreateSolveWrapper();
        if (string_parameters_ is not null)
        {
            solve_wrapper_.SetStringParameters(string_parameters_);
        }
        if (log_callback_ is not null)
        {
            solve_wrapper_.AddLogCallbackFromClass(log_callback_);
        }
        if (best_bound_callback_ is not null)
        {
            solve_wrapper_.AddBestBoundCallbackFromClass(best_bound_callback_);
        }
        if (cb is not null)
        {
            solve_wrapper_.AddSolutionCallback(cb);
        }

        response_ = solve_wrapper_.Solve(model.Model);

        // Cleanup search.
        if (cb is not null)
        {
            solve_wrapper_.ClearSolutionCallback(cb);
        }
        ReleaseSolveWrapper();

        return response_.Status;
    }

    /** <summary> Deprecated, use Solve() instead. </summary> */
    [ObsoleteAttribute("This method is obsolete. Call Solve instead.", false)]
    public CpSolverStatus SolveWithSolutionCallback(CpModel model, SolutionCallback cb)
    {
        return Solve(model, cb);
    }

    /** <summary> Deprecated, use Solve() instead. </summary> */
    [ObsoleteAttribute("This method is obsolete. Call Solve instead with the enumerate_all_solutions parameter.",
                       false)]
    public CpSolverStatus SearchAllSolutions(CpModel model, SolutionCallback cb)
    {
        string old_parameters = string_parameters_;
        string_parameters_ += " enumerate_all_solutions:true";
        Solve(model, cb);
        string_parameters_ = old_parameters;
        return response_.Status;
    }

    /** <summary>Stops the search asynchronously.</summary> */
    [MethodImpl(MethodImplOptions.Synchronized)]
    public void StopSearch()
    {
        if (solve_wrapper_ is not null)
        {
            solve_wrapper_.StopSearch();
        }
    }

    [MethodImpl(MethodImplOptions.Synchronized)]
    private void CreateSolveWrapper()
    {
        solve_wrapper_ = new SolveWrapper();
    }

    [MethodImpl(MethodImplOptions.Synchronized)]
    private void ReleaseSolveWrapper()
    {
        solve_wrapper_ = null;
    }

    /** <summary>Statistics on the solution found as a string.</summary>*/
    public String ResponseStats()
    {
        return CpSatHelper.SolverResponseStats(response_);
    }

    /** <summary>The best objective value found during search.</summary>*/
    public double ObjectiveValue
    {
        get {
            return response_.ObjectiveValue;
        }
    }

    /**
     * <summary>
     * The best lower bound found when minimizing, of the best upper bound found when maximizing
     * </summary>
     */
    public double BestObjectiveBound
    {
        get {
            return response_.BestObjectiveBound;
        }
    }

    /** The parameters of the search, stored as a string */
    public string StringParameters
    {
        get {
            return string_parameters_;
        }
        set {
            string_parameters_ = value;
        }
    }

    public void SetLogCallback(StringToVoidDelegate del)
    {
        log_callback_ = new LogCallbackDelegate(del);
    }

    public void ClearLogCallback()
    {
        log_callback_ = null;
    }

    public void SetBestBoundCallback(DoubleToVoidDelegate del)
    {
        best_bound_callback_ = new BestBoundCallbackDelegate(del);
    }

    public void ClearBestBoundCallback()
    {
        best_bound_callback_ = null;
    }

    public CpSolverResponse Response
    {
        get {
            return response_;
        }
    }

    /**
     * <summary>
     * Returns the value of an integer variable in the last solution found.
     * </summary>
     */
    public long Value(IntVar intVar)
    {
        int index = intVar.GetIndex();
        long value = index >= 0 ? response_.Solution[index] : -response_.Solution[-index - 1];
        return value;
    }

    /**
     * <summary>
     * Returns the value of a linear expression in the last solution found.
     * </summary>
     */
    public long Value(LinearExpr e)
    {
        long constant = 0;
        long coefficient = 1;
        LinearExpr expr = e;
        if (terms_ is null)
        {
            terms_ = new Queue<Term>();
        }
        else
        {
            terms_.Clear();
        }

        do
        {
            switch (expr)
            {
            case LinearExprBuilder a:
                constant += coefficient * a.Offset;
                if (coefficient == 1)
                {
                    foreach (Term sub in a.Terms)
                    {
                        terms_.Enqueue(sub);
                    }
                }
                else
                {
                    foreach (Term sub in a.Terms)
                    {
                        terms_.Enqueue(new Term(sub.expr, sub.coefficient * coefficient));
                    }
                }
                break;
            case IntVar intVar:
                int index = intVar.GetIndex();
                long value = index >= 0 ? response_.Solution[index] : -response_.Solution[-index - 1];
                constant += coefficient * value;
                break;
            case NotBoolVar:
                throw new ArgumentException("Cannot evaluate a literal in an integer expression.");
            default:
                throw new ArgumentException("Cannot evaluate '" + expr + "' in an integer expression");
            }

            if (!terms_.TryDequeue(out var term))
            {
                break;
            }
            expr = term.expr;
            coefficient = term.coefficient;
        } while (true);

        return constant;
    }

    /**
     * <summary>
     * Returns the Boolean value of a literal in the last solution found.
     * </summary>
     */
    public Boolean BooleanValue(ILiteral literal)
    {
        if (literal is BoolVar || literal is NotBoolVar)
        {
            int index = literal.GetIndex();
            if (index >= 0)
            {
                return response_.Solution[index] != 0;
            }
            else
            {
                return response_.Solution[-index - 1] == 0;
            }
        }
        else
        {
            throw new ArgumentException("Cannot evaluate '" + literal.ToString() + "' as a boolean literal");
        }
    }

    /** <summary>Returns the number of branches explored during search. </summary>*/
    public long NumBranches()
    {
        return response_.NumBranches;
    }

    /** <summary>Returns the number of conflicts created during search. </summary>*/
    public long NumConflicts()
    {
        return response_.NumConflicts;
    }

    /** <summary>Returns the wall time of the search. </summary>*/
    public double WallTime()
    {
        return response_.WallTime;
    }

    public IList<int> SufficientAssumptionsForInfeasibility()
    {
        return response_.SufficientAssumptionsForInfeasibility;
    }

    /**
     * <summary>
     * Returns some information on how the solution was found, or the reason why the model or the
     * parameters are invalid.
     * </summary>
     */
    public String SolutionInfo()
    {
        return response_.SolutionInfo;
    }

    private CpSolverResponse response_;
    private LogCallback log_callback_;
    private BestBoundCallback best_bound_callback_;
    private string string_parameters_;
    private SolveWrapper solve_wrapper_;
    private Queue<Term> terms_;
}

class LogCallbackDelegate : LogCallback
{
    public LogCallbackDelegate(StringToVoidDelegate del)
    {
        this.delegate_ = del;
    }

    public override void NewMessage(string message)
    {
        delegate_(message);
    }

    private StringToVoidDelegate delegate_;
}

class BestBoundCallbackDelegate : BestBoundCallback
{
    public BestBoundCallbackDelegate(DoubleToVoidDelegate del)
    {
        this.delegate_ = del;
    }

    public override void NewBestBound(double bound)
    {
        delegate_(bound);
    }

    private DoubleToVoidDelegate delegate_;
}

} // namespace Google.OrTools.Sat
