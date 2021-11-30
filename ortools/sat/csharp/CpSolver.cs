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

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Google.OrTools.Sat
{
    public class CpSolver
    {
        public CpSolverStatus Solve(CpModel model, SolutionCallback cb = null)
        {
            // Setup search.
            CreateSolveWrapper();
            if (string_parameters_ != null)
            {
                solve_wrapper_.SetStringParameters(string_parameters_);
            }
            if (log_callback_ != null)
            {
                solve_wrapper_.AddLogCallbackFromClass(log_callback_);
            }
            if (cb != null)
            {
                solve_wrapper_.AddSolutionCallback(cb);
            }

            response_ = solve_wrapper_.Solve(model.Model);

            // Cleanup search.
            if (cb != null)
            {
                solve_wrapper_.ClearSolutionCallback(cb);
            }
            ReleaseSolveWrapper();

            return response_.Status;
        }

        [ObsoleteAttribute("This method is obsolete. Call Solve instead.", false)]
        public CpSolverStatus SolveWithSolutionCallback(CpModel model, SolutionCallback cb)
        {
            return Solve(model, cb);
        }

        public CpSolverStatus SearchAllSolutions(CpModel model, SolutionCallback cb)
        {
            string old_parameters = string_parameters_;
            string_parameters_ += " enumerate_all_solutions:true";
            Solve(model, cb);
            string_parameters_ = old_parameters;
            return response_.Status;
        }

        [MethodImpl(MethodImplOptions.Synchronized)]
        public void StopSearch()
        {
            if (solve_wrapper_ != null)
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

        public String ResponseStats()
        {
            return CpSatHelper.SolverResponseStats(response_);
        }

        public double ObjectiveValue
        {
            get {
                return response_.ObjectiveValue;
            }
        }

        public double BestObjectiveBound
        {
            get {
                return response_.BestObjectiveBound;
            }
        }

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

        public CpSolverResponse Response
        {
            get {
                return response_;
            }
        }

        public long Value(LinearExpr e)
        {
            List<LinearExpr> exprs = new List<LinearExpr>();
            List<long> coeffs = new List<long>();
            exprs.Add(e);
            coeffs.Add(1L);
            long constant = 0;

            while (exprs.Count > 0)
            {
                LinearExpr expr = exprs[0];
                exprs.RemoveAt(0);
                long coeff = coeffs[0];
                coeffs.RemoveAt(0);
                if (coeff == 0)
                    continue;

                if (expr is ProductCst)
                {
                    ProductCst p = (ProductCst)expr;
                    if (p.Coeff != 0)
                    {
                        exprs.Add(p.Expr);
                        coeffs.Add(p.Coeff * coeff);
                    }
                }
                else if (expr is SumArray)
                {
                    SumArray a = (SumArray)expr;
                    constant += coeff * a.Offset;
                    foreach (LinearExpr sub in a.Expressions)
                    {
                        exprs.Add(sub);
                        coeffs.Add(coeff);
                    }
                }
                else if (expr is IntVar)
                {
                    int index = expr.Index;
                    long value = index >= 0 ? response_.Solution[index] : -response_.Solution[-index - 1];
                    constant += coeff * value;
                }
                else if (expr is NotBooleanVariable)
                {
                    throw new ArgumentException("Cannot evaluate a literal in an integer expression.");
                }
                else
                {
                    throw new ArgumentException("Cannot evaluate '" + expr.ToString() + "' in an integer expression");
                }
            }
            return constant;
        }

        public Boolean BooleanValue(ILiteral literal)
        {
            if (literal is IntVar || literal is NotBooleanVariable)
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

        public long NumBranches()
        {
            return response_.NumBranches;
        }

        public long NumConflicts()
        {
            return response_.NumConflicts;
        }

        public double WallTime()
        {
            return response_.WallTime;
        }

        public IList<int> SufficientAssumptionsForInfeasibility()
        {
            return response_.SufficientAssumptionsForInfeasibility;
        }

        private CpSolverResponse response_;
        private LogCallback log_callback_;
        private string string_parameters_;
        private SolveWrapper solve_wrapper_;
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

} // namespace Google.OrTools.Sat
