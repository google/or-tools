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
public class CpSolver : IDisposable
{
    private LogCallback? _log_callback;
    private BestBoundCallback? _best_bound_callback;
    private SolveWrapper? _solve_wrapper;
    private Queue<Term> _terms;
    private bool _disposed = false;

    public double ObjectiveValue => Response!.ObjectiveValue;

    public double BestObjectiveBound => Response!.BestObjectiveBound;

    public string? StringParameters { get; set; }

    public CpSolverResponse? Response { get; private set; }

    public CpSolverStatus Solve(CpModel model, SolutionCallback? cb = null)
    {
        // Setup search.
        CreateSolveWrapper();
        if (StringParameters is not null)
        {
            _solve_wrapper!.SetStringParameters(StringParameters);
        }

        if (_log_callback is not null)
        {
            _solve_wrapper!.AddLogCallbackFromClass(_log_callback);
        }

        if (_best_bound_callback is not null)
        {
            _solve_wrapper!.AddBestBoundCallbackFromClass(_best_bound_callback);
        }

        if (cb is not null)
        {
            _solve_wrapper!.AddSolutionCallback(cb);
        }

        Response = _solve_wrapper!.Solve(model.Model);

        // Cleanup search.
        if (cb is not null)
        {
            _solve_wrapper.ClearSolutionCallback(cb);
        }

        ReleaseSolveWrapper();

        return Response.Status;
    }

    /** <summary>Stops the search asynchronously.</summary> */
    [MethodImpl(MethodImplOptions.Synchronized)]
    public void StopSearch() => _solve_wrapper?.StopSearch();

    public string ResponseStats() => CpSatHelper.SolverResponseStats(Response);

    public void SetLogCallback(StringToVoidDelegate del)
    {
        _log_callback?.Dispose();
        _log_callback = new LogCallbackDelegate(del);
    }

    public void ClearLogCallback()
    {
        _log_callback?.Dispose();
        _log_callback = null;
    }

    public void SetBestBoundCallback(DoubleToVoidDelegate del)
    {
        _best_bound_callback?.Dispose();
        _best_bound_callback = new BestBoundCallbackDelegate(del);
    }

    public void ClearBestBoundCallback()
    {
        _best_bound_callback?.Dispose();
        _best_bound_callback = null;
    }

    public long Value(IntVar intVar)
    {
        var index = intVar.GetIndex();
        var value = index >= 0 ? Response!.Solution[index] : -Response!.Solution[-index - 1];
        return value;
    }

    public long Value(LinearExpr e)
    {
        long constant = 0;
        long coefficient = 1;
        var expr = e;
        if (_terms is null)
        {
            _terms = new Queue<Term>();
        }
        else
        {
            _terms.Clear();
        }

        do
        {
            switch (expr)
            {
            case LinearExprBuilder a:
                constant += coefficient * a.Offset;
                if (coefficient == 1)
                {
                    foreach (var sub in a.Terms)
                    {
                        _terms.Enqueue(sub);
                    }
                }
                else
                {
                    foreach (var sub in a.Terms)
                    {
                        _terms.Enqueue(new Term(sub.expr, sub.coefficient * coefficient));
                    }
                }

                break;
            case IntVar intVar:
                var index = intVar.GetIndex();
                var value = index >= 0 ? Response!.Solution[index] : -Response!.Solution[-index - 1];
                constant += coefficient * value;
                break;
            case NotBoolVar:
                throw new ArgumentException("Cannot evaluate a literal in an integer expression.");
            default:
                throw new ArgumentException("Cannot evaluate '" + expr + "' in an integer expression");
            }

            if (!_terms.TryDequeue(out var term))
            {
                break;
            }

            expr = term.expr;
            coefficient = term.coefficient;
        } while (true);

        return constant;
    }

    public bool BooleanValue(ILiteral literal)
    {
        if (literal is BoolVar || literal is NotBoolVar)
        {
            var index = literal.GetIndex();
            if (index >= 0)
            {
                return Response!.Solution[index] != 0;
            }
            else
            {
                return Response!.Solution[-index - 1] == 0;
            }
        }
        else
        {
            throw new ArgumentException("Cannot evaluate '" + literal.ToString() + "' as a boolean literal");
        }
    }

    public long NumBranches() => Response!.NumBranches;

    public long NumConflicts() => Response!.NumConflicts;

    public double WallTime() => Response!.WallTime;

    public string SolveLog() => Response!.SolveLog;

    public IList<int> SufficientAssumptionsForInfeasibility() => Response!.SufficientAssumptionsForInfeasibility;

    public string SolutionInfo() => Response!.SolutionInfo;

    /// <summary>
    /// Releases unmanaged resources and optionally releases managed resources.
    /// </summary>
    /// <param name="disposing">true to release both managed and unmanaged resources; false to release only unmanaged resources.</param>
    protected virtual void Dispose(bool disposing)
    {
        if (_disposed)
        {
            return;
        }

        if (disposing)
        {
            _best_bound_callback?.Dispose();
            _log_callback?.Dispose();
            ReleaseSolveWrapper();
        }

        _disposed = true;
    }

    /// <summary>
    /// Releases all resources used by the CpSolver.
    /// </summary>
    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
    }

    [MethodImpl(MethodImplOptions.Synchronized)]
    private void CreateSolveWrapper()
    {
        _solve_wrapper?.Dispose();
        _solve_wrapper = new SolveWrapper();
    }

    [MethodImpl(MethodImplOptions.Synchronized)]
    private void ReleaseSolveWrapper()
    {
        _solve_wrapper?.Dispose();
        _solve_wrapper = null;
    }
}

class LogCallbackDelegate : LogCallback
{
    private readonly StringToVoidDelegate _delegate_;

    public LogCallbackDelegate(StringToVoidDelegate del) => _delegate_ = del;

    public override void NewMessage(string message) => _delegate_(message);
}

class BestBoundCallbackDelegate : BestBoundCallback
{
    private readonly DoubleToVoidDelegate _delegate;

    public BestBoundCallbackDelegate(DoubleToVoidDelegate del) => _delegate = del;

    public override void NewBestBound(double bound) => _delegate(bound);
}

} // namespace Google.OrTools.Sat